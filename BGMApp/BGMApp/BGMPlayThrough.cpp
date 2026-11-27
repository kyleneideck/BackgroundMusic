// This file is part of Background Music.
//
// Background Music is free software: you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation, either version 2 of the
// License, or (at your option) any later version.
//
// Background Music is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Background Music. If not, see <http://www.gnu.org/licenses/>.

//
//  BGMPlayThrough.cpp
//  BGMApp
//
//  Copyright © 2016, 2017, 2020, 2026 Kyle Neideck
//

// Self Include
#include "BGMPlayThrough.h"

// Local Includes
#include "BGM_Types.h"
#include "BGM_Utils.h"

// PublicUtility Includes
#include "CAAutoDisposer.h"
#include "CAHALAudioSystemObject.h"
#include "CAPropertyAddress.h"

// STL Includes
#include <algorithm>  // For std::max
#include <cmath>      // For fabs

// System Includes
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/task.h>


// The number of IO cycles (roughly) to wait for our IOProcs to stop themselves before assuming something
// went wrong. If that happens, we try to stop them from a non-IO thread and continue anyway. 
static const UInt32 kStopIOProcTimeoutInIOCycles = 600;

#pragma mark Construction/Destruction

BGMPlayThrough::BGMPlayThrough(BGMAudioDevice inInputDevice, BGMAudioDevice inOutputDevice)
{
    Init(inInputDevice, { inOutputDevice });
}

BGMPlayThrough::BGMPlayThrough(BGMAudioDevice inInputDevice,
                               const std::vector<BGMAudioDevice>& inOutputDevices)
{
    Init(inInputDevice, inOutputDevices);
}

BGMPlayThrough::~BGMPlayThrough()
{
    CAMutex::Locker stateLocker(mStateMutex);

    BGMLogAndSwallowExceptionsMsg("BGMPlayThrough::~BGMPlayThrough", "Deactivate", [&]() {
        Deactivate();
    });

    // If one of the IOProcs failed to stop, CoreAudio could (at least in theory) still call it
    // after this point. This isn't a solution, but calling DeallocateBuffer instead of letting it
    // deallocate itself should at least make the error less likely to cause a segfault, since
    // DeallocateBuffer takes the buffer locks and sets mBuffer to null.
    //
    // TODO: It probably wouldn't be too hard to fix this properly by giving the IOProcs weak refs
    //       to the BGMPlayThrough object instead of raw pointers.
    DeallocateBuffer();
    
    if(mOutputDeviceIOProcSemaphore != SEMAPHORE_NULL)
    {
        kern_return_t theError = semaphore_destroy(mach_task_self(), mOutputDeviceIOProcSemaphore);
        BGM_Utils::LogIfMachError("BGMPlayThrough::~BGMPlayThrough", "semaphore_destroy", theError);
    }
}

void    BGMPlayThrough::Init(BGMAudioDevice inInputDevice, const std::vector<BGMAudioDevice>& inOutputDevices)
{
    BGMAssert(mInputDeviceIOProcState.is_lock_free(),
              "BGMPlayThrough::BGMPlayThrough: !mInputDeviceIOProcState.is_lock_free()");
    BGMAssert(!mActive, "BGMPlayThrough::BGMPlayThrough: Can't init while active.");
    
    ThrowIf(inOutputDevices.empty(),
            CAException(kAudioHardwareIllegalOperationError),
            "BGMPlayThrough::BGMPlayThrough: No output devices");

    mInputDevice = inInputDevice;
    
    // Replace the output devices, preserving the gain of any that are already present.
    std::deque<std::unique_ptr<OutputDevice>> newOutputDevices;
    for(const auto& inOutputDevice : inOutputDevices)
    {
        std::unique_ptr<OutputDevice> newOutputDevice(new OutputDevice(inOutputDevice));
        for(const auto& oldOutputDevice : mOutputDevices)
        {
            if(oldOutputDevice->device.GetObjectID() == inOutputDevice.GetObjectID())
            {
                newOutputDevice->gain.store(oldOutputDevice->gain.load());
                break;
            }
        }
        newOutputDevices.push_back(std::move(newOutputDevice));
    }
    mOutputDevices = std::move(newOutputDevices);

    for(const auto& outputDevice : mOutputDevices)
    {
        BGMAssert(outputDevice->state.is_lock_free(),
                  "BGMPlayThrough::BGMPlayThrough: !OutputDevice.state.is_lock_free()");
    }
    
    AllocateBuffer();
    
    try
    {
        // Init the semaphore for the output IOProc.
        if(mOutputDeviceIOProcSemaphore == SEMAPHORE_NULL)
        {
            kern_return_t theError = semaphore_create(mach_task_self(), &mOutputDeviceIOProcSemaphore, SYNC_POLICY_FIFO, 0);
            BGM_Utils::ThrowIfMachError("BGMPlayThrough::BGMPlayThrough", "semaphore_create", theError);
            
            ThrowIf(mOutputDeviceIOProcSemaphore == SEMAPHORE_NULL,
                    CAException(kAudioHardwareUnspecifiedError),
                    "BGMPlayThrough::BGMPlayThrough: Could not create semaphore");
        }
    }
    catch (...)
    {
        // Clean up.
        DeallocateBuffer();
        throw;
    }
}

void    BGMPlayThrough::Activate()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    if(!mActive)
    {
        DebugMsg("BGMPlayThrough::Activate: Activating playthrough");
        
        // The output devices don't need to share a sample rate: a device whose rate differs from
        // the input device's is resampled in its IOProc (see the header comment). We decide which
        // devices need that below, after the input rate is set.
        
        CreateIOProcIDs();
        
        mActive = true;
        
        // TODO: This code (the next two blocks) should be in BGMDeviceControlSync.
        
        // Set BGMDevice's sample rate to match the first output device.
        try
        {
            Float64 outputSampleRate = mOutputDevices.front()->device.GetNominalSampleRate();
            mInputDevice.SetNominalSampleRate(outputSampleRate);
        }
        catch (const CAException& e)
        {
            LogWarning("BGMPlayThrough::Activate: Failed to sync device sample rates. Error: %d",
                       e.GetError());
        }
        
        // Set BGMDevice's IO buffer size to match the first output device.
        try
        {
            UInt32 outputBufferSize = mOutputDevices.front()->device.GetIOBufferSize();
            mInputDevice.SetIOBufferSize(outputBufferSize);
        }
        catch (const CAException& e)
        {
            LogWarning("BGMPlayThrough::Activate: Failed to sync device buffer sizes. Error: %d",
                       e.GetError());
        }
        
        // Mark which output devices need resampling (their rate differs from the rate the audio is
        // stored at, which is the input device's) and create their AudioConverters. If creating a
        // resampler fails we throw, since playing the audio at the wrong speed would be worse.
        const Float64 storedRate = mBufferFormat.mSampleRate;
        for(auto& outputDevice : mOutputDevices)
        {
            outputDevice->needsResampling =
                    (fabs(outputDevice->device.GetNominalSampleRate() - storedRate) > 0.5);
        }
        
        CreateResamplers();
        
        if(!mInputDevice.IsBGMDeviceInstance())
        {
            LogWarning("BGMPlayThrough::Activate: Playthrough activated with an input device other "
                       "than BGMDevice. This hasn't been tested and is almost definitely a bug.");
        }
        BGMAssert(mInputDevice.IsBGMDeviceInstance(),
                  "BGMPlayThrough::Activate: !mInputDevice.IsBGMDeviceInstance()");
    }
}

void    BGMPlayThrough::Deactivate()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    if(mActive)
    {
        DebugMsg("BGMPlayThrough::Deactivate: Deactivating playthrough");
        
        BGMLogAndSwallowExceptions("BGMPlayThrough::Deactivate", [&] {
            Stop();
        });
        
        BGMLogAndSwallowExceptions("BGMPlayThrough::Deactivate", [&] {
            DestroyIOProcIDs();
        });
        
        // The IOProcs are stopped now, so it's safe to destroy the resamplers.
        BGMLogAndSwallowExceptions("BGMPlayThrough::Deactivate", [&] {
            DestroyResamplers();
        });
        
        mActive = false;
    }
}

void    BGMPlayThrough::AllocateBuffer()
{
    // Allocate the ring buffer that will hold the data passing between the devices
    UInt32 numberStreams = 1;
    AudioStreamBasicDescription outputFormat[1];
    mOutputDevices.front()->device.GetCurrentVirtualFormats(false, numberStreams, outputFormat);
    
    if(numberStreams < 1)
    {
        Throw(CAException(kAudioHardwareUnsupportedOperationError));
    }
    
    // Remember the format for the resamplers (it's the source format they convert from).
    mBufferFormat = outputFormat[0];
    
    // Need to lock the buffer mutexes to make sure the IOProcs aren't accessing it. The order is
    // important here. We always lock them in the same order to prevent deadlocks.
    CAMutex::Locker lockerInput(mBufferInputMutex);
    CAMutex::Locker lockerOutput(mBufferOutputMutex);

    mBuffer = std::unique_ptr<CARingBuffer>(new CARingBuffer);

    // The calculation for the size of the buffer is from Apple's CAPlayThrough.cpp sample code
    //
    // TODO: Test playthrough with hardware with more than 2 channels per frame, a sample (virtual) format other than
    //       32-bit floats and/or an IO buffer size other than 512 frames
    const UInt32 capacityFrames = mOutputDevices.front()->device.GetIOBufferSize() * 20;
    mBuffer->Allocate(outputFormat[0].mChannelsPerFrame,
                      outputFormat[0].mBytesPerFrame,
                      capacityFrames);
    mBufferCapacityFrames = capacityFrames;
}

void    BGMPlayThrough::DeallocateBuffer()
{
    // Need to lock the buffer mutexes to make sure the IOProcs aren't accessing it. The order is
    // important here. We always lock them in the same order to prevent deadlocks.
    CAMutex::Locker lockerInput(mBufferInputMutex);
    CAMutex::Locker lockerOutput(mBufferOutputMutex);
    mBuffer = nullptr;  // Note that the buffer's destructor will deallocate it.
}

// static
void    BGMPlayThrough::DisableIOProcStreams(BGMAudioDevice& inDevice,
                                             AudioDeviceIOProcID inIOProcID,
                                             bool inIsInput,
                                             const char* inDeviceName)
{
    BGMLogAndSwallowExceptions("BGMPlayThrough::DisableIOProcStreams", [&] {
        UInt32 numStreams = inDevice.GetNumberStreams(inIsInput);

        if(numStreams == 0)
        {
            // Nothing to do.
            return;
        }

        // Turn every stream in this direction off for our IOProc. The HAL then passes our IOProc a
        // NULL buffer for each of them (which it ignores).
        CAAutoArrayDelete<bool> streamUsage(numStreams);

        for(UInt32 i = 0; i < numStreams; i++)
        {
            streamUsage[i] = false;
        }

        inDevice.SetIOProcStreamUsage(inIOProcID, inIsInput, streamUsage);

        DebugMsg("BGMPlayThrough::DisableIOProcStreams: Disabled %u %s stream(s) on the %s device",
                 numStreams, (inIsInput ? "input" : "output"), inDeviceName);
    });
}

void    BGMPlayThrough::CreateIOProcIDs()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    BGMAssert(!mPlayingThrough,
              "BGMPlayThrough::CreateIOProcIDs: Tried to create IOProcs when playthrough was already running");
    BGMAssert(mInputDeviceIOProcID == nullptr,
              "BGMPlayThrough::CreateIOProcIDs: mInputDeviceIOProcID must be destroyed first.");
    for(const auto& outputDevice : mOutputDevices)
    {
        BGMAssert(outputDevice->ioProcID == nullptr,
                  "BGMPlayThrough::CreateIOProcIDs: An output device IOProc ID must be destroyed first.");
    }
    BGMAssert(CheckIOProcsAreStopped(),
              "BGMPlayThrough::CreateIOProcIDs: IOProcs not ready.");
    
    const bool inDeviceAlive = mInputDevice.IsAlive();
    bool allOutDevicesAlive = !mOutputDevices.empty();
    for(const auto& outputDevice : mOutputDevices)
    {
        allOutDevicesAlive = allOutDevicesAlive && outputDevice->device.IsAlive();
    }
    
    if(inDeviceAlive && allOutDevicesAlive)
    {
        DebugMsg("BGMPlayThrough::CreateIOProcIDs: Creating IOProcs for %u output device(s)",
                 static_cast<unsigned int>(mOutputDevices.size()));
    
        try
        {
            mInputDeviceIOProcID = mInputDevice.CreateIOProcID(&BGMPlayThrough::InputDeviceIOProc, this);
        }
        catch(const CAException& e)
        {
            LogWarning("BGMPlayThrough::CreateIOProcIDs: Failed to create input IOProc ID. mInputDevice = %d",
                       mInputDevice.GetObjectID());
            throw;
        }
        
        for(auto& outputDevice : mOutputDevices)
        {
            try
            {
                outputDevice->ioProcID =
                        outputDevice->device.CreateIOProcID(&BGMPlayThrough::OutputDeviceIOProc, this);
            }
            catch(const CAException& e)
            {
                LogWarning("BGMPlayThrough::CreateIOProcIDs: Failed to create output IOProc ID. "
                           "mOutputDevice = %d",
                           outputDevice->device.GetObjectID());
                DestroyIOProcIDs(); // Clean up.
                throw;
            }
        }

        if(mInputDeviceIOProcID == nullptr)
        {
            // Should never happen if CAHALAudioDevice::CreateIOProcID didn't throw.
            LogError("BGMPlayThrough::CreateIOProcIDs: Null IOProc ID returned by CreateIOProcID");
            throw CAException(kAudioHardwareIllegalOperationError);
        }
        for(const auto& outputDevice : mOutputDevices)
        {
            if(outputDevice->ioProcID == nullptr)
            {
                // Should never happen if CAHALAudioDevice::CreateIOProcID didn't throw.
                LogError("BGMPlayThrough::CreateIOProcIDs: Null IOProc ID returned by CreateIOProcID");
                throw CAException(kAudioHardwareIllegalOperationError);
            }
        }

        // Tell the HAL which streams our IOProcs actually use, so it won't engage streams we don't
        // use.
        // 
        // OutputDeviceIOProc only writes audio, so disable the output devices' input streams. With
        // a headset, for example, that avoids opening the mic input, which we think might help
        // avoid issues with Bluetooth headsets. See #859.
        for(auto& outputDevice : mOutputDevices)
        {
            DisableIOProcStreams(outputDevice->device, outputDevice->ioProcID, /* inIsInput = */ true, "output");
        }

        // InputDeviceIOProc only captures, so disable BGMDevice's output streams.
        DisableIOProcStreams(mInputDevice, mInputDeviceIOProcID, /* inIsInput = */ false, "input");
        
        // TODO: Try using SetIOCycleUsage to reduce latency? Our IOProcs don't really do anything except copy a small
        //       buffer. According to this, Jack OS X considered it:
        //       https://lists.apple.com/archives/coreaudio-api/2008/Mar/msg00043.html but from a quick look at their
        //       code, I don't think they ended up using it.
        // mInputDevice->SetIOCycleUsage(0.01f);
        // for(auto& outputDevice : mOutputDevices) { outputDevice->device.SetIOCycleUsage(0.01f); }
    }
    else
    {
        LogWarning("BGMPlayThrough::CreateIOProcIDs: Failed to create IOProcs.%s%s",
                   (inDeviceAlive ? "" : " Input device not alive."),
                   (allOutDevicesAlive ? "" : " An output device not alive."));
        throw CAException(kAudioHardwareIllegalOperationError);
    }
}

void    BGMPlayThrough::DestroyIOProcIDs()
{
    CAMutex::Locker stateLocker(mStateMutex);

    // In release builds, we still try to destroy the IDs if the IOProcs are running, hoping they just haven't been
    // stopped quite yet. The docs for AudioDeviceDestroyIOProcID don't say not to do that, but it could cause races
    // if one really is still running so it isn't ideal.
    BGMAssert(CheckIOProcsAreStopped(), "BGMPlayThrough::DestroyIOProcIDs: IOProcs not ready.");

    DebugMsg("BGMPlayThrough::DestroyIOProcIDs: Destroying IOProcs");

    auto destroy = [](BGMAudioDevice& device, const char* deviceName, AudioDeviceIOProcID& ioProcID) {
#if !DEBUG
    #pragma unused (deviceName)
#endif
        if(ioProcID != nullptr)
        {
            try
            {
                device.DestroyIOProcID(ioProcID);
            }
            catch(const CAException& e)
            {
                if((e.GetError() == kAudioHardwareBadDeviceError) || (e.GetError() == kAudioHardwareBadObjectError))
                {
                    // This means the IOProc IDs will have already been destroyed, so there's nothing to do.
                    DebugMsg("BGMPlayThrough::DestroyIOProcIDs: Didn't destroy IOProc ID for %s device because "
                             "it's not connected anymore. deviceID = %d",
                             deviceName,
                             device.GetObjectID());
                }
                else
                {
                    ioProcID = nullptr;
                    throw;
                }
            }
            
            ioProcID = nullptr;
        }
    };
    
    destroy(mInputDevice, "input", mInputDeviceIOProcID);
    for(auto& outputDevice : mOutputDevices)
    {
        destroy(outputDevice->device, "output", outputDevice->ioProcID);
    }
}

void    BGMPlayThrough::CreateResamplers()
{
    CAMutex::Locker stateLocker(mStateMutex);

    for(auto& outputDevice : mOutputDevices)
    {
        if(!outputDevice->needsResampling)
        {
            continue;
        }

        // The output device's format is the resampler's destination format (at the device's own
        // sample rate). The ring buffer's format (mBufferFormat) is the source format.
        UInt32 numberStreams = 1;
        AudioStreamBasicDescription destFormat[1];
        BGMLogAndSwallowExceptionsMsg("BGMPlayThrough::CreateResamplers", "Get the device format",
                                      [&] {
            outputDevice->device.GetCurrentVirtualFormats(false, numberStreams, destFormat);
        });

        if(numberStreams < 1)
        {
            LogError("BGMPlayThrough::CreateResamplers: Device %d has no output format", 
                     outputDevice->device.GetObjectID());
            Throw(CAException(kAudioHardwareUnsupportedOperationError));
        }

        AudioConverterRef converter = nullptr;
        OSStatus err = AudioConverterNew(&mBufferFormat, &destFormat[0], &converter);
        if(err != noErr)
        {
            LogWarning("BGMPlayThrough::CreateResamplers: AudioConverterNew failed for device %d: %d",
                       outputDevice->device.GetObjectID(), err);
            Throw(CAException(err));
        }

        // Use kConverterPrimeMethod_None so the resampler doesn't ask for extra leading/trailing
        // input frames, which we can't supply in real time. This adds a small amount of latency at
        // the start of playback (the "through latency"), which is acceptable here.
        const UInt32 primeMethod = kConverterPrimeMethod_None;
        err = AudioConverterSetProperty(converter, kAudioConverterPrimeMethod,
                                        sizeof(primeMethod), &primeMethod);
        if(err != noErr)
        {
            AudioConverterDispose(converter);
            LogWarning("BGMPlayThrough::CreateResamplers: Failed to set the prime method for "
                       "device %d: %d",
                       outputDevice->device.GetObjectID(), err);
            Throw(CAException(err));
        }

        // Allocate the input buffer the resampler's input callback reads the ring buffer into. It
        // must hold the most input the resampler is likely to ask for in one call (roughly
        // outputFrames * inputRate / outputRate), with a margin.
        {
            const Float64 inputRate = mBufferFormat.mSampleRate;
            const Float64 outputRate = destFormat[0].mSampleRate;
            const UInt32 ioBufferFrames = outputDevice->device.GetIOBufferSize();
            const UInt32 maxInputFrames =
                    static_cast<UInt32>(ioBufferFrames * (inputRate / outputRate)) + 1024;
            outputDevice->resamplerInputBufferSize = maxInputFrames * mBufferFormat.mBytesPerFrame;
            outputDevice->resamplerInputBuffer = static_cast<Byte*>(
                    calloc(1, outputDevice->resamplerInputBufferSize));
            if(outputDevice->resamplerInputBuffer == nullptr)
            {
                AudioConverterDispose(converter);
                outputDevice->resamplerInputBuffer = nullptr;
                outputDevice->resamplerInputBufferSize = 0;
                LogError("BGMPlayThrough::CreateResamplers: Failed to allocate the input buffer "
                         "for device %d",
                         outputDevice->device.GetObjectID());
                Throw(CAException(kAudioHardwareUnspecifiedError));
            }
        }

        // Pre-warm the resampler so any one-time internal setup (e.g. the sample-rate-conversion
        // filter state) happens now, in a non-real-time context, rather than on the first real-time
        // call. We feed it a little silence (from the pre-allocated buffer) and then reset it, so
        // the real-time IO starts fresh.
        {
            const UInt32 preWarmOutputFrames = 256;
            AudioBufferList preWarmOut;
            preWarmOut.mNumberBuffers = 1;
            preWarmOut.mBuffers[0].mNumberChannels = destFormat[0].mChannelsPerFrame;
            preWarmOut.mBuffers[0].mDataByteSize =
                    preWarmOutputFrames * destFormat[0].mBytesPerFrame;
            preWarmOut.mBuffers[0].mData =
                    static_cast<Byte*>(calloc(1, preWarmOut.mBuffers[0].mDataByteSize));

            ResamplerCallbackState preWarmState;
            preWarmState.buffer = nullptr;   // Provide silence, not the ring buffer.
            preWarmState.record = nullptr;
            preWarmState.inputBytesPerFrame = mBufferFormat.mBytesPerFrame;
            preWarmState.inputBuffer = outputDevice->resamplerInputBuffer;
            preWarmState.inputBufferSize = outputDevice->resamplerInputBufferSize;

            UInt32 preWarmPackets = preWarmOutputFrames;
            AudioConverterFillComplexBuffer(converter, &ResamplerInputProc, &preWarmState,
                                            &preWarmPackets, &preWarmOut, nullptr);

            free(preWarmOut.mBuffers[0].mData);
        }
        AudioConverterReset(converter);

        outputDevice->converter = converter;
        outputDevice->resamplerInputSampleTime = -1;

        DebugMsg("BGMPlayThrough::CreateResamplers: Created a resampler for device %d (%.0f Hz "
                 "-> %.0f Hz)",
                 outputDevice->device.GetObjectID(),
                 mBufferFormat.mSampleRate,
                 destFormat[0].mSampleRate);
    }
}

void    BGMPlayThrough::DestroyResamplers()
{
    CAMutex::Locker stateLocker(mStateMutex);

    for(auto& outputDevice : mOutputDevices)
    {
        if(outputDevice->converter != nullptr)
        {
            AudioConverterDispose(static_cast<AudioConverterRef>(outputDevice->converter));
            outputDevice->converter = nullptr;
        }

        if(outputDevice->resamplerInputBuffer != nullptr)
        {
            free(outputDevice->resamplerInputBuffer);
            outputDevice->resamplerInputBuffer = nullptr;
        }
        outputDevice->resamplerInputBufferSize = 0;
        outputDevice->resamplerInputSampleTime = -1;
        outputDevice->needsResampling = false;
    }
}

// static
OSStatus    BGMPlayThrough::ResamplerInputProc(AudioConverterRef                   inAudioConverter,
                                                UInt32*                             ioNumberDataPackets,
                                                AudioBufferList*                    ioData,
                                                AudioStreamPacketDescription* __nullable* __nullable outDataPacketDescription,
                                                void* __nullable                    inUserData)
{
    #pragma unused (inAudioConverter, outDataPacketDescription)

    ResamplerCallbackState* const state = static_cast<ResamplerCallbackState*>(inUserData);

    UInt32 frames = *ioNumberDataPackets;
    if(frames == 0)
    {
        return noErr;
    }

    if((state->inputBuffer == nullptr) || (state->inputBufferSize == 0))
    {
        // No input buffer provided. (Shouldn't happen.) Tell the converter we have no data.
        *ioNumberDataPackets = 0;
        return kAudioConverterErr_InvalidInputSize;
    }

    // Cap the number of frames to what fits in the pre-allocated buffer. (We sized it generously,
    // so this shouldn't trigger in practice.)
    const UInt32 maxFrames = state->inputBufferSize / state->inputBytesPerFrame;
    if(frames > maxFrames)
    {
        frames = maxFrames;
        if(frames == 0)
        {
            *ioNumberDataPackets = 0;
            return noErr;
        }
    }

    const UInt32 totalBytes = frames * state->inputBytesPerFrame;

    ioData->mNumberBuffers = 1;
    ioData->mBuffers[0].mData = state->inputBuffer;
    ioData->mBuffers[0].mDataByteSize = totalBytes;

    if(state->buffer == nullptr)
    {
        // Provide silence. (Used to pre-warm a newly-created resampler.)
        memset(ioData->mBuffers[0].mData, 0, totalBytes);
        *ioNumberDataPackets = frames;
        return noErr;
    }

    // Read from the ring buffer into the pre-allocated buffer.
    OutputDevice* const record = state->record;

    CARingBufferError err =
            state->buffer->Fetch(ioData, frames, record->resamplerInputSampleTime);
    // Advance the read position by the number of frames we used (the full requested amount, even
    // if Fetch zero-filled part of it because it wasn't in the buffer yet).
    record->resamplerInputSampleTime += frames;

    if(err != kCARingBufferError_OK)
    {
        // Fetch failed (e.g. CPU overload). Provide silence so the converter can continue.
        memset(ioData->mBuffers[0].mData, 0, totalBytes);
        *ioNumberDataPackets = 0;
        return kAudioConverterErr_InvalidInputSize;
    }

    // Fetch sets mDataByteSize to the in-range bytes, which may be less than totalBytes if part
    // of the requested range wasn't in the buffer. Restore it so the converter sees the full
    // number of frames (Fetch already zero-filled the missing part).
    ioData->mBuffers[0].mDataByteSize = totalBytes;
    *ioNumberDataPackets = frames;

    return noErr;
}

bool    BGMPlayThrough::CheckIOProcsAreStopped() const noexcept
{
    bool statesOK = true;
    
    if(mInputDeviceIOProcState != IOState::Stopped)
    {
        LogWarning("BGMPlayThrough::CheckIOProcsAreStopped: Input IOProc not stopped. mInputDeviceIOProcState = %d",
                   mInputDeviceIOProcState.load());
        statesOK = false;
    }
    
    for(const auto& outputDevice : mOutputDevices)
    {
        if(outputDevice->state != IOState::Stopped)
        {
            LogWarning("BGMPlayThrough::CheckIOProcsAreStopped: Output IOProc not stopped. deviceID = %d, "
                       "state = %d",
                       outputDevice->device.GetObjectID(),
                       outputDevice->state.load());
            statesOK = false;
        }
    }
    
    return statesOK;
}

void    BGMPlayThrough::SetDevices(const BGMAudioDevice* __nullable inInputDevice,
                                   const BGMAudioDevice* __nullable inOutputDevice)
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    bool wasActive = mActive;
    bool wasPlayingThrough = mPlayingThrough;
    
    if(wasPlayingThrough)
    {
        BGMAssert(wasActive, "BGMPlayThrough::SetOutputDevice: wasPlayingThrough && !wasActive");  // Sanity check.
    }
    
    Deactivate();
    
    mInputDevice = inInputDevice ? *inInputDevice : mInputDevice;
    
    if(inOutputDevice)
    {
        // Replace the output devices (or add the single one).
        std::vector<BGMAudioDevice> outputDevices;
        for(const auto& outputDevice : mOutputDevices)
        {
            if(outputDevice->device.GetObjectID() != inOutputDevice->GetObjectID())
            {
                outputDevices.push_back(outputDevice->device);
            }
        }
        outputDevices.push_back(*inOutputDevice);
        Init(mInputDevice, outputDevices);
    }
    else
    {
        // Only the input device changed, so keep the existing output devices.
        std::vector<BGMAudioDevice> outputDevices;
        for(const auto& outputDevice : mOutputDevices)
        {
            outputDevices.push_back(outputDevice->device);
        }
        Init(mInputDevice, outputDevices);
    }
    
    if(wasActive)
    {
        Activate();
    }
    
    if(wasPlayingThrough)
    {
        Start();
    }
}

void    BGMPlayThrough::SetDevices(BGMAudioDevice inInputDevice,
                                   const std::vector<BGMAudioDevice>& inOutputDevices)
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    bool wasActive = mActive;
    bool wasPlayingThrough = mPlayingThrough;
    
    if(wasPlayingThrough)
    {
        BGMAssert(wasActive, "BGMPlayThrough::SetDevices: wasPlayingThrough && !wasActive");  // Sanity check.
    }
    
    Deactivate();
    
    Init(inInputDevice, inOutputDevices);
    
    if(wasActive)
    {
        Activate();
    }
    
    if(wasPlayingThrough)
    {
        Start();
    }
}

void    BGMPlayThrough::SetOutputDevices(const std::vector<BGMAudioDevice>& inOutputDevices)
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    ThrowIf(inOutputDevices.empty(),
            CAException(kAudioHardwareIllegalOperationError),
            "BGMPlayThrough::SetOutputDevices: No output devices");

    bool wasActive = mActive;
    bool wasPlayingThrough = mPlayingThrough;
    
    if(wasPlayingThrough)
    {
        BGMAssert(wasActive, "BGMPlayThrough::SetOutputDevices: wasPlayingThrough && !wasActive");  // Sanity check.
    }
    
    Deactivate();
    
    Init(mInputDevice, inOutputDevices);
    
    if(wasActive)
    {
        Activate();
    }
    
    if(wasPlayingThrough)
    {
        Start();
    }
}

void    BGMPlayThrough::SetOutputDeviceGain(AudioObjectID inDeviceID, Float32 inGain)
{
    CAMutex::Locker stateLocker(mStateMutex);

    for(auto& outputDevice : mOutputDevices)
    {
        if(outputDevice->device.GetObjectID() == inDeviceID)
        {
            outputDevice->gain.store(inGain);
            return;
        }
    }
}

#pragma mark Control Playthrough

void    BGMPlayThrough::Start()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    if(mPlayingThrough)
    {
        DebugMsg("BGMPlayThrough::Start: Already started/starting.");

        for(const auto& outputDevice : mOutputDevices)
        {
            if(outputDevice->state == IOState::Running)
            {
                ReleaseThreadsWaitingForOutputToStart();
            }
        }

        return;
    }
    
    if(!mInputDevice.IsAlive())
    {
        LogError("BGMPlayThrough::Start: !mInputDevice");
        
        ReleaseThreadsWaitingForOutputToStart();
        
        throw CAException(kAudioHardwareBadDeviceError);
    }
    
    for(const auto& outputDevice : mOutputDevices)
    {
        if(!outputDevice->device.IsAlive())
        {
            LogError("BGMPlayThrough::Start: Output device not alive. deviceID = %d",
                     outputDevice->device.GetObjectID());
            
            ReleaseThreadsWaitingForOutputToStart();
            
            throw CAException(kAudioHardwareBadDeviceError);
        }
    }
    
    // Set up IOProcs and listeners if they haven't been already.
    Activate();
    
    BGMAssert(mInputDeviceIOProcID != nullptr, "BGMPlayThrough::Start: Null IOProc ID");
    for(const auto& outputDevice : mOutputDevices)
    {
        BGMAssert(outputDevice->ioProcID != nullptr, "BGMPlayThrough::Start: Null IOProc ID");
    }
    
    if((mInputDeviceIOProcState != IOState::Stopped) ||
       std::any_of(mOutputDevices.begin(), mOutputDevices.end(),
                   [](const std::unique_ptr<OutputDevice>& inOutputDevice)
                   { return inOutputDevice->state != IOState::Stopped; }))
    {
        LogWarning("BGMPlayThrough::Start: IOProc(s) not ready. Trying to start anyway.");
    }
    
    DebugMsg("BGMPlayThrough::Start: Starting playthrough");
    
    mToldOutputDeviceToStartAt = mach_absolute_time();
    
    // Start our IOProcs.
    try
    {
        mInputDeviceIOProcState = IOState::Starting;
        mInputDevice.StartIOProc(mInputDeviceIOProcID);
    
        for(auto& outputDevice : mOutputDevices)
        {
            outputDevice->state = IOState::Starting;
            outputDevice->device.StartIOProc(outputDevice->ioProcID);
        }
    }
    catch(const CAException& e)
    {
        ReleaseThreadsWaitingForOutputToStart();
        
        // Log an error message.
        OSStatus err = e.GetError();
        char err4CC[5] = CA4CCToCString(err);
        LogError("BGMPlayThrough::Start: Failed to start IOProc. Error: %d (%s)",
                 err, err4CC);
        
        // Try to stop the IOProcs in case StartIOProc failed because one of our IOProc was already
        // running. I don't know if it actually does fail in that case, but the documentation
        // doesn't say so it's safer to assume it could.
        CATry
        mInputDevice.StopIOProc(mInputDeviceIOProcID);
        CACatch
        for(auto& outputDevice : mOutputDevices)
        {
            CATry
            outputDevice->device.StopIOProc(outputDevice->ioProcID);
            CACatch
        }
        
        mInputDeviceIOProcState = IOState::Stopped;
        for(auto& outputDevice : mOutputDevices)
        {
            outputDevice->state = IOState::Stopped;
        }
        
        throw;
    }
    
    mPlayingThrough = true;
}

OSStatus    BGMPlayThrough::WaitForOutputDeviceToStart() noexcept
{
    // Check for errors.
    //
    // Technically we should take the state mutex here, but that could cause deadlocks because
    // BGM_Device::StartIO (in BGMDriver) blocks on this function (via XPC). Other BGMPlayThrough
    // functions make requests to BGMDriver while holding the state mutex, usually to get/set
    // properties, but the HAL will block those requests until BGM_Device::StartIO returns.
    try
    {
        if(!mActive)
        {
            LogError("BGMPlayThrough::WaitForOutputDeviceToStart: !mActive");
            return kAudioHardwareNotRunningError;
        }
        
        if(mOutputDevices.empty() || !mOutputDevices.front()->device.IsAlive())
        {
            LogError("BGMPlayThrough::WaitForOutputDeviceToStart: Output device not alive");
            return kAudioHardwareBadDeviceError;
        }
    }
    catch(const CAException& e)
    {
        BGMLogException(e);
        return e.GetError();
    }
    
    const IOState initialState = mOutputDevices.front()->state;
    const UInt64 startedAt = mach_absolute_time();

    if(initialState == IOState::Running)
    {
        // Return early because the output device is already running.
        return kAudioHardwareNoError;
    }
    else if(initialState != IOState::Starting)
    {
        // Warn if we haven't been told to start the output device yet. Usually means we
        // haven't received a kAudioDevicePropertyDeviceIsRunningSomewhere notification yet, which can
        // happen. It's most common when the user changes the output device while IO is
        // running.
        LogWarning("BGMPlayThrough::WaitForOutputDeviceToStart: Device not starting");
        
        return kDeviceNotStarting;
    }

    // Wait for one of our output IOProcs to start. mOutputDeviceIOProcSemaphore is reset to 0
    // (semaphore_signal_all) when an IOProc is running on an output device.
    //
    // This does mean that we won't have any data the first time our IOProc is called, but I
    // don't know any way to wait until just before that point. (The device's IsRunning property
    // changes immediately after we call StartIOProc.)
    //
    // We check the output devices' states every 200ms as a fault tolerance mechanism. (Though,
    // I'm not completely sure it's impossible to miss the signal from the IOProc because of a
    // spurious wake up, so it might actually be necessary.)
    DebugMsg("BGMPlayThrough::WaitForOutputDeviceToStart: Waiting.");
    
    kern_return_t theError;
    bool anyRunning = false;
    bool anyStarting = false;
    UInt64 waitedNsec = 0;
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    
    do
    {
        BGMAssert(mOutputDeviceIOProcSemaphore != SEMAPHORE_NULL,
                  "BGMPlayThrough::WaitForOutputDeviceToStart: !mOutputDeviceIOProcSemaphore");
        
        theError = semaphore_timedwait(mOutputDeviceIOProcSemaphore,
                                       (mach_timespec_t){ 0, 200 * NSEC_PER_MSEC });
        
        // Update the total time we've been waiting and the output devices' states.
        waitedNsec = (mach_absolute_time() - startedAt) * info.numer / info.denom;
        anyRunning = false;
        anyStarting = false;
        for(const auto& outputDevice : mOutputDevices)
        {
            if(outputDevice->state == IOState::Running)
            {
                anyRunning = true;
            }
            else if(outputDevice->state == IOState::Starting)
            {
                anyStarting = true;
            }
        }
    }
    while((theError != KERN_SUCCESS) &&         // Signalled from an IOProc.
          anyStarting &&                        // At least one IOProc is still starting.
          (waitedNsec < kStartIOTimeoutNsec));  // Timed out.

    if(BGMDebugLoggingIsEnabled())
    {
        UInt64 startedBy = mach_absolute_time();

        struct mach_timebase_info baseInfo = { 0, 0 };
        mach_timebase_info(&baseInfo);
        Float64 base = static_cast<Float64>(baseInfo.numer) / baseInfo.denom;

        DebugMsg("BGMPlayThrough::WaitForOutputDeviceToStart: Started %f ms after notification, %f "
                 "ms after entering WaitForOutputDeviceToStart.",
                 static_cast<Float64>(startedBy - mToldOutputDeviceToStartAt) * base / NSEC_PER_MSEC,
                 static_cast<Float64>(startedBy - startedAt) * base / NSEC_PER_MSEC);
    }

    // Figure out which error code to return.
    switch (theError)
    {
        case KERN_SUCCESS:              // Signalled from an IOProc.
            return kAudioHardwareNoError;
            
                                        // IO state changed or we timed out after
        case KERN_OPERATION_TIMED_OUT:  //  - semaphore_timedwait timed out, or
        case KERN_ABORTED:              //  - a spurious wake-up.
            return anyRunning ? kAudioHardwareNoError : kAudioHardwareNotRunningError;
            
        default:
            BGM_Utils::LogIfMachError("BGMPlayThrough::WaitForOutputDeviceToStart",
                                      "semaphore_timedwait",
                                      theError);
            return kAudioHardwareUnspecifiedError;
    }
}

// Release any threads waiting for the output device to start. This function doesn't take mStateMutex
// because it gets called on the IO thread, which is realtime priority.
void    BGMPlayThrough::ReleaseThreadsWaitingForOutputToStart()
{
    if(mActive)
    {
        semaphore_t semaphore = mOutputDeviceIOProcSemaphore;
        
        if(semaphore != SEMAPHORE_NULL)
        {
            mRTLogger.LogReleasingWaitingThreads();

            kern_return_t theError = semaphore_signal_all(semaphore);
            mRTLogger.LogIfMachError_ReleaseWaitingThreadsSignal(theError);
        }
    }
}

OSStatus    BGMPlayThrough::Stop()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    // TODO: Tell the waiting threads what happened so they can return an error?
    ReleaseThreadsWaitingForOutputToStart();
    
    if(mActive && mPlayingThrough)
    {
        DebugMsg("BGMPlayThrough::Stop: Stopping playthrough");
        
        bool inputDeviceAlive = false;
        
        CATry
        inputDeviceAlive = CAHALAudioObject::ObjectExists(mInputDevice) && mInputDevice.IsAlive();
        CACatch
        
        mInputDeviceIOProcState = inputDeviceAlive ? IOState::Stopping : IOState::Stopped;
        
        bool anyOutputDeviceAlive = false;
        for(auto& outputDevice : mOutputDevices)
        {
            bool deviceAlive = false;
            CATry
            deviceAlive =
                CAHALAudioObject::ObjectExists(outputDevice->device) && outputDevice->device.IsAlive();
            CACatch
            
            outputDevice->state = deviceAlive ? IOState::Stopping : IOState::Stopped;
            anyOutputDeviceAlive = anyOutputDeviceAlive || deviceAlive;
        }
        
        // Wait for the IOProcs to stop themselves. This is so the IOProcs don't get called after the BGMPlayThrough instance
        // (pointed to by the client data they get from the HAL) is deallocated.
        //
        // From Jeff Moore on the Core Audio mailing list:
        //     Note that there is no guarantee about how many times your IOProc might get called after AudioDeviceStop() returns
        //     when you make the call from outside of your IOProc. However, if you call AudioDeviceStop() from inside your IOProc,
        //     you do get the guarantee that your IOProc will not get called again after the IOProc has returned.
        UInt64 totalWaitNs = 0;
        BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&]() {
            Float64 expectedInputCycleNs = 0;

            if(inputDeviceAlive)
            {
                expectedInputCycleNs =
                    mInputDevice.GetIOBufferSize() * (1 / mInputDevice.GetNominalSampleRate()) *
                            NSEC_PER_SEC;
            }

            Float64 expectedMaxOutputCycleNs = 0;
            for(const auto& outputDevice : mOutputDevices)
            {
                if(outputDevice->device.IsAlive())
                {
                    expectedMaxOutputCycleNs = std::max(
                        expectedMaxOutputCycleNs,
                        outputDevice->device.GetIOBufferSize() *
                                (1 / outputDevice->device.GetNominalSampleRate()) * NSEC_PER_SEC);
                }
            }

            UInt64 expectedMaxCycleNs =
                static_cast<UInt64>(std::max(expectedInputCycleNs, expectedMaxOutputCycleNs));

            auto anyIOProcStopping = [&] {
                if(mInputDeviceIOProcState == IOState::Stopping)
                {
                    return true;
                }
                for(const auto& outputDevice : mOutputDevices)
                {
                    if(outputDevice->state == IOState::Stopping)
                    {
                        return true;
                    }
                }
                return false;
            };

            while(anyIOProcStopping() &&
                  (totalWaitNs < kStopIOProcTimeoutInIOCycles * expectedMaxCycleNs))
            {
                // TODO: If playthrough is started again while we're waiting in this loop we could drop frames. Wait on a
                //       semaphore instead of sleeping? That way Start() could also signal it, before waiting on the state mutex,
                //       as a way of cancelling the stop operation.
                struct timespec rmtp;
                int err = nanosleep((const struct timespec[]){{0, NSEC_PER_MSEC}}, &rmtp);
                totalWaitNs += NSEC_PER_MSEC - (err == -1 ? rmtp.tv_nsec : 0);
            }
        });
        
        // Clean up if the IOProcs didn't stop themselves
        if(mInputDeviceIOProcState == IOState::Stopping && mInputDeviceIOProcID != nullptr)
        {
            LogError("BGMPlayThrough::Stop: The input IOProc didn't stop itself in time. Stopping "
                     "it from outside of the IO thread.");
            
            BGMLogUnexpectedExceptions("BGMPlayThrough::Stop", [&]() {
                mInputDevice.StopIOProc(mInputDeviceIOProcID);
            });

            mInputDeviceIOProcState = IOState::Stopped;
        }
        
        for(auto& outputDevice : mOutputDevices)
        {
            if(outputDevice->state == IOState::Stopping && outputDevice->ioProcID != nullptr)
            {
                LogError("BGMPlayThrough::Stop: The output IOProc for device %d didn't stop itself "
                         "in time. Stopping it from outside of the IO thread.",
                         outputDevice->device.GetObjectID());
                
                BGMLogUnexpectedExceptions("BGMPlayThrough::Stop", [&]() {
                    outputDevice->device.StopIOProc(outputDevice->ioProcID);
                });

                outputDevice->state = IOState::Stopped;
            }
        }
        
        mPlayingThrough = false;
    }
    
    mFirstInputSampleTime = -1;
    mLastInputSampleTime = -1;
    for(auto& outputDevice : mOutputDevices)
    {
        outputDevice->lastOutputSampleTime = -1;
        outputDevice->inToOutSampleOffset = 0.0;
        outputDevice->resamplerInputSampleTime = -1;
    }
    
    return noErr; // TODO: Why does this return anything and why always noErr?
}

void    BGMPlayThrough::StopIfIdle()
{
    // To save CPU time, we stop playthrough when no clients are doing IO. This should reduce the coreaudiod and BGMApp
    // processes' idle CPU use to virtually none. If this isn't working for you, a client might be running IO without
    // being audible. VLC does that when you have a file paused, for example.
    
    CAMutex::Locker stateLocker(mStateMutex);
    
    BGMAssert(mInputDevice.IsBGMDeviceInstance(),
              "BGMDevice not set as input device. StopIfIdle can't tell if other devices are idle.");
    
    if(!mPlayingThrough || IsRunningSomewhereOtherThanBGMApp(mInputDevice))
    {
        return;
    }

    DebugMsg("BGMPlayThrough::StopIfIdle: BGMDevice is only running IO for BGMApp. "
             "Stopping playthrough.");
    Stop();
}

#pragma mark Helpers

// static
bool    BGMPlayThrough::IsRunningSomewhereOtherThanBGMApp(const BGMAudioDevice& inBGMDevice)
{
    CFTypeRef type = inBGMDevice.GetPropertyData_CFType(kBGMRunningSomewhereOtherThanBGMAppAddress);

    if(!type)
    {
        return false;
    }

    bool isRunning = false;

    if(CFGetTypeID(type) == CFBooleanGetTypeID())
    {
        isRunning = CFBooleanGetValue(static_cast<CFBooleanRef>(type));
    }
    else
    {
        LogWarning("BGMPlayThrough::IsRunningSomewhereOtherThanBGMApp: Unexpected property type");
    }

    CFRelease(type);

    return isRunning;
}

#pragma mark IOProcs

// Note that the IOProcs will very likely not run on the same thread and that they intentionally
// only lock mutexes around their use of mBuffer.

// static
OSStatus    BGMPlayThrough::InputDeviceIOProc(AudioObjectID           inDevice,
                                              const AudioTimeStamp*   inNow,
                                              const AudioBufferList*  inInputData,
                                              const AudioTimeStamp*   inInputTime,
                                              AudioBufferList*        outOutputData,
                                              const AudioTimeStamp*   inOutputTime,
                                              void* __nullable        inClientData)
{
    #pragma unused (inDevice, inNow, outOutputData, inOutputTime)
    
    // refCon (reference context) is the instance that created the IOProc
    BGMPlayThrough* const refCon = static_cast<BGMPlayThrough*>(inClientData);
    
    IOState state;
    UpdateIOProcState("InputDeviceIOProc",
                      refCon->mRTLogger,
                      refCon->mInputDeviceIOProcState,
                      refCon->mInputDeviceIOProcID,
                      refCon->mInputDevice,
                      state);
    
    if(state == IOState::Stopped || state == IOState::Stopping)
    {
        // Return early, since we just asked to stop. (Or something really weird is going on.)
        return noErr;
    }
    
    BGMAssert(state == IOState::Running, "BGMPlayThrough::InputDeviceIOProc: Unexpected state");
    
    if(refCon->mFirstInputSampleTime == -1)
    {
        refCon->mFirstInputSampleTime = inInputTime->mSampleTime;
    }
    
    UInt32 framesToStore = inInputData->mBuffers[0].mDataByteSize / (SizeOf32(Float32) * 2);

    // See the comments in OutputDeviceIOProc where it locks mBufferOutputMutex.
    CAMutex::Tryer tryer(refCon->mBufferInputMutex);

    // Disable a warning about accessing mBuffer without holding both mBufferInputMutex and
    // mBufferOutputMutex. Explained further in OutputDeviceIOProc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety"
    if(tryer.HasLock() && refCon->mBuffer)
    {
        CARingBufferError err =
                refCon->mBuffer->Store(inInputData,
                                       framesToStore,
                                       static_cast<CARingBuffer::SampleTime>(
                                               inInputTime->mSampleTime));
#pragma clang diagnostic pop
        refCon->mRTLogger.LogIfRingBufferError_Store(err);

        refCon->mLastInputSampleTime = inInputTime->mSampleTime;
    }
    else
    {
        refCon->mRTLogger.LogRingBufferUnavailable("InputDeviceIOProc", tryer.HasLock());
    }

    return noErr;
}

// static
OSStatus    BGMPlayThrough::OutputDeviceIOProc(AudioObjectID           inDevice,
                                               const AudioTimeStamp*   inNow,
                                               const AudioBufferList*  inInputData,
                                               const AudioTimeStamp*  inInputTime,
                                               AudioBufferList*        outOutputData,
                                               const AudioTimeStamp*  inOutputTime,
                                               void* __nullable        inClientData)
{
    #pragma unused (inNow, inInputData, inInputTime)
    
    // refCon (reference context) is the instance that created the IOProc
    BGMPlayThrough* const refCon = static_cast<BGMPlayThrough*>(inClientData);
    
    // Find the record for this output device. The records are only changed while the IOProcs are
    // stopped, so this is safe without holding mStateMutex.
    BGMPlayThrough::OutputDevice* record = nullptr;
    for(auto& outputDevice : refCon->mOutputDevices)
    {
        if(outputDevice->device.GetObjectID() == inDevice)
        {
            record = outputDevice.get();
            break;
        }
    }
    
    if(record == nullptr)
    {
        // This shouldn't happen. The HAL only calls this IOProc for devices we created it for.
        refCon->mRTLogger.LogRingBufferUnavailable("OutputDeviceIOProc", false);
        FillWithSilence(outOutputData);
        return noErr;
    }
    
    IOState state;
    const bool didChangeState = UpdateIOProcState("OutputDeviceIOProc",
                                                  refCon->mRTLogger,
                                                  record->state,
                                                  record->ioProcID,
                                                  record->device,
                                                  state);
    
    if(state == IOState::Stopped || state == IOState::Stopping)
    {
        // Return early, since we just asked to stop. (Or something really weird is going on.)
        FillWithSilence(outOutputData);
        return noErr;
    }
    
    BGMAssert(state == IOState::Running, "BGMPlayThrough::OutputDeviceIOProc: Unexpected state");
    
    if(didChangeState)
    {
        // We just changed state from Starting to Running, which means this is the first time this IOProc
        // has been called since the output device finished starting up, so now we can wake any threads
        // waiting in WaitForOutputDeviceToStart.
        BGMAssert(record->lastOutputSampleTime == -1,
                  "BGMPlayThrough::OutputDeviceIOProc: lastOutputSampleTime not reset");
        
        refCon->ReleaseThreadsWaitingForOutputToStart();
    }
    
    if(refCon->mLastInputSampleTime == -1)
    {
        // Return early, since we don't have any data to output yet.
        FillWithSilence(outOutputData);
        return noErr;
    }
    
    // If this is the first time this IOProc has been called since starting playthrough...
    if(record->lastOutputSampleTime == -1)
    {
        // Calculate the number of frames between the read and write heads
        record->inToOutSampleOffset =
                inOutputTime->mSampleTime - refCon->mLastInputSampleTime;
        
        // Log if we dropped frames
        refCon->mRTLogger.LogIfDroppedFrames(refCon->mFirstInputSampleTime,
                                             refCon->mLastInputSampleTime);
    }
    
    CARingBuffer::SampleTime readHeadSampleTime =
        static_cast<CARingBuffer::SampleTime>(
            inOutputTime->mSampleTime - record->inToOutSampleOffset);
    CARingBuffer::SampleTime lastInputSampleTime =
        static_cast<CARingBuffer::SampleTime>(refCon->mLastInputSampleTime);
    
    UInt32 framesToOutput = outOutputData->mBuffers[0].mDataByteSize / (SizeOf32(Float32) * 2);

    // When the input and output devices are set, during start up or because the user changed the
    // output device, this class (re)allocates the ring buffer (mBuffer). We try to take this
    // lock before accessing the buffer to make sure it's allocated.
    //
    // If we don't get the lock, another thread must be allocating or deallocating it, so we just
    // give up. We can't avoid audio glitches while changing devices anyway. This class tries to
    // make sure the IOProcs aren't running when it allocates the buffer, but it can't guarantee
    // that.
    //
    // Note that this is only realtime safe because we only try to lock the mutex. If another
    // thread has the mutex, it will be a non-realtime thread, so we can't wait for it.
    CAMutex::Tryer tryer(refCon->mBufferOutputMutex);

    // Disable a warning about accessing mBuffer without holding both mBufferInputMutex and
    // mBufferOutputMutex. The input IOProc always writes ahead of where the output IOProc will read
    // in a given IO cycle, so it's safe for them to read and write at the same time. Multiple output
    // IOProcs can read at the same time because Fetch doesn't modify the buffer's state.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety"
    if(tryer.HasLock() && refCon->mBuffer)
    {
        if(record->needsResampling && record->converter != nullptr)
        {
            // Resample the audio to this device's sample rate using its AudioConverter. The
            // converter's input callback reads from the ring buffer (see ResamplerInputProc), so
            // this device consumes the input at the input rate and stays in sync with the other
            // devices.
            if(record->resamplerInputSampleTime == -1)
            {
                // Start reading at the current write head (which is slightly behind the very latest
                // data, so there's data to read).
                record->resamplerInputSampleTime =
                        static_cast<CARingBuffer::SampleTime>(refCon->mLastInputSampleTime);
            }

            // Drift correction (mirrors the fast path below): this device's hardware clock isn't
            // locked to the input device's, so the read head (in input sample time) drifts away
            // from the write head while they run. Bluetooth sinks in particular can drift fast
            // enough that, without correction, the read head either runs ahead of the available
            // data (the resampler would read silence forever) or falls so far behind that the
            // data it wants has been overwritten (the resampler would stall).
            //
            // Re-anchor the read head to the latest input when it drifts more than a threshold in
            // either direction. This costs a brief discontinuity, but keeps this device in sync
            // with the others.
            {
                const CARingBuffer::SampleTime writeHead = lastInputSampleTime;
                const CARingBuffer::SampleTime readHead = record->resamplerInputSampleTime;

                // Drift by more than a quarter of the ring buffer (~50 ms with the default buffer
                // size): small enough to keep this device roughly in sync with the others, large
                // enough that re-anchoring (and its click) is rare.
                const CARingBuffer::SampleTime capacityFrames =
                        refCon->mBufferCapacityFrames.load();
                const CARingBuffer::SampleTime resyncThreshold =
                        capacityFrames > 0 ? capacityFrames / 4 : 2048;

                bool needsReanchor =
                        (readHead - writeHead > resyncThreshold) ||   // Ahead of the input (fast clock).
                        (writeHead - readHead > resyncThreshold);     // Behind the input (slow clock).

                if(!needsReanchor)
                {
                    SInt64 bufferStartTime = 0, bufferEndTime = 0;
                    if(refCon->mBuffer->GetTimeBounds(bufferStartTime, bufferEndTime) ==
                            kCARingBufferError_OK)
                    {
                        // The data we're about to read has already been overwritten (we fell too
                        // far behind), or it doesn't exist yet (we ran ahead). Re-anchor either
                        // way.
                        needsReanchor = (readHead < bufferStartTime) || (readHead > bufferEndTime);
                    }
                }

                if(needsReanchor)
                {
                    refCon->mRTLogger.LogNoSamplesReady(lastInputSampleTime, readHead,
                                                        record->inToOutSampleOffset);

                    record->resamplerInputSampleTime = writeHead;

                    // Discard the resampler's buffered (pre-jump) input so it doesn't emit stale
                    // samples after the jump.
                    AudioConverterReset(static_cast<AudioConverterRef>(record->converter));
                }
            }

            ResamplerCallbackState resamplerState;
            resamplerState.buffer = refCon->mBuffer.get();
            resamplerState.record = record;
            resamplerState.inputBytesPerFrame = refCon->mBufferFormat.mBytesPerFrame;
            resamplerState.inputBuffer = record->resamplerInputBuffer;
            resamplerState.inputBufferSize = record->resamplerInputBufferSize;

            UInt32 outputPackets = framesToOutput;
            OSStatus convErr = AudioConverterFillComplexBuffer(
                    static_cast<AudioConverterRef>(record->converter),
                    &ResamplerInputProc,
                    &resamplerState,
                    &outputPackets,
                    outOutputData,
                    nullptr);

            if((convErr != noErr) || (outputPackets < framesToOutput))
            {
                // The converter didn't fill the whole output buffer (e.g. it temporarily ran out of
                // input). Silence the unfilled remainder.
                if(outputPackets < framesToOutput)
                {
                    const UInt32 outputBytesPerFrame =
                            framesToOutput > 0 ?
                                outOutputData->mBuffers[0].mDataByteSize / framesToOutput :
                                0;
                    const UInt32 filledBytes = outputPackets * outputBytesPerFrame;
                    memset(static_cast<Byte*>(outOutputData->mBuffers[0].mData) + filledBytes,
                           0,
                           outOutputData->mBuffers[0].mDataByteSize - filledBytes);
                }
                else
                {
                    FillWithSilence(outOutputData);
                }

                refCon->mRTLogger.LogNoSamplesReady(lastInputSampleTime,
                                                    record->resamplerInputSampleTime,
                                                    record->inToOutSampleOffset);
            }

            // Apply the master volume and this device's relative volume.
            const Float32 level = refCon->mMasterVolume.load() * record->gain.load();
            if(level < 1.0f)
            {
                ScaleAudioBuffer(outOutputData, level);
            }
        }
        else
        {
            // Same-rate fast path: the device's rate matches the input's, so the ring buffer's
            // samples are already at the right rate. Copy them directly.
            //
            // Very occasionally (at least for me) our read head gets ahead of input, i.e. we haven't
            // received any new input since this IOProc was last called, and we have to recalculate
            // its position. I figure this might be caused by clock drift but I'm really not sure. It
            // also happens if the input or output sample times are restarted from zero.
            //
            // We also recalculate the offset if the read head is outside of the ring buffer. This
            // happens for example when you plug in or unplug headphones, which causes the output
            // sample times to be restarted from zero.
            //
            // The vast majority of the time, just using lastInputSampleTime as the read head time
            // instead of the one we calculate would work fine (and would also account for the above).
            SInt64 bufferStartTime, bufferEndTime;
            CARingBufferError err =
                    refCon->mBuffer->GetTimeBounds(bufferStartTime, bufferEndTime);
            bool outOfBounds = false;

            if(err == kCARingBufferError_OK)
            {
                outOfBounds = (readHeadSampleTime < bufferStartTime)
                        || (readHeadSampleTime - framesToOutput > bufferEndTime);
            }

            if(lastInputSampleTime < readHeadSampleTime || outOfBounds)
            {
                refCon->mRTLogger.LogNoSamplesReady(lastInputSampleTime,
                                                    readHeadSampleTime,
                                                    record->inToOutSampleOffset);

                // Recalculate the in-to-out offset and read head.
                record->inToOutSampleOffset =
                        inOutputTime->mSampleTime - static_cast<Float64>(lastInputSampleTime);
                readHeadSampleTime = static_cast<CARingBuffer::SampleTime>(
                        inOutputTime->mSampleTime - record->inToOutSampleOffset);
            }

            // Copy the frames from the ring buffer.
            err = refCon->mBuffer->Fetch(outOutputData, framesToOutput, readHeadSampleTime);
            refCon->mRTLogger.LogIfRingBufferError_Fetch(err);

            if(err != kCARingBufferError_OK)
            {
                FillWithSilence(outOutputData);
            }
            else
            {
                // Apply the master volume and this device's relative volume.
                const Float32 level = refCon->mMasterVolume.load() * record->gain.load();
                if(level < 1.0f)
                {
                    ScaleAudioBuffer(outOutputData, level);
                }
            }
        }
    }
    else
    {
        refCon->mRTLogger.LogRingBufferUnavailable("OutputDeviceIOProc", tryer.HasLock());
        FillWithSilence(outOutputData);
    }
#pragma clang diagnostic pop

    record->lastOutputSampleTime = inOutputTime->mSampleTime;
    
    return noErr;
}

// static
inline void BGMPlayThrough::FillWithSilence(AudioBufferList* ioBuffer)
{
    for(UInt32 i = 0; i < ioBuffer->mNumberBuffers; i++)
    {
        memset(ioBuffer->mBuffers[i].mData, 0, ioBuffer->mBuffers[i].mDataByteSize);
    }
}

// static
inline void BGMPlayThrough::ScaleAudioBuffer(AudioBufferList* ioBuffer, Float32 inLevel)
{
    if(inLevel <= 0.0f)
    {
        FillWithSilence(ioBuffer);
        return;
    }

    for(UInt32 i = 0; i < ioBuffer->mNumberBuffers; i++)
    {
        const UInt32 frames = ioBuffer->mBuffers[i].mDataByteSize / sizeof(Float32);
        Float32* samples = reinterpret_cast<Float32*>(ioBuffer->mBuffers[i].mData);
        for(UInt32 frame = 0; frame < frames; frame++)
        {
            samples[frame] *= inLevel;
        }
    }
}

// static
bool    BGMPlayThrough::UpdateIOProcState(const char* inCallerName,
                                          BGMPlayThroughRTLogger& inRTLogger,
                                          std::atomic<IOState>& inState,
                                          AudioDeviceIOProcID __nullable inIOProcID,
                                          BGMAudioDevice& inDevice,
                                          IOState& outNewState)
{
    BGMAssert(inIOProcID != nullptr, "BGMPlayThrough::UpdateIOProcState: !inIOProcID");

    // Change this IOProc's state to Running if this is the first time it's been called since we
    // started playthrough.
    //
    // compare_exchange_strong will return true iff it changed inState from Starting to Running.
    // Otherwise it will set prevState to the current value of inState.
    //
    // TODO: We probably don't actually need memory_order_seq_cst (the default). Would it be worth
    //       changing? Might be worth checking for the other atomics/barriers in this class, too.
    IOState prevState = IOState::Starting;
    bool didChangeState = inState.compare_exchange_strong(prevState, IOState::Running);

    if(didChangeState)
    {
        BGMAssert(prevState == IOState::Starting, "BGMPlayThrough::UpdateIOProcState: ?!");
        outNewState = IOState::Running;
    }
    else
    {
        // Return the current value of inState to the caller.
        outNewState = prevState;
        
        if(outNewState != IOState::Running)
        {
            // The IOProc isn't Starting or Running, so it must be Stopping. That is, it's been
            // told to stop itself.
            BGMAssert(outNewState == IOState::Stopping,
                      "BGMPlayThrough::UpdateIOProcState: Unexpected state: %d",
                      static_cast<int>(outNewState));
            
            bool stoppedSuccessfully = false;

            try
            {
                inDevice.StopIOProc(inIOProcID);

                // StopIOProc didn't throw, so the IOProc won't be called again until the next
                // time playthrough is started.
                stoppedSuccessfully = true;
            }
            catch(const CAException& e)
            {
                inRTLogger.LogExceptionStoppingIOProc(inCallerName, e.GetError());
            }
            catch(...)
            {
                inRTLogger.LogExceptionStoppingIOProc(inCallerName);
            }

            if(stoppedSuccessfully)
            {
                // Change inState to Stopped.
                //
                // If inState has been changed since we last read it, we don't know if we called
                // StopIOProc before or after the thread that changed it called StartIOProc (if it
                // did). However, inState is only changed here (in the IOProc), in Start and in
                // Stop.
                //
                // Stop won't return until the IOProc has changed inState to Stopped, unless it
                // times out, so Stop should still be waiting. And since Start and Stop are
                // mutually exclusive, this should be safe.
                //
                // But if Stop has timed out and inState has changed, we leave it in its new
                // state (unless there's some ABA problem thing happening), which I suspect is the
                // safest option.
                didChangeState = inState.compare_exchange_strong(outNewState, IOState::Stopped);
                
                if(didChangeState)
                {
                    outNewState = IOState::Stopped;
                }
                else
                {
                    inRTLogger.LogUnexpectedIOStateAfterStopping(inCallerName,
                                                                 static_cast<int>(outNewState));
                }
            }
        }
    }

    return didChangeState;
}

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
//  Copyright © 2016, 2017, 2020 Kyle Neideck
//

// Self Include
#include "BGMPlayThrough.h"

// Local Includes
#include "BGM_Types.h"
#include "BGM_Utils.h"

// PublicUtility Includes
#include "CAHALAudioSystemObject.h"
#include "CAPropertyAddress.h"

// STL Includes
#include <algorithm>  // For std::max

// System Includes
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/task.h>


// The number of IO cycles (roughly) to wait for our IOProcs to stop themselves before assuming something
// went wrong. If that happens, we try to stop them from a non-IO thread and continue anyway. 
static const UInt32 kStopIOProcTimeoutInIOCycles = 600;

#pragma mark Construction/Destruction

BGMPlayThrough::BGMPlayThrough(BGMAudioDevice inInputDevice, BGMAudioDevice inOutputDevice)
:
    mInputDevice(inInputDevice),
    mOutputDevice(inOutputDevice)
{
    Init(inInputDevice, inOutputDevice);
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

void    BGMPlayThrough::Init(BGMAudioDevice inInputDevice, BGMAudioDevice inOutputDevice)
{
    BGMAssert(mInputDeviceIOProcState.is_lock_free(),
              "BGMPlayThrough::BGMPlayThrough: !mInputDeviceIOProcState.is_lock_free()");
    BGMAssert(mOutputDeviceIOProcState.is_lock_free(),
              "BGMPlayThrough::BGMPlayThrough: !mOutputDeviceIOProcState.is_lock_free()");
    BGMAssert(!mActive, "BGMPlayThrough::BGMPlayThrough: Can't init while active.");
    
    mInputDevice = inInputDevice;
    mOutputDevice = inOutputDevice;
    
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
        
        CreateIOProcIDs();
        
        mActive = true;
        
        // TODO: This code (the next two blocks) should be in BGMDeviceControlSync.
        
        // Set BGMDevice's sample rate to match the output device.
        try
        {
            Float64 outputSampleRate = mOutputDevice.GetNominalSampleRate();
            mInputDevice.SetNominalSampleRate(outputSampleRate);
        }
        catch (CAException e)
        {
            LogWarning("BGMPlayThrough::Activate: Failed to sync device sample rates. Error: %d",
                       e.GetError());
        }
        
        // Set BGMDevice's IO buffer size to match the output device.
        try
        {
            UInt32 outputBufferSize = mOutputDevice.GetIOBufferSize();
            mInputDevice.SetIOBufferSize(outputBufferSize);
        }
        catch (CAException e)
        {
            LogWarning("BGMPlayThrough::Activate: Failed to sync device buffer sizes. Error: %d",
                       e.GetError());
        }
        
        DebugMsg("BGMPlayThrough::Activate: Registering for notifications from BGMDevice.");
        
        mInputDevice.AddPropertyListener(CAPropertyAddress(kAudioDevicePropertyDeviceIsRunning),
                                         &BGMPlayThrough::BGMDeviceListenerProc,
                                         this);
        mInputDevice.AddPropertyListener(CAPropertyAddress(kAudioDeviceProcessorOverload),
                                         &BGMPlayThrough::BGMDeviceListenerProc,
                                         this);
        
        bool isBGMDevice = true;
        CATry
        isBGMDevice = mInputDevice.IsBGMDeviceInstance();
        CACatch
        
        if(isBGMDevice)
        {
            mInputDevice.AddPropertyListener(kBGMRunningSomewhereOtherThanBGMAppAddress,
                                             &BGMPlayThrough::BGMDeviceListenerProc,
                                             this);
        }
        else
        {
            LogWarning("BGMPlayThrough::Activate: Playthrough activated with an output device other "
                       "than BGMDevice. This hasn't been tested and is almost definitely a bug.");
            BGMAssert(false, "BGMPlayThrough::Activate: !mInputDevice.IsBGMDeviceInstance()");
        }
    }
}

void    BGMPlayThrough::Deactivate()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    if(mActive)
    {
        DebugMsg("BGMPlayThrough::Deactivate: Deactivating playthrough");
        
        bool inputDeviceIsBGMDevice = true;

        CATry
        inputDeviceIsBGMDevice = mInputDevice.IsBGMDeviceInstance();
        CACatch
        
        // Unregister notification listeners.
        if(inputDeviceIsBGMDevice)
        {
            // There's not much we can do if these calls throw. The docs for AudioObjectRemovePropertyListener
            // just say that means it failed.
            BGMLogAndSwallowExceptions("BGMPlayThrough::Deactivate", [&] {
                mInputDevice.RemovePropertyListener(CAPropertyAddress(kAudioDevicePropertyDeviceIsRunning),
                                                    &BGMPlayThrough::BGMDeviceListenerProc,
                                                    this);
            });
            
            BGMLogAndSwallowExceptions("BGMPlayThrough::Deactivate", [&] {
                mInputDevice.RemovePropertyListener(CAPropertyAddress(kAudioDeviceProcessorOverload),
                                                    &BGMPlayThrough::BGMDeviceListenerProc,
                                                    this);
            });
            
            BGMLogAndSwallowExceptions("BGMPlayThrough::Deactivate", [&] {
                mInputDevice.RemovePropertyListener(kBGMRunningSomewhereOtherThanBGMAppAddress,
                                                    &BGMPlayThrough::BGMDeviceListenerProc,
                                                    this);
            });
        }

        BGMLogAndSwallowExceptions("BGMPlayThrough::Deactivate", [&] {
            Stop();
        });
        
        BGMLogAndSwallowExceptions("BGMPlayThrough::Deactivate", [&] {
            DestroyIOProcIDs();
        });
        
        mActive = false;
    }
}

void    BGMPlayThrough::AllocateBuffer()
{
    // Allocate the ring buffer that will hold the data passing between the devices
    UInt32 numberStreams = 1;
    AudioStreamBasicDescription outputFormat[1];
    mOutputDevice.GetCurrentVirtualFormats(false, numberStreams, outputFormat);
    
    if(numberStreams < 1)
    {
        Throw(CAException(kAudioHardwareUnsupportedOperationError));
    }
    
    // Need to lock the buffer mutexes to make sure the IOProcs aren't accessing it. The order is
    // important here. We always lock them in the same order to prevent deadlocks.
    CAMutex::Locker lockerInput(mBufferInputMutex);
    CAMutex::Locker lockerOutput(mBufferOutputMutex);

    mBuffer = std::unique_ptr<CARingBuffer>(new CARingBuffer);

    // The calculation for the size of the buffer is from Apple's CAPlayThrough.cpp sample code
    //
    // TODO: Test playthrough with hardware with more than 2 channels per frame, a sample (virtual) format other than
    //       32-bit floats and/or an IO buffer size other than 512 frames
    mBuffer->Allocate(outputFormat[0].mChannelsPerFrame,
                      outputFormat[0].mBytesPerFrame,
                      mOutputDevice.GetIOBufferSize() * 20);
}

void    BGMPlayThrough::DeallocateBuffer()
{
    // Need to lock the buffer mutexes to make sure the IOProcs aren't accessing it. The order is
    // important here. We always lock them in the same order to prevent deadlocks.
    CAMutex::Locker lockerInput(mBufferInputMutex);
    CAMutex::Locker lockerOutput(mBufferOutputMutex);
    mBuffer = nullptr;  // Note that the buffer's destructor will deallocate it.
}

void    BGMPlayThrough::CreateIOProcIDs()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    BGMAssert(!mPlayingThrough,
              "BGMPlayThrough::CreateIOProcIDs: Tried to create IOProcs when playthrough was already running");
    BGMAssert(mInputDeviceIOProcID == nullptr,
              "BGMPlayThrough::CreateIOProcIDs: mInputDeviceIOProcID must be destroyed first.");
    BGMAssert(mOutputDeviceIOProcID == nullptr,
              "BGMPlayThrough::CreateIOProcIDs: mOutputDeviceIOProcID must be destroyed first.");
    BGMAssert(CheckIOProcsAreStopped(),
              "BGMPlayThrough::CreateIOProcIDs: IOProcs not ready.");
    
    const bool inDeviceAlive = mInputDevice.IsAlive();
    const bool outDeviceAlive = mOutputDevice.IsAlive();
    
    if(inDeviceAlive && outDeviceAlive)
    {
        DebugMsg("BGMPlayThrough::CreateIOProcIDs: Creating IOProcs");
    
        try
        {
            mInputDeviceIOProcID = mInputDevice.CreateIOProcID(&BGMPlayThrough::InputDeviceIOProc, this);
        }
        catch(CAException e)
        {
            LogWarning("BGMPlayThrough::CreateIOProcIDs: Failed to create input IOProc ID. mInputDevice = %d",
                       mInputDevice.GetObjectID());
            throw;
        }
        
        try
        {
            mOutputDeviceIOProcID = mOutputDevice.CreateIOProcID(&BGMPlayThrough::OutputDeviceIOProc, this);
        }
        catch(CAException e)
        {
            LogWarning("BGMPlayThrough::CreateIOProcIDs: Failed to create output IOProc ID. mOutputDevice = %d",
                       mOutputDevice.GetObjectID());
            DestroyIOProcIDs(); // Clean up.
            throw;
        }

        if(mInputDeviceIOProcID == nullptr || mOutputDeviceIOProcID == nullptr)
        {
            // Should never happen if CAHALAudioDevice::CreateIOProcID didn't throw.
            LogError("BGMPlayThrough::CreateIOProcIDs: Null IOProc ID returned by CreateIOProcID");
            throw new CAException(kAudioHardwareIllegalOperationError);
        }
        
        // TODO: Try using SetIOCycleUsage to reduce latency? Our IOProcs don't really do anything except copy a small
        //       buffer. According to this, Jack OS X considered it:
        //       https://lists.apple.com/archives/coreaudio-api/2008/Mar/msg00043.html but from a quick look at their
        //       code, I don't think they ended up using it.
        // mInputDevice->SetIOCycleUsage(0.01f);
        // mOutputDevice->SetIOCycleUsage(0.01f);
    }
    else
    {
        LogWarning("BGMPlayThrough::CreateIOProcIDs: Failed to create IOProcs.%s%s",
                   (inDeviceAlive ? "" : " Input device not alive."),
                   (outDeviceAlive ? "" : " Output device not alive."));
        throw new CAException(kAudioHardwareIllegalOperationError);
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
            catch(CAException e)
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
    destroy(mOutputDevice, "output", mOutputDeviceIOProcID);
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
    
    if(mOutputDeviceIOProcState != IOState::Stopped)
    {
        LogWarning("BGMPlayThrough::CheckIOProcsAreStopped: Output IOProc not stopped. mOutputDeviceIOProcState = %d",
                   mOutputDeviceIOProcState.load());
        statesOK = false;
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
    mOutputDevice = inOutputDevice ? *inOutputDevice : mOutputDevice;
    
    // Resize and reallocate the buffer if necessary.
    Init(mInputDevice, mOutputDevice);
    
    if(wasActive)
    {
        Activate();
    }
    
    if(wasPlayingThrough)
    {
        Start();
    }
}

#pragma mark Control Playthrough

void    BGMPlayThrough::Start()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    if(mPlayingThrough)
    {
        DebugMsg("BGMPlayThrough::Start: Already started/starting.");

        if(mOutputDeviceIOProcState == IOState::Running)
        {
            ReleaseThreadsWaitingForOutputToStart();
        }

        return;
    }
    
    if(!mInputDevice.IsAlive() || !mOutputDevice.IsAlive())
    {
        LogError("BGMPlayThrough::Start: %s %s",
                 mInputDevice.IsAlive() ? "" : "!mInputDevice",
                 mOutputDevice.IsAlive() ? "" : "!mOutputDevice");
        
        ReleaseThreadsWaitingForOutputToStart();
        
        throw CAException(kAudioHardwareBadDeviceError);
    }
    
    // Set up IOProcs and listeners if they haven't been already.
    Activate();
    
    BGMAssert((mInputDeviceIOProcID != nullptr) && (mOutputDeviceIOProcID != nullptr),
              "BGMPlayThrough::Start: Null IOProc ID");
    
    if((mInputDeviceIOProcState != IOState::Stopped) || (mOutputDeviceIOProcState != IOState::Stopped))
    {
        LogWarning("BGMPlayThrough::Start: IOProc(s) not ready. Trying to start anyway. %s%d %s%d",
                   "mInputDeviceIOProcState = ", mInputDeviceIOProcState.load(),
                   "mOutputDeviceIOProcState = ", mOutputDeviceIOProcState.load());
    }
    
    DebugMsg("BGMPlayThrough::Start: Starting playthrough");
    
    // Start our IOProcs.
    try
    {
        mInputDeviceIOProcState = IOState::Starting;
        mInputDevice.StartIOProc(mInputDeviceIOProcID);
    
        mOutputDeviceIOProcState = IOState::Starting;
        mOutputDevice.StartIOProc(mOutputDeviceIOProcID);
    }
    catch(CAException e)
    {
        ReleaseThreadsWaitingForOutputToStart();
        
        // Log an error message.
        OSStatus err = e.GetError();
        char err4CC[5] = CA4CCToCString(err);
        LogError("BGMPlayThrough::Start: Failed to start %s device. Error: %d (%s)",
                 (mOutputDeviceIOProcState == IOState::Starting ? "output" : "input"),
                 err,
                 err4CC);
        
        // Try to stop the IOProcs in case StartIOProc failed because one of our IOProc was already
        // running. I don't know if it actually does fail in that case, but the documentation
        // doesn't say so it's safer to assume it could.
        CATry
        mInputDevice.StopIOProc(mInputDeviceIOProcID);
        CACatch
        CATry
        mOutputDevice.StopIOProc(mOutputDeviceIOProcID);
        CACatch
        
        mInputDeviceIOProcState = IOState::Stopped;
        mOutputDeviceIOProcState = IOState::Stopped;
        
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
        
        if(!mOutputDevice.IsAlive())
        {
            LogError("BGMPlayThrough::WaitForOutputDeviceToStart: Device not alive");
            return kAudioHardwareBadDeviceError;
        }
    }
    catch(const CAException& e)
    {
        BGMLogException(e);
        return e.GetError();
    }
    
    const IOState initialState = mOutputDeviceIOProcState;
    const UInt64 startedAt = mach_absolute_time();

    if(initialState == IOState::Running)
    {
        // Return early because the output device is already running.
        return kAudioHardwareNoError;
    }
    else if(initialState != IOState::Starting)
    {
        // Warn if we haven't been told to start the output device yet. Usually means we
        // haven't received a kAudioDevicePropertyDeviceIsRunning notification yet, which can
        // happen. It's most common when the user changes the output device while IO is
        // running.
        LogWarning("BGMPlayThrough::WaitForOutputDeviceToStart: Device not starting");
        
        return kDeviceNotStarting;
    }

    // Wait for our output IOProc to start. mOutputDeviceIOProcSemaphore is reset to 0
    // (semaphore_signal_all) when our IOProc is running on the output device.
    //
    // This does mean that we won't have any data the first time our IOProc is called, but I
    // don't know any way to wait until just before that point. (The device's IsRunning property
    // changes immediately after we call StartIOProc.)
    //
    // We check mOutputDeviceIOProcState every 200ms as a fault tolerance mechanism. (Though,
    // I'm not completely sure it's impossible to miss the signal from the IOProc because of a
    // spurious wake up, so it might actually be necessary.)
    DebugMsg("BGMPlayThrough::WaitForOutputDeviceToStart: Waiting.");
    
    kern_return_t theError;
    IOState state;
    UInt64 waitedNsec = 0;
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    
    do
    {
        BGMAssert(mOutputDeviceIOProcSemaphore != SEMAPHORE_NULL,
                  "BGMPlayThrough::WaitForOutputDeviceToStart: !mOutputDeviceIOProcSemaphore");
        
        theError = semaphore_timedwait(mOutputDeviceIOProcSemaphore,
                                       (mach_timespec_t){ 0, 200 * NSEC_PER_MSEC });
        
        // Update the total time we've been waiting and the output device's state.
        waitedNsec = (mach_absolute_time() - startedAt) * info.numer / info.denom;
        state = mOutputDeviceIOProcState;
    }
    while((theError != KERN_SUCCESS) &&         // Signalled from the IOProc.
          (state == IOState::Starting) &&       // IO state changed.
          (waitedNsec < kStartIOTimeoutNsec));  // Timed out.

    if(BGMDebugLoggingIsEnabled())
    {
        UInt64 startedBy = mach_absolute_time();

        struct mach_timebase_info baseInfo = { 0, 0 };
        mach_timebase_info(&baseInfo);
        UInt64 base = baseInfo.numer / baseInfo.denom;

        DebugMsg("BGMPlayThrough::WaitForOutputDeviceToStart: Started %f ms after notification, %f "
                 "ms after entering WaitForOutputDeviceToStart.",
                 static_cast<Float64>(startedBy - mToldOutputDeviceToStartAt) * base / NSEC_PER_MSEC,
                 static_cast<Float64>(startedBy - startedAt) * base / NSEC_PER_MSEC);
    }

    // Figure out which error code to return.
    switch (theError)
    {
        case KERN_SUCCESS:              // Signalled from the IOProc.
            return kAudioHardwareNoError;
            
                                        // IO state changed or we timed out after
        case KERN_OPERATION_TIMED_OUT:  //  - semaphore_timedwait timed out, or
        case KERN_ABORTED:              //  - a spurious wake-up.
            return (state == IOState::Running) ? kAudioHardwareNoError : kAudioHardwareNotRunningError;
            
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
        bool outputDeviceAlive = false;
        
        CATry
        inputDeviceAlive = CAHALAudioObject::ObjectExists(mInputDevice) && mInputDevice.IsAlive();
        CACatch
        
        CATry
        outputDeviceAlive =
            CAHALAudioObject::ObjectExists(mOutputDevice) && mOutputDevice.IsAlive();
        CACatch

        mInputDeviceIOProcState = inputDeviceAlive ? IOState::Stopping : IOState::Stopped;
        mOutputDeviceIOProcState = outputDeviceAlive ? IOState::Stopping : IOState::Stopped;
        
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

            Float64 expectedOutputCycleNs = 0;

            if(outputDeviceAlive)
            {
                expectedOutputCycleNs =
                    mOutputDevice.GetIOBufferSize() * (1 / mOutputDevice.GetNominalSampleRate()) *
                            NSEC_PER_SEC;
            }

            UInt64 expectedMaxCycleNs =
                static_cast<UInt64>(std::max(expectedInputCycleNs, expectedOutputCycleNs));

            while((mInputDeviceIOProcState == IOState::Stopping || mOutputDeviceIOProcState == IOState::Stopping)
                  && (totalWaitNs < kStopIOProcTimeoutInIOCycles * expectedMaxCycleNs))
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
        
        if(mOutputDeviceIOProcState == IOState::Stopping && mOutputDeviceIOProcID != nullptr)
        {
            LogError("BGMPlayThrough::Stop: The output IOProc didn't stop itself in time. Stopping "
                     "it from outside of the IO thread.");
            
            BGMLogUnexpectedExceptions("BGMPlayThrough::Stop", [&]() {
                mOutputDevice.StopIOProc(mOutputDeviceIOProcID);
            });

            mOutputDeviceIOProcState = IOState::Stopped;
        }
        
        mPlayingThrough = false;
    }
    
    mFirstInputSampleTime = -1;
    mLastInputSampleTime = -1;
    mLastOutputSampleTime = -1;
    
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
    
    if(!IsRunningSomewhereOtherThanBGMApp(mInputDevice))
    {
        mLastNotifiedIOStoppedOnBGMDevice = mach_absolute_time();
        
        // Wait a bit before stopping playthrough.
        //
        // This keeps us from starting and stopping IO too rapidly, which wastes CPU, and gives BGMDriver time to update
        // kAudioDeviceCustomPropertyDeviceAudibleState, which it can only do while IO is running. (The wait duration is
        // more or less arbitrary, except that it has to be longer than kDeviceAudibleStateMinChangedFramesForUpdate.)

        // 1 / sample rate = seconds per frame
        Float64 nsecPerFrame = (1.0 / mInputDevice.GetNominalSampleRate()) * NSEC_PER_SEC;
        UInt64 waitNsec = static_cast<UInt64>(20 * kDeviceAudibleStateMinChangedFramesForUpdate * nsecPerFrame);
        UInt64 queuedAt = mLastNotifiedIOStoppedOnBGMDevice;
        
        DebugMsg("BGMPlayThrough::StopIfIdle: Will dispatch stop-if-idle block in %llu ns. %s%llu",
                 waitNsec,
                 "queuedAt=", queuedAt);
        
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, waitNsec),
                       dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                       ^{
                           // Check the BGMPlayThrough instance hasn't been destructed since it queued this block
                           if(mActive)
                           {
                               // The "2" is just to avoid shadowing the other locker
                               CAMutex::Locker stateLocker2(mStateMutex);
                               
                               // Don't stop playthrough if IO has started running again or if
                               // kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp has changed since
                               // this block was queued
                               if(mPlayingThrough
                                  && !IsRunningSomewhereOtherThanBGMApp(mInputDevice)
                                  && queuedAt == mLastNotifiedIOStoppedOnBGMDevice)
                               {
                                   DebugMsg("BGMPlayThrough::StopIfIdle: BGMDevice is only running IO for BGMApp. "
                                            "Stopping playthrough.");
                                   Stop();
                               }
                           }
                       });
    }
}

#pragma mark BGMDevice Listener

// TODO: Listen for changes to the sample rate and IO buffer size of the output device and update the input device to match

// static
OSStatus    BGMPlayThrough::BGMDeviceListenerProc(AudioObjectID inObjectID,
                                                  UInt32 inNumberAddresses,
                                                  const AudioObjectPropertyAddress* __nonnull inAddresses,
                                                  void* __nullable inClientData)
{
    // refCon (reference context) is the instance that registered the listener proc
    BGMPlayThrough* refCon = static_cast<BGMPlayThrough*>(inClientData);
    
    // If the input device isn't BGMDevice, this listener proc shouldn't be registered
    ThrowIf(inObjectID != refCon->mInputDevice.GetObjectID(),
            CAException(kAudioHardwareBadObjectError),
            "BGMPlayThrough::BGMDeviceListenerProc: notified about audio object other than BGMDevice");
    
    for(int i = 0; i < inNumberAddresses; i++)
    {
        switch(inAddresses[i].mSelector)
        {
            case kAudioDeviceProcessorOverload:
                // These warnings are common when you use the UI if you're running a debug build or have "Debug executable"
                // checked. You shouldn't be seeing them otherwise.
                DebugMsg("BGMPlayThrough::BGMDeviceListenerProc: WARNING! Got kAudioDeviceProcessorOverload notification");
                LogWarning("Background Music: CPU overload reported\n");
                break;
                
            // Start playthrough when a client starts IO on BGMDevice and stop when BGMApp (i.e. playthrough itself) is
            // the only client left doing IO.
            //
            // These cases are dispatched to avoid causing deadlocks by triggering one of the following notifications in
            // the process of handling one. Deadlocks could happen if these were handled synchronously when:
            //     - the first BGMDeviceListenerProc call takes the state mutex, then requests some data from the HAL and
            //       waits for it to return,
            //     - the request triggers the HAL to send notifications, which it sends on a different thread,
            //     - the HAL waits for the second BGMDeviceListenerProc call to return before it returns the data
            //       requested by the first BGMDeviceListenerProc call, and
            //     - the second BGMDeviceListenerProc call waits for the first to unlock the state mutex.
                
            case kAudioDevicePropertyDeviceIsRunning:  // Received on the IO thread before our IOProc is called
                HandleBGMDeviceIsRunning(refCon);
                break;
                
            case kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp:
                HandleBGMDeviceIsRunningSomewhereOtherThanBGMApp(refCon);
                break;
                
            default:
                // We might get properties we didn't ask for, so we just ignore them.
                break;
        }
    }
    
    // From AudioHardware.h: "The return value is currently unused and should always be 0."
    return 0;
}

// static
void    BGMPlayThrough::HandleBGMDeviceIsRunning(BGMPlayThrough* refCon)
{
    DebugMsg("BGMPlayThrough::HandleBGMDeviceIsRunning: Got notification");
    
    // This is dispatched because it can block and
    //   - we might be on a real-time thread, or
    //   - BGMXPCListener::startPlayThroughSyncWithReply might get called on the same thread just
    //     before this and time out waiting for this to run.
    //
    // TODO: We should find a way to do this without dispatching because dispatching isn't actually
    //       real-time safe.
    dispatch_async(BGMGetDispatchQueue_PriorityUserInteractive(), ^{
        if(refCon->mActive)
        {
            CAMutex::Locker stateLocker(refCon->mStateMutex);
            
            // Set to true initially because if we fail to get this property from BGMDevice we want to
            // try to start playthrough anyway.
            bool isRunningSomewhereOtherThanBGMApp = true;
            
            BGMLogAndSwallowExceptions("HandleBGMDeviceIsRunning", [&]() {
                // IsRunning doesn't always return true when IO is starting. Using
                // RunningSomewhereOtherThanBGMApp instead seems to be working so far.
                isRunningSomewhereOtherThanBGMApp =
                    IsRunningSomewhereOtherThanBGMApp(refCon->mInputDevice);
            });

            DebugMsg("BGMPlayThrough::HandleBGMDeviceIsRunning: "
                     "BGMDevice is %srunning somewhere other than BGMApp",
                     isRunningSomewhereOtherThanBGMApp ? "" : " not");
            
            if(isRunningSomewhereOtherThanBGMApp)
            {
                refCon->mToldOutputDeviceToStartAt = mach_absolute_time();

                // TODO: Handle expected exceptions (mostly CAExceptions from PublicUtility classes) in Start.
                //       For any that can't be handled sensibly in Start, catch them here and retry a few
                //       times (with a very short delay) before handling them by showing an unobtrusive error
                //       message or something. Then try a different device or just set the system device back
                //       to the real device.
                BGMLogAndSwallowExceptions("HandleBGMDeviceIsRunning", [&refCon]() {
                    refCon->Start();
                });
            }
        }
    });
}

// static
void    BGMPlayThrough::HandleBGMDeviceIsRunningSomewhereOtherThanBGMApp(BGMPlayThrough* refCon)
{
    DebugMsg("BGMPlayThrough::HandleBGMDeviceIsRunningSomewhereOtherThanBGMApp: Got notification");
    
    // These notifications don't need to be handled quickly, so we can always dispatch.
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        // TODO: Handle expected exceptions (mostly CAExceptions from PublicUtility classes) in StopIfIdle.
        BGMLogUnexpectedExceptions("HandleBGMDeviceIsRunningSomewhereOtherThanBGMApp", [&refCon]() {
            if(refCon->mActive)
            {
                refCon->StopIfIdle();
            }
        });
    });
}

// static
bool    BGMPlayThrough::IsRunningSomewhereOtherThanBGMApp(const BGMAudioDevice& inBGMDevice)
{
    return CFBooleanGetValue(
        static_cast<CFBooleanRef>(
            inBGMDevice.GetPropertyData_CFType(kBGMRunningSomewhereOtherThanBGMAppAddress)));
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
                                               const AudioTimeStamp*   inInputTime,
                                               AudioBufferList*        outOutputData,
                                               const AudioTimeStamp*   inOutputTime,
                                               void* __nullable        inClientData)
{
    #pragma unused (inDevice, inNow, inInputData, inInputTime)
    
    // refCon (reference context) is the instance that created the IOProc
    BGMPlayThrough* const refCon = static_cast<BGMPlayThrough*>(inClientData);
    
    IOState state;
    const bool didChangeState = UpdateIOProcState("OutputDeviceIOProc",
                                                  refCon->mRTLogger,
                                                  refCon->mOutputDeviceIOProcState,
                                                  refCon->mOutputDeviceIOProcID,
                                                  refCon->mOutputDevice,
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
        BGMAssert(refCon->mLastOutputSampleTime == -1,
                  "BGMPlayThrough::OutputDeviceIOProc: mLastOutputSampleTime not reset");
        
        refCon->ReleaseThreadsWaitingForOutputToStart();
    }
    
    if(refCon->mLastInputSampleTime == -1)
    {
        // Return early, since we don't have any data to output yet.
        FillWithSilence(outOutputData);
        return noErr;
    }
    
    // If this is the first time this IOProc has been called since starting playthrough...
    if(refCon->mLastOutputSampleTime == -1)
    {
        // Calculate the number of frames between the read and write heads
        refCon->mInToOutSampleOffset = inOutputTime->mSampleTime - refCon->mLastInputSampleTime;
        
        // Log if we dropped frames
        refCon->mRTLogger.LogIfDroppedFrames(refCon->mFirstInputSampleTime,
                                             refCon->mLastInputSampleTime);
    }
    
    CARingBuffer::SampleTime readHeadSampleTime =
        static_cast<CARingBuffer::SampleTime>(inOutputTime->mSampleTime - refCon->mInToOutSampleOffset);
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
    // in a given IO cycle, so it's safe for them to read and write at the same time.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety"
    if(tryer.HasLock() && refCon->mBuffer)
    {
        // Very occasionally (at least for me) our read head gets ahead of input, i.e. we haven't
        // received any new input since this IOProc was last called, and we have to recalculate its
        // position. I figure this might be caused by clock drift but I'm really not sure. It also
        // happens if the input or output sample times are restarted from zero.
        //
        // We also recalculate the offset if the read head is outside of the ring buffer. This
        // happens for example when you plug in or unplug headphones, which causes the output sample
        // times to be restarted from zero.
        //
        // The vast majority of the time, just using lastInputSampleTime as the read head time
        // instead of the one we calculate would work fine (and would also account for the above).
        SInt64 bufferStartTime, bufferEndTime;
        CARingBufferError err = refCon->mBuffer->GetTimeBounds(bufferStartTime, bufferEndTime);
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
                                                refCon->mInToOutSampleOffset);

            // Recalculate the in-to-out offset and read head.
            refCon->mInToOutSampleOffset = inOutputTime->mSampleTime - lastInputSampleTime;
            readHeadSampleTime = static_cast<CARingBuffer::SampleTime>(
                    inOutputTime->mSampleTime - refCon->mInToOutSampleOffset);
        }

        // Copy the frames from the ring buffer.
        err = refCon->mBuffer->Fetch(outOutputData, framesToOutput, readHeadSampleTime);
        refCon->mRTLogger.LogIfRingBufferError_Fetch(err);

        if(err != kCARingBufferError_OK)
        {
            FillWithSilence(outOutputData);
        }
    }
    else
    {
        refCon->mRTLogger.LogRingBufferUnavailable("OutputDeviceIOProc", tryer.HasLock());
        FillWithSilence(outOutputData);
    }
#pragma clang diagnostic pop

    refCon->mLastOutputSampleTime = inOutputTime->mSampleTime;
    
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
                      outNewState);
            
            bool stoppedSuccessfully = false;

            try
            {
                inDevice.StopIOProc(inIOProcID);

                // StopIOProc didn't throw, so the IOProc won't be called again until the next
                // time playthrough is started.
                stoppedSuccessfully = true;
            }
            catch(CAException e)
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
                // state (unless there's some ABA problem thing happening), which I suspect is
                // the safest option.
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


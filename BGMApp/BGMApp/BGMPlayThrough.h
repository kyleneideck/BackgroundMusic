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
//  BGMPlayThrough.h
//  BGMApp
//
//  Copyright © 2016, 2017, 2020, 2026 Kyle Neideck
//
//  Reads audio from an input device and immediately writes it to one or more output devices. We
//  currently use this class with the input device always set to BGMDevice and the output devices
//  set to the ones the user has enabled in the output devices menu.
//
//  Each output device's audio is scaled by the master volume (SetMasterVolume) times the
//  device's relative volume (SetOutputDeviceGain) in its IOProc, in real time.
//
//  The output devices don't need to share a sample rate. The input device is set to the first
//  output device's rate, and the audio is stored in a ring buffer at that rate. An output device
//  whose rate matches the input plays the ring buffer's samples directly. A device with a
//  different rate is resampled in its IOProc using an AudioConverter (created in CreateResamplers,
//  configured with kConverterPrimeMethod_None for real-time safety). The resampler's input
//  callback reads from the ring buffer, so all the devices stay in sync: each one consumes the
//  input at the input rate, regardless of its own rate. A same-rate device skips the resampler
//  entirely, so matched-rate devices have no extra cost.
//
//  Apple's CAPlayThrough sample code (https://developer.apple.com/library/mac/samplecode/CAPlayThrough/Introduction/Intro.html)
//  has a similar class, but I couldn't get it fast enough to use here. Soundflower also has a similar class
//  (https://github.com/mattingalls/Soundflower/blob/master/SoundflowerBed/AudioThruEngine.h) that seems to be based on Apple
//  sample code from 2004. This class's main additions are pausing playthrough when idle to save CPU and
//  playing through to multiple output devices at independent volumes.
//
//  Playing audio with this class uses more CPU, mostly in the coreaudiod process, than playing audio normally because we need
//  an input IOProc as well as an output one, and BGMDriver is running in addition to the output device's driver. For me, it
//  usually adds around 1-2% (as a percentage of total usage -- it doesn't seem to be relative to the
//  CPU used when playing audio normally).
//
//  This class will hopefully not be needed after CoreAudio's aggregate devices get support for controls, which is planned for
//  a future release.
//

#ifndef BGMApp__BGMPlayThrough
#define BGMApp__BGMPlayThrough

// Local Includes
#include "BGMAudioDevice.h"
#include "BGMPlayThroughRTLogger.h"

// PublicUtility Includes
#include "CAMutex.h"
#include "CARingBuffer.h"
#include "BGMThreadSafetyAnalysis.h"

// STL Includes
#include <atomic>
#include <algorithm>
#include <deque>
#include <memory>
#include <vector>

// System Includes
#include <AudioToolbox/AudioConverter.h>
#include <cstdlib>
#include <mach/semaphore.h>


#pragma clang assume_nonnull begin

class BGMPlayThrough
{
    
public:
    // Error codes
    static const OSStatus kDeviceNotStarting = 100;

public:
                        BGMPlayThrough(BGMAudioDevice inInputDevice, BGMAudioDevice inOutputDevice);
                        BGMPlayThrough(BGMAudioDevice inInputDevice, const std::vector<BGMAudioDevice>& inOutputDevices);
                        ~BGMPlayThrough();
    // Disallow copying
                        BGMPlayThrough(const BGMPlayThrough&) = delete;
                        BGMPlayThrough& operator=(const BGMPlayThrough&) = delete;

#ifdef __OBJC__
                        // Only intended as a convenience (hack) for Objective-C instance vars. Call
                        // SetDevices to initialise the instance before using it.
                        BGMPlayThrough() { }
#endif
    
private:
    /*!
     @throws CAException
     @param inOutputDevices At least one output device.
     */
    void                Init(BGMAudioDevice inInputDevice, const std::vector<BGMAudioDevice>& inOutputDevices)
                            REQUIRES(mStateMutex);

public:
    /*! @throws CAException */
    void                Activate();
    /*! @throws CAException */
    void                Deactivate();

private:
    void                AllocateBuffer() REQUIRES(mStateMutex);
    void                DeallocateBuffer();

    /*! @throws CAException */
    void                CreateIOProcIDs();
    /*! @throws CAException */
    void                DestroyIOProcIDs();
    /*!
        @return True if the input IOProc and all the output IOProcs are stopped.
        @nonthreadsafe
     */
    bool                CheckIOProcsAreStopped() const noexcept REQUIRES(mStateMutex);

public:
    /*!
     Pass null for either param to only change one of the devices.
     @throws CAException
     */
    void                SetDevices(const BGMAudioDevice* __nullable inInputDevice,
                                   const BGMAudioDevice* __nullable inOutputDevice);

    /*! Replace both the input and output devices. Blocks while the old devices stop IO.
     @param inOutputDevices At least one output device.
     @throws CAException */
    void                SetDevices(BGMAudioDevice inInputDevice,
                                   const std::vector<BGMAudioDevice>& inOutputDevices);

    /*!
     Replace the output devices. Blocks while the old devices stop IO.
     @param inOutputDevices At least one output device.
     @throws CAException
     */
    void                SetOutputDevices(const std::vector<BGMAudioDevice>& inOutputDevices);

    /*!
     Set the master volume, which is applied to every output device. 0.0 to 1.0. (0.0 also mutes
     all of the output devices.) Realtime safe.
     */
    void                SetMasterVolume(Float32 inVolume) noexcept { mMasterVolume.store(inVolume); }

    /*!
     Set the relative volume of a single output device. 0.0 to 1.0. The device's level is
     master volume * relative volume. Realtime safe.
     */
    void                SetOutputDeviceGain(AudioObjectID inDeviceID, Float32 inGain);

    /*!
     The master volume. See SetMasterVolume.
     */
    Float32             GetMasterVolume() const noexcept { return mMasterVolume.load(); }

    /*! @throws CAException */
    void                Start();
    
    // Blocks until the first output device has started our IOProc. Returns one of the error
    // constants from AudioHardwareBase.h (e.g. kAudioHardwareNoError).
    OSStatus            WaitForOutputDeviceToStart() noexcept;

private:
    
    /*! Real-time safe. */
    void                ReleaseThreadsWaitingForOutputToStart();
    
public:
    OSStatus            Stop();
    void                StopIfIdle();
    
private:
    
    static bool         IsRunningSomewhereOtherThanBGMApp(const BGMAudioDevice& inBGMDevice);

    // This lets the unit tests access the private members.
    friend class BGMPlayThroughTestAccess;

    static OSStatus     InputDeviceIOProc(AudioObjectID           inDevice,
                                          const AudioTimeStamp*   inNow,
                                          const AudioBufferList*  inInputData,
                                          const AudioTimeStamp*   inInputTime,
                                          AudioBufferList*        outOutputData,
                                          const AudioTimeStamp*   inOutputTime,
                                          void* __nullable        inClientData);
    static OSStatus     OutputDeviceIOProc(AudioObjectID           inDevice,
                                           const AudioTimeStamp*   inNow,
                                           const AudioBufferList*  inInputData,
                                           const AudioTimeStamp*   inInputTime,
                                           AudioBufferList*        outOutputData,
                                           const AudioTimeStamp*   inOutputTime,
                                           void* __nullable        inClientData);

    // Tells the HAL that our IOProc on inDevice doesn't use the streams in one direction, so it
    // won't engage them. inIsInput picks the direction to disable: true disables the input streams,
    // false disables the output streams.
    static void         DisableIOProcStreams(BGMAudioDevice& inDevice,
                                             AudioDeviceIOProcID inIOProcID,
                                             bool inIsInput,
                                             const char* inDeviceName);

    /*! Fills the given ABL with zeroes to make it silent. */
    static inline void  FillWithSilence(AudioBufferList* ioBuffer);

    // Scales the samples in the given ABL by inLevel. Realtime safe. Assumes 32-bit float samples,
    // which is the only format BGMDevice outputs.
    static inline void  ScaleAudioBuffer(AudioBufferList* ioBuffer, Float32 inLevel);

    // The state of an IOProc. Used by the IOProc to tell other threads when it's finished starting. Used by other
    // threads to tell the IOProc to stop itself. (Probably used for other things as well.)
    enum class          IOState
                        {
                            Stopped, Starting, Running, Stopping
                        };
    
    // Per-output-device state and settings. The atomics make it non-movable, so it's stored in a
    // deque (which keeps references to its elements stable when it grows or shrinks).
    //
    // The IOProc accesses its record's fields without holding mStateMutex. The records are only
    // added or removed while their IOProcs are stopped (SetOutputDevices/Deactivate stop the
    // IOProcs before changing the records), so that's safe.
    struct OutputDevice
    {
        BGMAudioDevice                     device;

        OutputDevice(BGMAudioDevice inDevice) : device(inDevice) { }

        AudioDeviceIOProcID __nullable     ioProcID = nullptr;
        std::atomic<IOState>               state { IOState::Stopped };
        // The relative volume of this output device (0.0-1.0). It's multiplied by mMasterVolume
        // in the IOProc.
        std::atomic<Float32>               gain { 1.0f };
        // The sample time of the last call to this device's IOProc. -1 for unset.
        Float64                            lastOutputSampleTime = -1;
        // Subtract this from the output time to get the input time.
        Float64                            inToOutSampleOffset = 0.0;

        // Resampling support (see the header comment). Only used when the device's sample rate
        // differs from the input device's (needsResampling).
        bool                               needsResampling = false;
        AudioConverterRef __nullable       converter = nullptr;
        // The input sample time of the next frame to read from the ring buffer for the resampler's
        // input callback. -1 until the resampler has read its first frame.
        CARingBuffer::SampleTime           resamplerInputSampleTime = -1;
        // Pre-allocated input buffer for the resampler's input callback (see CreateResamplers).
        // Allocated in CreateResamplers and freed here (or in DestroyResamplers).
        Byte* __nullable                   resamplerInputBuffer = nullptr;
        UInt32                             resamplerInputBufferSize = 0;

        ~OutputDevice()
        {
            if(converter != nullptr)
            {
                AudioConverterDispose(static_cast<AudioConverterRef>(converter));
                converter = nullptr;
            }
            if(resamplerInputBuffer != nullptr)
            {
                free(resamplerInputBuffer);
                resamplerInputBuffer = nullptr;
            }
        }
    };

    // Creates the AudioConverters for the output devices whose sample rate differs from the
    // input device's. Called from Activate, before the IOProcs start. @throws CAException
    void                CreateResamplers();
    // Destroys the AudioConverters and frees their input buffers. Called from Deactivate, after
    // the IOProcs have stopped.
    void                DestroyResamplers();

    // The input callback for the resamplers' AudioConverters (see AudioConverter.h). Real-time
    // safe: when reading from the ring buffer it uses the pre-allocated per-device buffer, so it
    // doesn't allocate. See the .cpp for details.
    static OSStatus     ResamplerInputProc(AudioConverterRef                   inAudioConverter,
                                           UInt32*                             ioNumberDataPackets,
                                           AudioBufferList*                    ioData,
                                           AudioStreamPacketDescription* __nullable* __nullable outDataPacketDescription,
                                           void* __nullable                    inUserData);

    // State passed to ResamplerInputProc as its inUserData.
    struct ResamplerCallbackState
    {
        // The ring buffer to read from. Null when the callback should provide silence instead
        // (used to pre-warm a newly-created resampler).
        CARingBuffer* __nullable    buffer;
        // The device record, for the resampler's input sample time. Null when buffer is null.
        OutputDevice* __nullable    record;
        // The source format's bytes per frame, for sizing the callback's buffer.
        UInt32                    inputBytesPerFrame;
        // The pre-allocated input buffer the callback fills (so it never allocates). Non-null in
        // both the pre-warm and the real-time path.
        Byte* __nullable          inputBuffer;
        UInt32                    inputBufferSize;
    };

    // The IOProcs call this to update their IOState member. Also stops the IOProc if its state has been set to Stopping.
    // Returns true if it changes the state.
    static bool         UpdateIOProcState(const char* inCallerName,
                                          BGMPlayThroughRTLogger& inRTLogger,
                                          std::atomic<IOState>& inState,
                                          AudioDeviceIOProcID __nullable inIOProcID,
                                          BGMAudioDevice& inDevice,
                                          IOState& outNewState);

private:
    std::unique_ptr<CARingBuffer>    mBuffer PT_GUARDED_BY(mBufferInputMutex)
                                        PT_GUARDED_BY(mBufferOutputMutex) { nullptr };
    // The format of the audio stored in mBuffer (i.e. the first output device's virtual format,
    // at the input device's sample rate). Used as the resamplers' source format. Set in
    // AllocateBuffer.
    AudioStreamBasicDescription      mBufferFormat { };
    
    AudioDeviceIOProcID __nullable mInputDeviceIOProcID { nullptr };
    
    BGMAudioDevice      mInputDevice { kAudioObjectUnknown };
    // At least one element while active. Guarded by mStateMutex except for the per-record fields
    // that the IOProcs read (see the OutputDevice comments above).
    //
    // The records are heap allocated because they contain std::atomic fields, which are
    // non-copyable, so they can't be stored by value in a deque.
    std::deque<std::unique_ptr<OutputDevice>> mOutputDevices;

    // The master volume applied to every output device (0.0-1.0). See SetMasterVolume.
    std::atomic<Float32>  mMasterVolume { 1.0f };

    // mStateMutex is the general purpose mutex. mBufferInputMutex and mBufferOutputMutex are
    // just used to make sure mBuffer, the ring buffer, is allocated when the IOProcs access it. See
    // the comments in the IOProcs for details.
    //
    // If a thread might lock more than one of these mutexes, it *must* take them in this order:
    //     1. mStateMutex
    //     2. mBufferInputMutex
    //     3. mBufferOutputMutex
    //
    // TODO: It would be better to have a separate class for the buffer and its mutexes.
    CAMutex             mStateMutex ACQUIRED_BEFORE(mBufferInputMutex)
                            ACQUIRED_BEFORE(mBufferOutputMutex) { "Playthrough state" };
    CAMutex             mBufferInputMutex ACQUIRED_BEFORE(mBufferOutputMutex)
                            { "Playthrough ring buffer input" };
    CAMutex             mBufferOutputMutex { "Playthrough ring buffer output" };

    // Signalled when an output IOProc runs. We use it to tell BGMDriver when the output device is ready to receive audio data.
    semaphore_t         mOutputDeviceIOProcSemaphore { SEMAPHORE_NULL };
    
    bool                mActive = false;
    bool                mPlayingThrough = false;

    std::atomic<IOState>    mInputDeviceIOProcState { IOState::Stopped };
    
    // For debug logging.
    UInt64              mToldOutputDeviceToStartAt { 0 };

    // IOProc vars. (Should only be used inside IOProcs.)
    
    // The earliest/latest sample times seen by the input IOProc since starting playthrough. -1 for unset.
    Float64             mFirstInputSampleTime = -1;
    Float64             mLastInputSampleTime = -1;

    BGMPlayThroughRTLogger mRTLogger;

};

#pragma clang assume_nonnull end

#endif /* BGMApp__BGMPlayThrough */

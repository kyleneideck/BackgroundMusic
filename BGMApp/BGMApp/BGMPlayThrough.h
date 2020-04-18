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
//  Copyright © 2016, 2017, 2020 Kyle Neideck
//
//  Reads audio from an input device and immediately writes it to an output device. We currently use this class with the input
//  device always set to BGMDevice and the output device set to the one selected in the preferences menu.
//
//  Apple's CAPlayThrough sample code (https://developer.apple.com/library/mac/samplecode/CAPlayThrough/Introduction/Intro.html)
//  has a similar class, but I couldn't get it fast enough to use here. Soundflower also has a similar class
//  (https://github.com/mattingalls/Soundflower/blob/master/SoundflowerBed/AudioThruEngine.h) that seems to be based on Apple
//  sample code from 2004. This class's main addition is pausing playthrough when idle to save CPU.
//
//  Playing audio with this class uses more CPU, mostly in the coreaudiod process, than playing audio normally because we need
//  an input IOProc as well as an output one, and BGMDriver is running in addition to the output device's driver. For me, it
//  usually adds around 1-2% (as a percentage of total usage -- it doesn't seem to be relative to the CPU used when playing
//  audio normally).
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
#include <memory>

// System Includes
#include <mach/semaphore.h>


#pragma clang assume_nonnull begin

class BGMPlayThrough
{
    
public:
    // Error codes
    static const OSStatus kDeviceNotStarting = 100;

public:
                        BGMPlayThrough(BGMAudioDevice inInputDevice, BGMAudioDevice inOutputDevice);
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
    /*! @throws CAException */
    void                Init(BGMAudioDevice inInputDevice, BGMAudioDevice inOutputDevice)
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
        @return True if both IOProcs are stopped.
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
    
    /*! @throws CAException */
    void                Start();
    
    // Blocks until the output device has started our IOProc. Returns one of the error constants
    // from AudioHardwareBase.h (e.g. kAudioHardwareNoError).
    OSStatus            WaitForOutputDeviceToStart() noexcept;
    
private:
    /*! Real-time safe. */
    void                ReleaseThreadsWaitingForOutputToStart();
    
public:
    OSStatus            Stop();
    void                StopIfIdle();
    
private:
    
    static OSStatus     BGMDeviceListenerProc(AudioObjectID inObjectID,
                                              UInt32 inNumberAddresses,
                                              const AudioObjectPropertyAddress* inAddresses,
                                              void* __nullable inClientData);
    static void         HandleBGMDeviceIsRunning(BGMPlayThrough* refCon);
    static void         HandleBGMDeviceIsRunningSomewhereOtherThanBGMApp(BGMPlayThrough* refCon);
    
    static bool         IsRunningSomewhereOtherThanBGMApp(const BGMAudioDevice& inBGMDevice);

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

    /*! Fills the given ABL with zeroes to make it silent. */
    static inline void  FillWithSilence(AudioBufferList* ioBuffer);

    // The state of an IOProc. Used by the IOProc to tell other threads when it's finished starting. Used by other
    // threads to tell the IOProc to stop itself. (Probably used for other things as well.)
    enum class          IOState
                        {
                            Stopped, Starting, Running, Stopping
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
    
    AudioDeviceIOProcID __nullable mInputDeviceIOProcID { nullptr };
    AudioDeviceIOProcID __nullable mOutputDeviceIOProcID { nullptr };
    
    BGMAudioDevice      mInputDevice { kAudioObjectUnknown };
    BGMAudioDevice      mOutputDevice { kAudioObjectUnknown };

    // mStateMutex is the general purpose mutex. mBufferInputMutex and mBufferOutputMutex are
    // just used to make sure mBuffer, the ring buffer, is allocated when the IOProcs access it. See
    // the comments in the IOProcs for details.
    //
    // If a thread might lock more than one of these mutexes, it *must* take them in this order:
    //     1. mStateMutex
    //     2. mBufferInputMutex
    //     3. mBufferOutputMutex
    //
    // The ACQUIRED_BEFORE annotations don't do anything yet. From clang's docs: "ACQUIRED_BEFORE(…)
    // and ACQUIRED_AFTER(…) are currently unimplemented. To be fixed in a future update." After
    // they've fixed that, the compiler will enforce the ordering statically.
    //
    // TODO: We can't use std::shared_lock because we're still on C++11, but we could use std::lock
    //       to help ensure the locks are always taken in the right order.
    // TODO: It would be better to have a separate class for the buffer and its mutexes.
    CAMutex             mStateMutex ACQUIRED_BEFORE(mBufferInputMutex)
                            ACQUIRED_BEFORE(mBufferOutputMutex) { "Playthrough state" };
    CAMutex             mBufferInputMutex ACQUIRED_BEFORE(mBufferOutputMutex)
                            { "Playthrough ring buffer input" };
    CAMutex             mBufferOutputMutex { "Playthrough ring buffer output" };

    // Signalled when the output IOProc runs. We use it to tell BGMDriver when the output device is ready to receive audio data.
    semaphore_t         mOutputDeviceIOProcSemaphore { SEMAPHORE_NULL };
    
    bool                mActive = false;
    bool                mPlayingThrough = false;

    UInt64              mLastNotifiedIOStoppedOnBGMDevice { 0 };

    std::atomic<IOState>    mInputDeviceIOProcState { IOState::Stopped };
    std::atomic<IOState>    mOutputDeviceIOProcState { IOState::Stopped };
    
    // For debug logging.
    UInt64              mToldOutputDeviceToStartAt { 0 };

    // IOProc vars. (Should only be used inside IOProcs.)
    
    // The earliest/latest sample times seen by the IOProcs since starting playthrough. -1 for unset.
    Float64             mFirstInputSampleTime = -1;
    Float64             mLastInputSampleTime = -1;
    Float64             mLastOutputSampleTime = -1;
    
    // Subtract this from the output time to get the input time.
    Float64             mInToOutSampleOffset { 0.0 };

    BGMPlayThroughRTLogger mRTLogger;

};

#pragma clang assume_nonnull end

#endif /* BGMApp__BGMPlayThrough */


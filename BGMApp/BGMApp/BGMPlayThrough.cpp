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
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#include "BGMPlayThrough.h"

// Local Includes
#include "BGM_Types.h"

// PublicUtility Includes
#include "CAAtomic.h"

// STL Includes
#include <algorithm>  // For std::max

// System Includes
#include <mach/mach_time.h>


#pragma mark Construction/Destruction

static const AudioObjectPropertyAddress kDeviceIsRunningAddress = {
    kAudioDevicePropertyDeviceIsRunning,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};

static const AudioObjectPropertyAddress kProcessorOverloadAddress = {
    kAudioDeviceProcessorOverload,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};

BGMPlayThrough::BGMPlayThrough(CAHALAudioDevice inInputDevice, CAHALAudioDevice inOutputDevice)
:
    mInputDevice(inInputDevice),
    mOutputDevice(inOutputDevice)
{
    AllocateBuffer();
}

BGMPlayThrough::~BGMPlayThrough()
{
    Deactivate();
}

void    BGMPlayThrough::Swap(BGMPlayThrough &inPlayThrough)
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    bool wasPlayingThrough = inPlayThrough.mPlayingThrough;
    
    Deactivate();
    
    mInputDevice = inPlayThrough.mInputDevice;
    mOutputDevice = inPlayThrough.mOutputDevice;
    
    AllocateBuffer();
    
    inPlayThrough.Deactivate();
    
    if(wasPlayingThrough)
    {
        Start();
    }
}

void    BGMPlayThrough::Activate()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    if(!mActive)
    {
        CreateIOProcs();
        
        // Start the output device. (See the comments in PauseIfIdle for an explanation.)
        mOutputDevice.StartIOProc(NULL);
        
        if(IsBGMDevice(mInputDevice))
        {
            // Set BGMDevice sample rate to match the output device
            Float64 outputSampleRate = mOutputDevice.GetNominalSampleRate();
            mInputDevice.SetNominalSampleRate(outputSampleRate);
            
            // Set BGMDevice IO buffer size to match the output device
            mInputDevice.SetNominalSampleRate(outputSampleRate);
            
            // Register for notifications from BGMDevice
            mInputDevice.AddPropertyListener(kDeviceIsRunningAddress, &BGMPlayThrough::BGMDeviceListenerProc, this);
            mInputDevice.AddPropertyListener(kProcessorOverloadAddress, &BGMPlayThrough::BGMDeviceListenerProc, this);
            mInputDevice.AddPropertyListener(kBGMRunningSomewhereOtherThanBGMAppAddress,
                                             &BGMPlayThrough::BGMDeviceListenerProc,
                                             this);
        }
        
        mActive = true;
    }
}

void    BGMPlayThrough::Deactivate()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    if(mActive)
    {
        DebugMsg("BGMPlayThrough::Deactivate: Deactivating playthrough");
        
        if(IsBGMDevice(mInputDevice))
        {
            // Unregister notification listeners
            mInputDevice.RemovePropertyListener(kDeviceIsRunningAddress, &BGMPlayThrough::BGMDeviceListenerProc, this);
            mInputDevice.RemovePropertyListener(kProcessorOverloadAddress, &BGMPlayThrough::BGMDeviceListenerProc, this);
            mInputDevice.RemovePropertyListener(kBGMRunningSomewhereOtherThanBGMAppAddress,
                                                &BGMPlayThrough::BGMDeviceListenerProc,
                                                this);
        }
        
        if(!mPlayingThrough)
        {
            // Balance the StartIOProc(NULL) call in Activate() to stop the output device. (See the docs for
            // AudioDeviceStart in AudioHardware.h)
            mOutputDevice.StopIOProc(NULL);
        }
        
        DestroyIOProcs();
        
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
    
    // The calculation for the size of the buffer is from Apple's CAPlayThrough.cpp sample code
    //
    // TODO: Test playthrough with hardware with more than 2 channels per frame, a sample (virtual) format other than
    //       32-bit floats and/or an IO buffer size other than 512 frames
    mBuffer.Allocate(outputFormat[0].mChannelsPerFrame,
                     outputFormat[0].mBytesPerFrame,
                     mOutputDevice.GetIOBufferSize() * 20);
}

// static
bool    BGMPlayThrough::IsBGMDevice(CAHALAudioDevice inDevice)
{
    CFStringRef uid = inDevice.CopyDeviceUID();
    bool isBGMDevice = CFEqual(uid, CFSTR(kBGMDeviceUID));
    CFRelease(uid);
    return isBGMDevice;
}

void    BGMPlayThrough::CreateIOProcs()
{
    Assert(!mPlayingThrough, "BGMPlayThrough::CreateIOProcs: Tried to create IOProcs when playthrough was already running");
    
    if(mInputDevice.IsAlive() && mOutputDevice.IsAlive())
    {
        mInputDeviceIOProcID = mInputDevice.CreateIOProcID(&BGMPlayThrough::InputDeviceIOProc, this);
        mOutputDeviceIOProcID = mOutputDevice.CreateIOProcID(&BGMPlayThrough::OutputDeviceIOProc, this);
        
        Assert(mInputDeviceIOProcID != NULL && mOutputDeviceIOProcID != NULL,
               "BGMPlayThrough::CreateIOProcs: Null IOProc ID returned by CreateIOProcID");
        
        // TODO: Try using SetIOCycleUsage to reduce latency? Our IOProcs don't really do anything except copy a small
        //       buffer. According to this, Jack OS X considered it:
        //       https://lists.apple.com/archives/coreaudio-api/2008/Mar/msg00043.html but from a quick look at their
        //       code, I don't think they ended up using it.
        // mInputDevice->SetIOCycleUsage(0.01f);
        // mOutputDevice->SetIOCycleUsage(0.01f);
    }
}

void    BGMPlayThrough::DestroyIOProcs()
{
    Pause();
    
    if(mInputDeviceIOProcID != NULL)
    {
        mInputDevice.DestroyIOProcID(mInputDeviceIOProcID);
        mInputDeviceIOProcID = NULL;
    }
    
    if(mOutputDeviceIOProcID != NULL)
    {
        mOutputDevice.DestroyIOProcID(mOutputDeviceIOProcID);
        mOutputDeviceIOProcID = NULL;
    }
}

#pragma mark Control Playthrough

OSStatus    BGMPlayThrough::Start()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    if(!mPlayingThrough && mInputDevice.IsAlive() && mOutputDevice.IsAlive())
    {
        DebugMsg("BGMPlayThrough::Start: Starting playthrough");
        
        // Set up IOProcs and listeners if they aren't already
        Activate();
        
        // Just in case Pause() didn't reset these for some reason
        mInputDeviceIOProcShouldStop = false;
        mOutputDeviceIOProcShouldStop = false;
        CAMemoryBarrier();
        
        // Start our IOProcs
        mInputDevice.StartIOProc(mInputDeviceIOProcID);
        // mOutputDevice.SetIOBufferSize(512);
        mOutputDevice.StartIOProc(mOutputDeviceIOProcID);
        
        mPlayingThrough = true;
    }

    return noErr;
}

OSStatus    BGMPlayThrough::Pause()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    if(mActive && mPlayingThrough)
    {
        DebugMsg("BGMPlayThrough::Pause: Pausing playthrough");
        
        if(mInputDevice.IsAlive())
        {
            mInputDeviceIOProcShouldStop = true;
        }
        if(mOutputDevice.IsAlive())
        {
            mOutputDeviceIOProcShouldStop = true;
        }
        
        // Wait for the IOProcs to stop themselves, with a timeout of about two IO cycles. This is so the IOProcs don't get called
        // after the BGMPlayThrough instance (pointed to by the client data they get from the HAL) is deallocated.
        //
        // From Jeff Moore on the Core Audio mailing list:
        //     Note that there is no guarantee about how many times your IOProc might get called after AudioDeviceStop() returns
        //     when you make the call from outside of your IOProc. However, if you call AudioDeviceStop() from inside your IOProc,
        //     you do get the guarantee that your IOProc will not get called again after the IOProc has returned.
        UInt64 totalWaitNs = 0;
        Float64 expectedInputCycleNs = mInputDevice.GetIOBufferSize() * (1 / mInputDevice.GetNominalSampleRate()) * NSEC_PER_SEC;
        Float64 expectedOutputCycleNs = mOutputDevice.GetIOBufferSize() * (1 / mOutputDevice.GetNominalSampleRate()) * NSEC_PER_SEC;
        UInt64 expectedMaxCycleNs = static_cast<UInt64>(std::max(expectedInputCycleNs, expectedOutputCycleNs));
        while((mInputDeviceIOProcShouldStop || mOutputDeviceIOProcShouldStop) && totalWaitNs < 2 * expectedMaxCycleNs)
        {
            // TODO: If playthrough is started again while we're waiting in this loop we could drop frames. Wait on a semaphore
            //       instead of sleeping? That way Start() could also signal it, before waiting on the state mutex, as a way of
            //       cancelling the pause operation.
            struct timespec rmtp;
            int err = nanosleep((const struct timespec[]){{0, NSEC_PER_MSEC}}, &rmtp);
            totalWaitNs += NSEC_PER_MSEC - (err == -1 ? rmtp.tv_nsec : 0);
            CAMemoryBarrier();
        }
        
        // Clean up if the IOProcs didn't stop themselves
        if(mInputDeviceIOProcShouldStop && mInputDeviceIOProcID != NULL)
        {
            DebugMsg("BGMPlayThrough::Pause: The input IOProc didn't stop itself in time. Stopping it from outside of the IO thread.");
            mInputDevice.StopIOProc(mInputDeviceIOProcID);
            mInputDeviceIOProcShouldStop = false;
        }
        if(mOutputDeviceIOProcShouldStop && mOutputDeviceIOProcID != NULL)
        {
            DebugMsg("BGMPlayThrough::Pause: The output IOProc didn't stop itself in time. Stopping it from outside of the IO thread.");
            mOutputDevice.StopIOProc(mOutputDeviceIOProcID);
            mOutputDeviceIOProcShouldStop = false;
        }
        
        /*
        // Increase the size of the IO buffers while idle to save CPU
        UInt32 unusedMin, max;
        mOutputDevice->GetIOBufferSizeRange(unusedMin, max);
        DebugMsg("BGMPlayThrough::Pause: Set output device to its max IO buffer size: %u", max);
        mOutputDevice->SetIOBufferSize(max);
        */
        
        mPlayingThrough = false;
    }
    
    mFirstInputSampleTime = -1;
    mLastInputSampleTime = -1;
    mLastOutputSampleTime = -1;
    
    return noErr;
}

void    BGMPlayThrough::PauseIfIdle()
{
    // To save CPU time, we pause playthrough when no clients are doing IO. On my system, this reduces coreaudiod's CPU
    // use from around 5% to 2.5% and BGMApp's from around 0.6% to 0.3%. If this isn't working for you, a client might be
    // running IO without being audible. VLC does that when you have a file paused, for example.
    //
    // The driver for Apple's built-in audio uses a fair bit of CPU doing some DSP on all audio playing through the
    // built-in speakers. So this might not have much effect if you're using headphones or different audio hardware.
    //
    // To minimize latency, rather than stopping IO on the hardware completely, we just stop our IOProcs. (See Pause().)
    // We can start them again quickly enough that this doesn't add latency, though I'm not sure the HAL guarantees that.
    // This function should at least be called with real-time priority because we call it when we get the
    // kAudioDevicePropertyDeviceIsRunning notification. From Jeff Moore on the Core Audio mailing list:
    //     "Only kAudioDevicePropertyDeviceIsRunning comes from the IO thread, but it happens at a relatively safe time
    //      - either before the HAL starts tracking time or after it is finished.
    //
    // Another option is to increase the size of the IO buffers on the output device (with SetIOBufferSize()), which uses
    // less CPU at the cost of higher latency. I also tried increasing the size of the IO buffers while playthrough is
    // paused but changing them back before starting playthrough. It was around 4-5 times faster than restarting IO for
    // me, but it still roughly doubled latency compared to the current solution. And of course that's all completely
    // hardware-dependent.
    //
    // TODO: I think the correct way to handle this is for BGMDriver's StartIO function to tell BGMApp to start IO on the
    //       output device, and not return until BGMApp replies that IO has started. We should probably use XPC to send
    //       those messages rather than HAL notifications. With this solution we'd add no more latency than with the
    //       current solution (which is no more than if we run the hardware constantly -- the latency comes from needing
    //       an extra output buffer) and we wouldn't have to waste any CPU time when audio is idle.
    
    CAMutex::Locker stateLocker(mStateMutex);
    
    Assert(IsBGMDevice(mInputDevice),
           "BGMDevice not set as input device. PauseIfIdle can't tell if other devices are idle");
    
    if(!RunningSomewhereOtherThanBGMApp(mInputDevice))
    {
        mLastNotifiedIOStoppedOnBGMDevice = mach_absolute_time();
        
        // Wait a bit before pausing playthrough
        //
        // This stops us from starting and stopping IO too rapidly, which wastes CPU, and gives BGMDriver time to update
        // kAudioDeviceCustomPropertyDeviceAudibleState, which it can only do while IO is running. (The wait duration is
        // more or less arbitrary, except that it has to be longer than kDeviceAudibleStateMinChangedFramesForUpdate.)

        // 1 / sample rate = seconds per frame
        Float64 nsecPerFrame = (1.0 / mInputDevice.GetNominalSampleRate()) * NSEC_PER_SEC;
        UInt64 waitNsec = static_cast<UInt64>(20 * kDeviceAudibleStateMinChangedFramesForUpdate * nsecPerFrame);
        UInt64 queuedAt = mLastNotifiedIOStoppedOnBGMDevice;
        
        DebugMsg("BGMPlayThrough::PauseIfIdle: Will dispatch pause-if-idle block in %llu ns. %s%llu",
                 waitNsec,
                 "queuedAt=", queuedAt);
        
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, waitNsec),
                       dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0),
                       ^{
                           // Check the BGMPlayThrough instance hasn't been destructed since it queued this block
                           if(mActive)
                           {
                               // The "2" is just to avoid shadowing the other locker
                               CAMutex::Locker stateLocker2(mStateMutex);
                               
                               // Don't Pause playthrough if IO has started running again or if
                               // kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp has changed since
                               // this block was queued
                               if(mPlayingThrough && !RunningSomewhereOtherThanBGMApp(mInputDevice)
                                  && queuedAt == mLastNotifiedIOStoppedOnBGMDevice)
                               {
                                   DebugMsg("BGMPlayThrough::PauseIfIdle: BGMDevice is only running IO for BGMApp. %s",
                                            "Pausing playthrough.");
                                   Pause();
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
                LogWarning("Background Music: CPU overload reported");
                break;
                
            // Start playthrough when a client starts IO on BGMDevice and pause when BGMApp (i.e. playthrough itself) is
            // the only client left doing IO.
            //
            // These cases are dispatched to avoid causing deadlocks by triggering one of the following notifications in
            // the process of handling one. Deadlocks could happen if these were handled synchronously when:
            //     - the first ListenerProc call takes the state mutex, then requests some data from the HAL and waits
            //       for it to return,
            //     - the request triggers the HAL to send notifications, which it sends on a different thread,
            //     - the HAL waits for the second ListenerProc call to return before it returns the data requested by the
            //       first ListenerProc call, and
            //     - the second ListenerProc call waits for the first to unlock the state mutex.
                
            case kAudioDevicePropertyDeviceIsRunning:  // Received on the IO thread before our IOProc is called
                {
                    DebugMsg("BGMPlayThrough::BGMDeviceListenerProc: Got kAudioDevicePropertyDeviceIsRunning notification");
                    
                    auto deviceIsRunningHandler = [refCon] {
                        // IsRunning doesn't always return true when IO is starting. Not sure why. But using
                        // RunningSomewhereOtherThanBGMApp instead seems to be working so far.
                        //
                        //if(refCon->mInputDevice->IsRunning())
                        if(RunningSomewhereOtherThanBGMApp(refCon->mInputDevice))
                        {
                            refCon->Start();
                        }
                    };
                    
                    CAMutex::Tryer stateTrier(refCon->mStateMutex);
                    if(stateTrier.HasLock())
                    {
                        // In the vast majority of cases (when we actually start playthrough here) we get the state lock
                        // and can invoke the handler directly
                        deviceIsRunningHandler();
                    }
                    else
                    {
                        // TODO: This should be rare, but we still shouldn't dispatch on the IO thread because it isn't
                        //       real-time safe.
                        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
                            if(refCon->mActive)
                            {
                                DebugMsg("BGMPlayThrough::BGMDeviceListenerProc: Handling %s",
                                         "kAudioDevicePropertyDeviceIsRunning notification in dispatched block");
                                CAMutex::Locker stateLocker(refCon->mStateMutex);
                                deviceIsRunningHandler();
                            }
                        });
                    }
                }
                break;
                
            case kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp:
                DebugMsg("BGMPlayThrough::BGMDeviceListenerProc: Got %s",
                         "kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp notification");
                
                // These notifications don't need to be handled very quickly, so we can always dispatch
                dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
                    if(refCon->mActive)
                    {
                        refCon->PauseIfIdle();
                    }
                });
                break;
                
            default:
                break;
        }
    }
    
    return 0;
}

// static
bool    BGMPlayThrough::RunningSomewhereOtherThanBGMApp(const CAHALAudioDevice inBGMDevice)
{
    return CFBooleanGetValue(
        static_cast<CFBooleanRef>(
            inBGMDevice.GetPropertyData_CFType(kBGMRunningSomewhereOtherThanBGMAppAddress)));
}

#pragma mark IOProcs

// Note that the IOProcs will very likely not run on the same thread and that they intentionally don't lock any mutexes.

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
    BGMPlayThrough* refCon = static_cast<BGMPlayThrough*>(inClientData);

    // Stop this IOProc if the main thread has told us to
    if(refCon->mInputDeviceIOProcShouldStop)
    {
        refCon->mInputDevice.StopIOProc(refCon->mInputDeviceIOProcID);
        CAMemoryBarrier();
        refCon->mInputDeviceIOProcShouldStop = false;
        return noErr;
    }
    
    if(refCon->mFirstInputSampleTime == -1)
    {
        refCon->mFirstInputSampleTime = inInputTime->mSampleTime;
    }
    
    UInt32 framesToStore = inInputData->mBuffers[0].mDataByteSize / (SizeOf32(Float32) * 2);

    CARingBufferError err = refCon->mBuffer.Store(inInputData,
                                                  framesToStore,
                                                  static_cast<CARingBuffer::SampleTime>(inInputTime->mSampleTime));
    
    HandleRingBufferError(err, "InputDeviceIOProc", "mBuffer.Store");
    
    CAMemoryBarrier();
    refCon->mLastInputSampleTime = inInputTime->mSampleTime;
    
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
    #pragma unused (inDevice, inNow, inInputData, inInputTime, inOutputTime)
    
    // refCon (reference context) is the instance that created the IOProc
    BGMPlayThrough* refCon = static_cast<BGMPlayThrough*>(inClientData);
    
    // Stop this IOProc if the main thread has told us to
    if(refCon->mOutputDeviceIOProcShouldStop)
    {
        refCon->mOutputDevice.StopIOProc(refCon->mOutputDeviceIOProcID);
        CAMemoryBarrier();
        refCon->mOutputDeviceIOProcShouldStop = false;
        return noErr;
    }
    
    // Return early if we don't have any data to output yet
    if(refCon->mLastInputSampleTime == -1)
    {
        // TODO: Write silence to outOutputData here
        return noErr;
    }
    
    // If this is the first time this IOProc has been called since starting playthrough...
    if(refCon->mLastOutputSampleTime == -1)
    {
        // Calculate the number of frames between the read and write heads
        refCon->mInToOutSampleOffset = inOutputTime->mSampleTime - refCon->mLastInputSampleTime;
        
        // Log if we dropped frames
        if(refCon->mFirstInputSampleTime != refCon->mLastInputSampleTime)
        {
            DebugMsg("BGMPlayThrough::OutputDeviceIOProc: Dropped %f frames before output started. %s%f %s%f",
                     (refCon->mLastInputSampleTime - refCon->mFirstInputSampleTime),
                     "mFirstInputSampleTime=", refCon->mFirstInputSampleTime,
                     "mLastInputSampleTime=", refCon->mLastInputSampleTime);
        }
    }
    
    CARingBuffer::SampleTime readHeadSampleTime =
        static_cast<CARingBuffer::SampleTime>(inOutputTime->mSampleTime - refCon->mInToOutSampleOffset);
    CARingBuffer::SampleTime lastInputSampleTime =
        static_cast<CARingBuffer::SampleTime>(refCon->mLastInputSampleTime);
    
    UInt32 framesToOutput = outOutputData->mBuffers[0].mDataByteSize / (SizeOf32(Float32) * 2);
    
    // Very occasionally (at least for me) our read head gets ahead of input, i.e. we haven't received any new input since
    // this IOProc was last called, and we have to recalculate its position. I figure this might be caused by clock drift
    // but I'm really not sure. It also happens if the input or output sample times are restarted from zero.
    //
    // We also recalculate the offset if the read head is outside of the ring buffer. This happens for example when you plug
    // in or unplug headphones, which causes the output sample times to be restarted from zero.
    //
    // The vast majority of the time, just using lastInputSampleTime as the read head time instead of the one we calculate
    // would work fine (and would also account for the above).
    SInt64 bufferStartTime, bufferEndTime;
    CARingBufferError err = refCon->mBuffer.GetTimeBounds(bufferStartTime, bufferEndTime);
    bool outOfBounds = false;
    if(err == kCARingBufferError_OK)
    {
        outOfBounds = (readHeadSampleTime < bufferStartTime) || (readHeadSampleTime - framesToOutput > bufferEndTime);
    }
    if(lastInputSampleTime < readHeadSampleTime || outOfBounds)
    {
        DebugMsg("BGMPlayThrough::OutputDeviceIOProc: No input samples ready at output sample time. %s%lld %s%lld %s%f",
                 "lastInputSampleTime=", lastInputSampleTime,
                 "readHeadSampleTime=", readHeadSampleTime,
                 "mInToOutSampleOffset=", refCon->mInToOutSampleOffset);
        
        // Recalculate the in-to-out offset and read head
        refCon->mInToOutSampleOffset = inOutputTime->mSampleTime - lastInputSampleTime;
        readHeadSampleTime = static_cast<CARingBuffer::SampleTime>(inOutputTime->mSampleTime - refCon->mInToOutSampleOffset);
    }

    // Copy the frames from the ring buffer
    err = refCon->mBuffer.Fetch(outOutputData, framesToOutput, readHeadSampleTime);
    
    HandleRingBufferError(err, "OutputDeviceIOProc", "mBuffer.Fetch");
    
    refCon->mLastOutputSampleTime = inOutputTime->mSampleTime;
    
    return noErr;
}

// static
void    BGMPlayThrough::HandleRingBufferError(CARingBufferError inErr,
                                              const char* inMethodName,
                                              const char* inCallReturningErr)
{
#if DEBUG
    if(inErr != kCARingBufferError_OK)
    {
        const char* errStr = (inErr == kCARingBufferError_TooMuch ? "kCARingBufferError_TooMuch" :
                              (inErr == kCARingBufferError_CPUOverload ? "kCARingBufferError_CPUOverload" : "unknown error"));

        DebugMsg("BGMPlayThrough::%s: %s returned %s (%d)", inMethodName, inCallReturningErr, errStr, inErr);
        
        // kCARingBufferError_CPUOverload wouldn't mean we have a bug, but I think kCARingBufferError_TooMuch would
        if(inErr != kCARingBufferError_CPUOverload)
        {
            Throw(CAException(inErr));
        }
    }
#else
    // Not sure what we should do to handle these errors in release builds, if anything.
    // TODO: There's code in Apple's CAPlayThrough.cpp sample code that handles them. (Look for "kCARingBufferError_OK"
    //       around line 707.) Should be easy enough to use, but it's more complicated that just directly copying it.
    #pragma unused (inErr, inMethodName, inCallReturningErr)
#endif
}


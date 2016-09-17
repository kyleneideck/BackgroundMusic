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
#include "BGM_Utils.h"

// PublicUtility Includes
#include "CAAtomic.h"
#include "CAHALAudioSystemObject.h"

// STL Includes
#include <algorithm>  // For std::max

// System Includes
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/task.h>


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
    
    // Init the semaphore for the output IO Proc.
    kern_return_t theError = semaphore_create(mach_task_self(), &mOutputDeviceIOProcSemaphore, SYNC_POLICY_FIFO, 0);
    BGM_Utils::ThrowIfMachError("BGMPlayThrough::BGMPlayThrough", "semaphore_create", theError);
    
    ThrowIf(mOutputDeviceIOProcSemaphore == SEMAPHORE_NULL,
            CAException(kAudioHardwareUnspecifiedError),
            "BGMPlayThrough::BGMPlayThrough: Could not create semaphore");
}

BGMPlayThrough::~BGMPlayThrough()
{
    Deactivate();
    
    if(mOutputDeviceIOProcSemaphore != SEMAPHORE_NULL)
    {
        kern_return_t theError = semaphore_destroy(mach_task_self(), mOutputDeviceIOProcSemaphore);
        BGM_Utils::ThrowIfMachError("BGMPlayThrough::~BGMPlayThrough", "semaphore_destroy", theError);
    }
}

void    BGMPlayThrough::Swap(BGMPlayThrough &inPlayThrough)
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    bool wasPlayingThrough = inPlayThrough.mPlayingThrough;
    
    Deactivate();
    
    mInputDevice = inPlayThrough.mInputDevice;
    mOutputDevice = inPlayThrough.mOutputDevice;
    
    // Steal inPlayThrough's semaphore if this object needs one.
    if(mOutputDeviceIOProcSemaphore == SEMAPHORE_NULL)
    {
        mOutputDeviceIOProcSemaphore = inPlayThrough.mOutputDeviceIOProcSemaphore;
        inPlayThrough.mOutputDeviceIOProcSemaphore = SEMAPHORE_NULL;
    }
    
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
    Stop();
    
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
        
        // Just in case Stop() didn't reset these for some reason
        mInputDeviceIOProcShouldStop = false;
        mOutputDeviceIOProcShouldStop = false;
        CAMemoryBarrier();
        
        // Start our IOProcs
        Assert(mInputDeviceIOProcID != NULL && mOutputDeviceIOProcID != NULL, "BGMPlayThrough::Start: Null IO proc ID");
        mInputDevice.StartIOProc(mInputDeviceIOProcID);
        // mOutputDevice.SetIOBufferSize(512);
        mOutputDevice.StartIOProc(mOutputDeviceIOProcID);
        
        mPlayingThrough = true;
    }

    return noErr;
}

OSStatus    BGMPlayThrough::WaitForOutputDeviceToStart()
{
    // Check for errors.
    if(!mActive)
    {
        return kAudioHardwareNotRunningError;
    }
    if(!mOutputDevice.IsAlive())
    {
        return kAudioHardwareBadDeviceError;
    }
    
#if DEBUG
    UInt64 startedAt = mach_absolute_time();
#endif
    // Wait for our IO proc to start. mOutputDeviceIOProcSemaphore is reset to 0 (semaphore_signal_all) when our IO proc is
    // running on the output device.
    //
    // This does mean that we won't have any data the first time our IO proc is called, but I don't know any way to wait until
    // just before that point. (The device's IsRunning property changes immediately after we call StartIOProc.)
    kern_return_t theError = semaphore_wait(mOutputDeviceIOProcSemaphore);
    BGM_Utils::ThrowIfMachError("BGMPlayThrough::WaitForOutputDeviceToStart", "semaphore_wait", theError);

#if DEBUG
    UInt64 startedBy = mach_absolute_time();
    
    struct mach_timebase_info baseInfo = { 0, 0 };
    mach_timebase_info(&baseInfo);
    UInt64 base = baseInfo.numer / baseInfo.denom;
    
    DebugMsg("BGMPlayThrough::WaitForOutputDeviceToStart: Started %f ms after notification, %f ms after entering WaitForOutputDeviceToStart.",
             static_cast<Float64>(startedBy - mToldOutputDeviceToStartAt) * base / NSEC_PER_MSEC,
             static_cast<Float64>(startedBy - startedAt) * base / NSEC_PER_MSEC);
#endif
    
    return noErr;
}

OSStatus    BGMPlayThrough::Stop()
{
    CAMutex::Locker stateLocker(mStateMutex);
    
    if(mActive && mPlayingThrough)
    {
        DebugMsg("BGMPlayThrough::Stop: Stopping playthrough");
        
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
            //       cancelling the stop operation.
            struct timespec rmtp;
            int err = nanosleep((const struct timespec[]){{0, NSEC_PER_MSEC}}, &rmtp);
            totalWaitNs += NSEC_PER_MSEC - (err == -1 ? rmtp.tv_nsec : 0);
            CAMemoryBarrier();
        }
        
        // Clean up if the IOProcs didn't stop themselves
        if(mInputDeviceIOProcShouldStop && mInputDeviceIOProcID != NULL)
        {
            DebugMsg("BGMPlayThrough::Stop: The input IOProc didn't stop itself in time. Stopping it from outside of the IO thread.");
            mInputDevice.StopIOProc(mInputDeviceIOProcID);
            mInputDeviceIOProcShouldStop = false;
        }
        if(mOutputDeviceIOProcShouldStop && mOutputDeviceIOProcID != NULL)
        {
            DebugMsg("BGMPlayThrough::Stop: The output IOProc didn't stop itself in time. Stopping it from outside of the IO thread.");
            mOutputDevice.StopIOProc(mOutputDeviceIOProcID);
            mOutputDeviceIOProcShouldStop = false;
        }
        
        mPlayingThrough = false;
    }
    
    mFirstInputSampleTime = -1;
    mLastInputSampleTime = -1;
    mLastOutputSampleTime = -1;
    
    return noErr;
}

void    BGMPlayThrough::StopIfIdle()
{
    // To save CPU time, we stop playthrough when no clients are doing IO. This should reduce the coreaudiod and BGMApp
    // processes' idle CPU use to virtually none. If this isn't working for you, a client might be running IO without
    // being audible. VLC does that when you have a file paused, for example.
    
    CAMutex::Locker stateLocker(mStateMutex);
    
    Assert(IsBGMDevice(mInputDevice),
           "BGMDevice not set as input device. StopIfIdle can't tell if other devices are idle.");
    
    if(!RunningSomewhereOtherThanBGMApp(mInputDevice))
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
                       dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0),
                       ^{
                           // Check the BGMPlayThrough instance hasn't been destructed since it queued this block
                           if(mActive)
                           {
                               // The "2" is just to avoid shadowing the other locker
                               CAMutex::Locker stateLocker2(mStateMutex);
                               
                               // Don't stop playthrough if IO has started running again or if
                               // kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp has changed since
                               // this block was queued
                               if(mPlayingThrough && !RunningSomewhereOtherThanBGMApp(mInputDevice)
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
                {
                    DebugMsg("BGMPlayThrough::BGMDeviceListenerProc: Got kAudioDevicePropertyDeviceIsRunning notification");
                    
                    // This is dispatched because it can block and
                    //   - we might be on a real-time thread, or
                    //   - BGMXPCListener::waitForOutputDeviceToStartWithReply might get called on the same thread just
                    //     before this and timeout waiting for this to run.
                    //
                    // TODO: We should find a way to do this without dispatching because dispatching isn't real-time safe.
                    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
                        if(refCon->mActive)
                        {
                            CAMutex::Locker stateLocker(refCon->mStateMutex);
                            
                            // IsRunning doesn't always return true when IO is starting. Not sure why. But using
                            // RunningSomewhereOtherThanBGMApp instead seems to be working so far.
                            //
                            //if(refCon->mInputDevice->IsRunning())
                            if(RunningSomewhereOtherThanBGMApp(refCon->mInputDevice))
                            {
#if DEBUG
                                refCon->mToldOutputDeviceToStartAt = mach_absolute_time();
#endif
                                refCon->Start();
                            }
                        }
                    });
                }
                break;
                
            case kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp:
                DebugMsg("BGMPlayThrough::BGMDeviceListenerProc: Got "
                         "kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp notification");
                
                // These notifications don't need to be handled quickly, so we can always dispatch.
                dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
                    if(refCon->mActive)
                    {
                        refCon->StopIfIdle();
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
        Assert(refCon->mInputDeviceIOProcID != NULL, "BGMPlayThrough::InputDeviceIOProc: !mInputDeviceIOProcID");
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
        Assert(refCon->mOutputDeviceIOProcID != NULL, "BGMPlayThrough::OutputDeviceIOProc: !mOutputDeviceIOProcID");
        refCon->mOutputDevice.StopIOProc(refCon->mOutputDeviceIOProcID);
        CAMemoryBarrier();
        refCon->mOutputDeviceIOProcShouldStop = false;
        return noErr;
    }
    
    // Wake any threads waiting in WaitForOutputDeviceToStart, since the output device has finished starting up.
    kern_return_t theError = semaphore_signal_all(refCon->mOutputDeviceIOProcSemaphore);
    BGM_Utils::ThrowIfMachError("BGMPlayThrough::OutputDeviceIOProc", "semaphore_signal_all", theError);
    
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


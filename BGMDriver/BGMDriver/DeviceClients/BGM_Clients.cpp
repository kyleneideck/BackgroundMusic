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
//  BGM_Clients.cpp
//  BGMDriver
//
//  Copyright © 2016, 2017, 2019, 2025, 2026 Kyle Neideck
//  Copyright © 2017 Andrew Tonner
//

// Self Include
#include "BGM_Clients.h"

// Local Includes
#include "BGM_Types.h"
#include "BGM_Utils.h"
#include "BGM_PlugIn.h"

// PublicUtility Includes
#include "CAException.h"
#include "CACFDictionary.h"
#include "CADispatchQueue.h"
#include "CAHostTimeBase.h"

// System Includes
#include <mach/mach_time.h>


#pragma mark Construction/Destruction

BGM_Clients::BGM_Clients(AudioObjectID inOwnerDeviceID, BGM_TaskQueue* inTaskQueue)
:
    mOwnerDeviceID(inOwnerDeviceID),
    mClientMap(inTaskQueue)
{
    mRelativeVolumeCurve.AddRange(kAppRelativeVolumeMinRawValue,
                                  kAppRelativeVolumeMaxRawValue,
                                  kAppRelativeVolumeMinDbValue,
                                  kAppRelativeVolumeMaxDbValue);
}

#pragma mark Add/Remove Clients

void    BGM_Clients::AddClient(BGM_Client inClient)
{
    CAMutex::Locker theLocker(mMutex);

    // Check whether this is the music player's client
    bool pidMatchesMusicPlayerProperty =
        (mMusicPlayerProcessIDProperty != 0 && inClient.mProcessID == mMusicPlayerProcessIDProperty);
    bool bundleIDMatchesMusicPlayerProperty =
        inClient.BundleIDMatchesMusicPlayer(mMusicPlayerBundleIDProperty);
    
    inClient.mIsMusicPlayer = (pidMatchesMusicPlayerProperty || bundleIDMatchesMusicPlayerProperty);
    
    if(inClient.mIsMusicPlayer)
    {
        DebugMsg("BGM_Clients::AddClient: Adding music player client. mClientID = %u", inClient.mClientID);
    }
    
    mClientMap.AddClient(inClient);
    
    // If we're adding BGMApp, update our local copy of its client ID
    if(inClient.mBundleID.IsValid() && inClient.mBundleID == kBGMAppBundleID)
    {
        mBGMAppClientID.store(inClient.mClientID, std::memory_order_relaxed);
    }
}

void    BGM_Clients::RemoveClient(const UInt32 inClientID)
{
    CAMutex::Locker theLocker(mMutex);
    
    BGM_Client theRemovedClient = mClientMap.RemoveClient(inClientID);
    
    // If we're removing BGMApp, clear our local copy of its client ID
    if(theRemovedClient.mClientID == static_cast<UInt32>(mBGMAppClientID.load(std::memory_order_relaxed)))
    {
        mBGMAppClientID.store(-1, std::memory_order_relaxed);
    }
}

#pragma mark IO Status

// TODO: We could remove kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp and all of the code that
//       manages it (the functions below, BGM_Device::mInactivityTimer, etc.) if we split BGMDevice into a separate
//       input device and output device. BGMApp doesn't have an IOProc for BGMDevice's output device/stream, just its
//       input, so it would be able to use kAudioDevicePropertyDeviceIsRunningSomewhere to tell when apps are playing
//       audio to the output BGMDevice.
//
//       Splitting BGMDevice would also get rid of the microphone permissions dialogs that appear on macOS Tahoe when
//       some other apps play audio to BGMDevice. I'd guess those apps are doing something like enumerating all streams
//       (not just output ones) and requesting info about the input stream even though they don't need it, and that
//       triggers the permission prompt.

bool    BGM_Clients::ClientsOtherThanBGMAppRunningIO() const noexcept
{
    // 2 seconds in mach ticks
    static const uint64_t kThreshold = CAHostTimeBase::ConvertFromNanos(2ULL * NSEC_PER_SEC);

    const uint64_t lastNonBGMAppIOTime = mLastNonBGMAppIOTime.load(std::memory_order_relaxed);
    return lastNonBGMAppIOTime != 0 && mach_absolute_time() - lastNonBGMAppIOTime < kThreshold;
}

void    BGM_Clients::RecordNonBGMAppIO(const UInt32 inClientID) noexcept
{
    static_assert(ATOMIC_LLONG_LOCK_FREE == 2, "std::atomic<uint64_t> must be lock-free for RT safety");

    // RT-safe: only uses atomics.
    if(static_cast<SInt64>(inClientID) != mBGMAppClientID.load(std::memory_order_relaxed))
    {
        mLastNonBGMAppIOTime.store(mach_absolute_time(), std::memory_order_relaxed);
    }
}

void    BGM_Clients::SendIORunningPropertyNotification() noexcept
{
    // Send a property change notification if the property has changed since the last time we sent a notification.
    const bool isRunning = ClientsOtherThanBGMAppRunningIO();
    const bool wasRunning = mLastNotifiedIsRunning.exchange(isRunning, std::memory_order_relaxed);

    if(isRunning != wasRunning)
    {
        DebugMsg("BGM_Clients::SendIORunningPropertyNotification: isRunningSomewhereOtherThanBGMApp changed to %s",
                 isRunning ? "true" : "false");
        CADispatchQueue::GetGlobalSerialQueue().Dispatch(false, ^{
            AudioObjectPropertyAddress theAddr = kBGMRunningSomewhereOtherThanBGMAppAddress;
            BGM_PlugIn::Host_PropertiesChanged(mOwnerDeviceID, 1, &theAddr);
        });
    }
}

bool    BGM_Clients::StartIO(UInt32 inClientID)
{
    CAMutex::Locker theLocker(mMutex);

    ThrowIf(mStartCount == UINT64_MAX,
            CAException(kAudioHardwareIllegalOperationError),
            "BGM_Clients::StartIO: mStartCount overflow");

    // Update the kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp property.
    if(!IsBGMApp(inClientID))
    {
        RecordNonBGMAppIO(inClientID);
        SendIORunningPropertyNotification();
    }

    const bool isFirstClient = (mStartCount == 0);
    ++mStartCount;

    if(isFirstClient)
    {
        // Start a periodic timer (500 ms interval) to send notifications for
        // kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp when it changes.
        BGMAssert(mInactivityTimer == nullptr, "BGM_Clients::StartIO: Inactivity timer already running");
        dispatch_queue_t theTimerQueue = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);
        dispatch_source_t __nonnull theTimer =
            dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, theTimerQueue);
        dispatch_source_set_timer(theTimer,
                                  dispatch_time(DISPATCH_TIME_NOW, 500 * NSEC_PER_MSEC),
                                  500 * NSEC_PER_MSEC,
                                  100 * NSEC_PER_MSEC);
        dispatch_source_set_event_handler(theTimer, ^{
            BGMLogAndSwallowExceptions("BGM_Clients::StartIO inactivityTimer", ([&] {
                CAMutex::Locker theTimerLocker(mMutex);
                if(mInactivityTimer == theTimer && mStartCount > 0)
                {
                    SendIORunningPropertyNotification();
                }
            }));
        });
        dispatch_resume(theTimer);
        mInactivityTimer = theTimer;
    }

    return isFirstClient;
}

bool    BGM_Clients::StopIO()
{
    CAMutex::Locker theLocker(mMutex);

    ThrowIf(mStartCount == 0,
            CAException(kAudioHardwareIllegalOperationError),
            "BGM_Clients::StopIO: mStartCount underflow");

    --mStartCount;
    const bool isLastClient = (mStartCount == 0);

    if(isLastClient)
    {
        if(mInactivityTimer != nullptr)
        {
            dispatch_source_cancel(BGM_Utils::NN(mInactivityTimer));
            dispatch_release(BGM_Utils::NN(mInactivityTimer));
            mInactivityTimer = nullptr;
        }

        mLastNonBGMAppIOTime.store(0, std::memory_order_relaxed);

        // Send a property change notification if the property was true the last time we sent a notification.
        const bool wasRunning = mLastNotifiedIsRunning.exchange(false, std::memory_order_relaxed);

        if(wasRunning)
        {
            DebugMsg("BGM_Clients::StopIO: isRunningSomewhereOtherThanBGMApp changed to false");
            CADispatchQueue::GetGlobalSerialQueue().Dispatch(false, ^{
                AudioObjectPropertyAddress theAddr = kBGMRunningSomewhereOtherThanBGMAppAddress;
                BGM_PlugIn::Host_PropertiesChanged(mOwnerDeviceID, 1, &theAddr);
            });
        }
    }

    return isLastClient;
}

bool    BGM_Clients::IsIORunning() const
{
    CAMutex::Locker theLocker(mMutex);
    return mStartCount > 0;
}

#pragma mark Music Player

bool    BGM_Clients::SetMusicPlayer(const pid_t inPID)
{
    ThrowIf(inPID < 0, BGM_InvalidClientPIDException(), "BGM_Clients::SetMusicPlayer: Invalid music player PID");
    
    CAMutex::Locker theLocker(mMutex);
    
    if(mMusicPlayerProcessIDProperty == inPID)
    {
        // We're not changing the properties, so return false
        return false;
    }
    
    mMusicPlayerProcessIDProperty = inPID;
    // Unset the bundle ID property
    mMusicPlayerBundleIDProperty = "";
    
    DebugMsg("BGM_Clients::SetMusicPlayer: Setting music player by PID. inPID=%d", inPID);
    
    // Update the clients' mIsMusicPlayer fields
    mClientMap.UpdateMusicPlayerFlags(inPID);
    
    return true;
}

bool    BGM_Clients::SetMusicPlayer(const CACFString inBundleID)
{
    Assert(inBundleID.IsValid(), "BGM_Clients::SetMusicPlayer: Invalid CACFString given as bundle ID");
    
    CAMutex::Locker theLocker(mMutex);
    
    if(mMusicPlayerBundleIDProperty == inBundleID)
    {
        // We're not changing the properties, so return false
        return false;
    }
    
    mMusicPlayerBundleIDProperty = inBundleID;
    // Unset the PID property
    mMusicPlayerProcessIDProperty = 0;
    
    DebugMsg("BGM_Clients::SetMusicPlayer: Setting music player by bundle ID. inBundleID=%s",
             CFStringGetCStringPtr(inBundleID.GetCFString(), kCFStringEncodingUTF8));
    
    // Update the clients' mIsMusicPlayer fields
    mClientMap.UpdateMusicPlayerFlags(inBundleID);
    
    return true;
}

bool    BGM_Clients::IsMusicPlayerRT(const UInt32 inClientID) const
{
    BGM_Client theClient;
    bool didGetClient = mClientMap.GetClientRT(inClientID, &theClient);
    return didGetClient && theClient.mIsMusicPlayer;
}

#pragma mark App Volumes

Float32 BGM_Clients::GetClientRelativeVolumeRT(UInt32 inClientID) const
{
    BGM_Client theClient;
    bool didGetClient = mClientMap.GetClientRT(inClientID, &theClient);
    return (didGetClient ? theClient.mRelativeVolume : 1.0f);
}

SInt32 BGM_Clients::GetClientPanPositionRT(UInt32 inClientID) const
{
    BGM_Client theClient;
    bool didGetClient = mClientMap.GetClientRT(inClientID, &theClient);
    return (didGetClient ? theClient.mPanPosition : kAppPanCenterRawValue);
}

bool    BGM_Clients::SetClientsRelativeVolumes(const CACFArray inAppVolumes)
{
    bool didChangeAppVolumes = false;
    
    // Each element in appVolumes is a CFDictionary containing the process id and/or bundle id of an app, and its
    // new relative volume
    for(UInt32 i = 0; i < inAppVolumes.GetNumberItems(); i++)
    {
        CACFDictionary theAppVolume(false);
        inAppVolumes.GetCACFDictionary(i, theAppVolume);
        
        // Get the app's PID from the dict
        pid_t theAppPID;
        bool didFindPID = theAppVolume.GetSInt32(CFSTR(kBGMAppVolumesKey_ProcessID), theAppPID);

        // Get the app's bundle ID from the dict
        CACFString theAppBundleID;
        theAppBundleID.DontAllowRelease();
        theAppVolume.GetCACFString(CFSTR(kBGMAppVolumesKey_BundleID), theAppBundleID);

        BGMAssert(didFindPID || theAppBundleID.IsValid(),
                  "BGM_Clients::SetClientsRelativeVolumes: No PID or bundle ID");
        (void)didFindPID;  // Suppress unused variable warning in release builds.

        bool didGetVolume;
        {
            SInt32 theRawRelativeVolume;
            didGetVolume = theAppVolume.GetSInt32(CFSTR(kBGMAppVolumesKey_RelativeVolume), theRawRelativeVolume);

            if (didGetVolume) {
                // Apply the volume curve to the raw volume
                //
                // mRelativeVolumeCurve uses the default kPow2Over1Curve transfer function, so we also multiply by 4 to
                // keep the middle volume equal to 1 (meaning apps' volumes are unchanged by default).
                Float32 theRelativeVolume = mRelativeVolumeCurve.ConvertRawToScalar(theRawRelativeVolume) * 4;

                // Try to update the client's volume, first by PID and then by bundle ID. Always try
                // both because apps can have multiple clients.
                if(mClientMap.SetClientsRelativeVolume(theAppPID, theRelativeVolume))
                {
                    didChangeAppVolumes = true;
                }

                if(mClientMap.SetClientsRelativeVolume(theAppBundleID, theRelativeVolume))
                {
                    didChangeAppVolumes = true;
                }

                // TODO: If the app isn't currently a client, we should add it to the past clients
                //       map, or update its past volume if it's already in there.
            }
        }
        
        bool didGetPanPosition;
        {
            SInt32 thePanPosition;
            didGetPanPosition = theAppVolume.GetSInt32(CFSTR(kBGMAppVolumesKey_PanPosition), thePanPosition);
            if (didGetPanPosition) {
                if(mClientMap.SetClientsPanPosition(theAppPID, thePanPosition))
                {
                    didChangeAppVolumes = true;
                }

                if(mClientMap.SetClientsPanPosition(theAppBundleID, thePanPosition))
                {
                    didChangeAppVolumes = true;
                }

                // TODO: If the app isn't currently a client, we should add it to the past clients
                //       map, or update its past pan position if it's already in there.
            }
        }
        
        ThrowIf(!didGetVolume && !didGetPanPosition,
                BGM_InvalidClientRelativeVolumeException(),
                "BGM_Clients::SetClientsRelativeVolumes: No volume or pan position in request");
    }
    
    return didChangeAppVolumes;
}


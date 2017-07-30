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
//  BGM_Clients.h
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//

#ifndef __BGMDriver__BGM_Clients__
#define __BGMDriver__BGM_Clients__

// Local Includes
#include "BGM_Client.h"
#include "BGM_ClientMap.h"

// PublicUtility Includes
#include "CAVolumeCurve.h"
#include "CAMutex.h"
#include "CACFArray.h"

// System Includes
#include <CoreAudio/AudioServerPlugIn.h>


// Forward Declations
class BGM_ClientTasks;


#pragma clang assume_nonnull begin

//==================================================================================================
//	BGM_Clients
//
//  Holds information about the clients (of the host) of the BGMDevice, i.e. the apps registered
//  with the HAL, generally so they can do IO at some point. BGMApp and the music player are special
//  case clients.
//
//  Methods whose names end with "RT" should only be called from real-time threads.
//==================================================================================================

class BGM_Clients
{
    
    friend class BGM_ClientTasks;
    
public:
                                        BGM_Clients(AudioObjectID inOwnerDeviceID, BGM_TaskQueue* inTaskQueue);
                                        ~BGM_Clients() = default;
    // Disallow copying. (It could make sense to implement these in future, but we don't need them currently.)
                                        BGM_Clients(const BGM_Clients&) = delete;
                                        BGM_Clients& operator=(const BGM_Clients&) = delete;
    
    void                                AddClient(BGM_Client inClient);
    void                                RemoveClient(const UInt32 inClientID);
    
private:
    // Only BGM_TaskQueue is allowed to call these (through the BGM_ClientTasks interface). We get notifications
    // from the HAL when clients start/stop IO and they have to be processed in the order we receive them to
    // avoid race conditions. If these methods could be called directly those calls would skip any queued calls.
    bool                                StartIONonRT(UInt32 inClientID);
    bool                                StopIONonRT(UInt32 inClientID);

public:
    bool                                ClientsRunningIO() const;
    bool                                ClientsOtherThanBGMAppRunningIO() const;
    
private:
    void                                SendIORunningNotifications(bool sendIsRunningNotification, bool sendIsRunningSomewhereOtherThanBGMAppNotification) const;
public:
    bool                                IsBGMApp(UInt32 inClientID) const { return inClientID == mBGMAppClientID; }
    bool                                BGMAppHasClientRegistered() const { return mBGMAppClientID != -1; }
    
    inline pid_t                        GetMusicPlayerProcessIDProperty() const { return mMusicPlayerProcessIDProperty; }
    inline CFStringRef                  CopyMusicPlayerBundleIDProperty() const { return mMusicPlayerBundleIDProperty.CopyCFString(); }
    
    // Returns true if the PID was changed
    bool                                SetMusicPlayer(const pid_t inPID);
    // Returns true if the bundle ID was changed
    bool                                SetMusicPlayer(const CACFString inBundleID);
    
    bool                                IsMusicPlayerRT(const UInt32 inClientID) const;
    
    Float32                             GetClientRelativeVolumeRT(UInt32 inClientID) const;
    SInt32                              GetClientPanPositionRT(UInt32 inClientID) const;
    
    // Copies the current and past clients into an array in the format expected for
    // kAudioDeviceCustomPropertyAppVolumes. (Except that CACFArray and CACFDictionary are used instead
    // of unwrapped CFArray and CFDictionary refs.)
    CACFArray                           CopyClientRelativeVolumesAsAppVolumes() const { return mClientMap.CopyClientRelativeVolumesAsAppVolumes(mRelativeVolumeCurve); };
    
    // inAppVolumes is an array of dicts with the keys kBGMAppVolumesKey_ProcessID,
    // kBGMAppVolumesKey_BundleID and optionally kBGMAppVolumesKey_RelativeVolume and
    // kBGMAppVolumesKey_PanPosition. This method finds the client for
    // each app by PID or bundle ID, sets the volume and applies mRelativeVolumeCurve to it.
    //
    // Returns true if any clients' relative volumes were changed.
    bool                                SetClientsRelativeVolumes(const CACFArray inAppVolumes);
    
private:
    AudioObjectID                       mOwnerDeviceID;
    BGM_ClientMap                       mClientMap;
    
    // Counters for the number of clients that are doing IO. These are used to tell whether any clients
    // are currently doing IO without having to check every client's mDoingIO.
    //
    // We need to reference count this rather than just using a bool because the HAL might (but usually
    // doesn't) call our StartIO/StopIO functions for clients other than the first to start and last to
    // stop.
    UInt64                              mStartCount = 0;
    UInt64                              mStartCountExcludingBGMApp = 0;
    
    CAMutex                             mMutex { "Clients" };
    
    SInt64                              mBGMAppClientID = -1;
    
    // The value of the kAudioDeviceCustomPropertyMusicPlayerProcessID property, or 0 if it's unset/null.
    // We store this separately because the music player might not always be a client, but could be added
    // as one at a later time.
    pid_t                               mMusicPlayerProcessIDProperty = 0;
    
    // The value of the kAudioDeviceCustomPropertyMusicPlayerBundleID property, or the empty string it it's
    // unset/null. UTF8 encoding.
    //
    // As with mMusicPlayerProcessID, we keep a copy of the bundle ID the user sets for the music player
    // because there might be no client with that bundle ID. In that case we need to be able to give the
    // property's value if the HAL asks for it, and to recognise the music player if it's added a client.
    CACFString                          mMusicPlayerBundleIDProperty { "" };
    
    // The volume curve we apply to raw client volumes before they're used
    CAVolumeCurve                       mRelativeVolumeCurve;
    
};

#pragma clang assume_nonnull end

#endif /* __BGMDriver__BGM_Clients__ */


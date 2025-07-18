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
//  BGMAutoPauseMusic.m
//  BGMApp
//
//  Copyright © 2016, 2017 Kyle Neideck
//

// Self Include
#import "BGMAutoPauseMusic.h"

// Local Includes
#include "BGM_Types.h"
#import "BGMMusicPlayer.h"

// STL Includes
#import <algorithm>  // std::max, std::min

// System Includes
#include <CoreAudio/AudioHardware.h>
#include <mach/mach_time.h>


// We multiply the time spent paused by this factor to calculate the delay before we consider unpausing.
static Float32 const kUnpauseDelayWeightingFactor = 0.1f;

@implementation BGMAutoPauseMusic {
    BOOL enabled;
    
    BGMAudioDeviceManager* audioDevices;
    BGMMusicPlayers* musicPlayers;
    BGMUserDefaults* userDefaults;
    
    dispatch_queue_t listenerQueue;
    // Have to keep track of the listener block we add so we can remove it later.
    AudioObjectPropertyListenerBlock listenerBlock;
    
    dispatch_queue_t pauseUnpauseMusicQueue;
    
    // True if BGMApp has paused musicPlayer and hasn't unpaused it yet. (Will be out of sync with the music player app if the
    // user has unpaused it themselves.)
    BOOL wePaused;
    // The times, in absolute time, that the BGMDevice last changed its audible state to silent...
    UInt64 wentSilent;
    // ...and to audible.
    UInt64 wentAudible;
}

- (id) initWithAudioDevices:(BGMAudioDeviceManager*)inAudioDevices musicPlayers:(BGMMusicPlayers*)inMusicPlayers userDefaults:(BGMUserDefaults*)inUserDefaults {
    if ((self = [super init])) {
        audioDevices = inAudioDevices;
        musicPlayers = inMusicPlayers;
        userDefaults = inUserDefaults;
        
        enabled = NO;
        wePaused = NO;
        
        dispatch_queue_attr_t attr;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
        if (&dispatch_queue_attr_make_with_qos_class) {
            attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_DEFAULT, 0);
        } else {
            // OS X 10.9 fallback
            attr = DISPATCH_QUEUE_SERIAL;
        }
#pragma clang diagnostic pop

        listenerQueue = dispatch_queue_create("com.bearisdriving.BGM.AutoPauseMusic.Listener", attr);
        pauseUnpauseMusicQueue = dispatch_queue_create("com.bearisdriving.BGM.AutoPauseMusic.PauseUnpauseMusic", attr);
        
        [self initListenerBlock];
    }
    
    return self;
}

- (void) dealloc {
    [self disable];
}

- (void) initListenerBlock {
    // To avoid retain cycle
    __unsafe_unretained BGMAutoPauseMusic* weakSelf = self;
    
    listenerBlock = ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress * _Nonnull inAddresses) {
        // inAddresses "may contain addresses for properties for which the listener is not signed up to receive notifications",
        // so we have to check them all
        for (int i = 0; i < inNumberAddresses; i++) {
            if (inAddresses[i].mSelector == kAudioDeviceCustomPropertyDeviceAudibleState) {
                BGMDeviceAudibleState audibleState = [weakSelf deviceAudibleState];
                
#if DEBUG
                const char audibleStateStr[5] = CA4CCToCString(audibleState);
                DebugMsg("BGMAutoPauseMusic::initListenerBlock: kAudioDeviceCustomPropertyDeviceAudibleState property changed to '%s'",
                         audibleStateStr);
#endif
                
                // TODO: We shouldn't assume this block will only get called when BGMDevice's audible state changes. (Even if
                //       the Core Audio docs did specify that, there's no reason not to be fault tolerant.)
                if (audibleState == kBGMDeviceIsAudible) {
                    [weakSelf queuePauseBlock];
                } else if (audibleState == kBGMDeviceIsSilent) {
                    [weakSelf queueUnpauseBlock];
                } else if (audibleState == kBGMDeviceIsSilentExceptMusic) {
                    // If we pause the music player and then the user unpauses it before the other audio stops, we need to set
                    // wePaused to false at some point before the other audio starts again so we know we should pause
                    DebugMsg("BGMAutoPauseMusic: Device is silent except music, resetting wePaused flag");
                    wePaused = NO;
                }
                // TODO: Add a fourth audible state, something like "AudibleAndMusicPlaying", and check it here to
                //       handle the user unpausing and then repausing music while also playing other audio?
            }
        }
    };
}

- (BGMDeviceAudibleState) deviceAudibleState {
    return [audioDevices bgmDevice].GetAudibleState();
}

- (void) queuePauseBlock {
    UInt64 now = mach_absolute_time();
    wentAudible = now;
    UInt64 startedPauseDelay = now;
    
    UInt64 pauseDelayMS = userDefaults.pauseDelayMS;
    
    // If pause delay is 0, pause immediately (no delay)
    if (pauseDelayMS == 0) {
        DebugMsg("BGMAutoPauseMusic::queuePauseBlock: Pause delay is 0, pausing immediately");
        
        // Pause immediately if device is audible and we haven't already paused
        if (!wePaused && ([self deviceAudibleState] == kBGMDeviceIsAudible)) {
            wePaused = ([musicPlayers.selectedMusicPlayer pause] || wePaused);
        }
        return;
    }
    
    UInt64 pauseDelayNSec = pauseDelayMS * NSEC_PER_MSEC;
    
    DebugMsg("BGMAutoPauseMusic::queuePauseBlock: Dispatching pause block at %llu", now);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, pauseDelayNSec),
                   pauseUnpauseMusicQueue,
                   ^{
                       BOOL stillAudible = ([self deviceAudibleState] == kBGMDeviceIsAudible);
                       
                       DebugMsg("BGMAutoPauseMusic::queuePauseBlock: Running pause block dispatched at %llu.%s wentAudible=%llu",
                                startedPauseDelay,
                                stillAudible ? "" : " Not pausing because the device isn't audible.",
                                wentAudible);
                       
                       // Pause if this is the most recent pause block and the device is still audible, which means the audible
                       // state hasn't changed since this block was queued. Also set wePaused to true if the player wasn't
                       // already paused.
                       if (!wePaused && (startedPauseDelay == wentAudible) && stillAudible) {
                           wePaused = ([musicPlayers.selectedMusicPlayer pause] || wePaused);
                       }
                   });
}

- (void) queueUnpauseBlock {
    UInt64 now = mach_absolute_time();
    wentSilent = now;
    UInt64 startedUnpauseDelay = now;
    
    // Get user-configurable max delay
    UInt64 maxUnpauseDelayMS = userDefaults.maxUnpauseDelayMS;
    
    // If max unpause delay is 0, unpause immediately (no delay)
    if (maxUnpauseDelayMS == 0) {
        DebugMsg("BGMAutoPauseMusic::queueUnpauseBlock: Max unpause delay is 0, unpausing immediately");
        
        // Unpause immediately if we were the one who paused and device is still silent
        BGMDeviceAudibleState currentState = [self deviceAudibleState];
        DebugMsg("BGMAutoPauseMusic::queueUnpauseBlock: Immediate unpause - wePaused=%s, currentState=%s", 
                 wePaused ? "YES" : "NO", 
                 currentState == kBGMDeviceIsSilent ? "Silent" : 
                 (currentState == kBGMDeviceIsAudible ? "Audible" : "SilentExceptMusic"));
        
        if (wePaused && (currentState == kBGMDeviceIsSilent)) {
            wePaused = NO;
            [musicPlayers.selectedMusicPlayer unpause];
        }
        return;
    }
    
    // Unpause sooner if we've only been paused for a short time. This is so a notification sound causing an auto-pause is
    // less of an interruption.
    //
    // TODO: Fading in and out would make short pauses a lot less jarring because, if they were short enough, we wouldn't
    //       actually pause the music player. So you'd hear a dip in the music's volume rather than a gap.
    UInt64 unpauseDelayNsec =
        static_cast<UInt64>((wentSilent - wentAudible) * kUnpauseDelayWeightingFactor);
    
    // Convert from absolute time to nanos.
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    unpauseDelayNsec = unpauseDelayNsec * info.numer / info.denom;
    
    // Clamp using user-configurable max delay and calculated min delay.
    UInt64 maxUnpauseDelayNSec = maxUnpauseDelayMS * NSEC_PER_MSEC;
    UInt64 minUnpauseDelayNSec = maxUnpauseDelayNSec / 10;
    unpauseDelayNsec = std::min(maxUnpauseDelayNSec, unpauseDelayNsec);
    unpauseDelayNsec = std::max(minUnpauseDelayNSec, unpauseDelayNsec);
    
    DebugMsg("BGMAutoPauseMusic::queueUnpauseBlock: Dispatched unpause block at %llu. unpauseDelayNsec=%llu",
             now,
             unpauseDelayNsec);
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, unpauseDelayNsec),
                   pauseUnpauseMusicQueue,
                   ^{
                       BGMDeviceAudibleState currentState = [self deviceAudibleState];
                       BOOL stillSilent = (currentState == kBGMDeviceIsSilent);
                       BOOL isLatestUnpause = (startedUnpauseDelay == wentSilent);
                       
                       DebugMsg("BGMAutoPauseMusic::queueUnpauseBlock: Running unpause block dispatched at %llu. wePaused=%s, isLatest=%s, currentState=%s, wentSilent=%llu",
                                startedUnpauseDelay,
                                wePaused ? "YES" : "NO",
                                isLatestUnpause ? "YES" : "NO",
                                currentState == kBGMDeviceIsSilent ? "Silent" : 
                                (currentState == kBGMDeviceIsAudible ? "Audible" : "SilentExceptMusic"),
                                wentSilent);
                       
                       // Unpause if we were the one who paused. Also check that this is the most recent unpause block and the
                       // device is still silent, which means the audible state hasn't changed since this block was queued.
                       if (wePaused && isLatestUnpause && stillSilent) {
                           DebugMsg("BGMAutoPauseMusic::queueUnpauseBlock: Unpausing music player");
                           wePaused = NO;
                           [musicPlayers.selectedMusicPlayer unpause];
                       }
                   });
}

- (void) enable {
    if (!enabled) {
        [audioDevices bgmDevice].AddPropertyListenerBlock(kBGMAudibleStateAddress, listenerQueue, listenerBlock);
        enabled = YES;
    }
}

- (void) disable {
    if (enabled) {
        [audioDevices bgmDevice].RemovePropertyListenerBlock(kBGMAudibleStateAddress, listenerQueue, listenerBlock);
        enabled = NO;
    }
}

@end


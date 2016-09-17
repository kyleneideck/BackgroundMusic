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
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#import "BGMAutoPauseMusic.h"

// Local Includes
#include "BGM_Types.h"
#import "BGMMusicPlayer.h"

// System Includes
#include <CoreAudio/AudioHardware.h>
#include <mach/mach_time.h>


// How long to wait before pausing/unpausing. This is so short sounds can play without awkwardly causing a short period of silence,
// and other audio can have short periods of silence without causing music to play and quickly pause again. Of course, it's a
// trade-off against how long the music will overlap the other audio before it gets paused and how long the music will stay paused
// after a sound that was only slightly longer than the pause delay.
//
// TODO: Make these settable in advanced settings?
static int const kPauseDelayMSecs = 1500;
static int const kUnpauseDelayMSecs = 3000;

@implementation BGMAutoPauseMusic {
    BOOL enabled;
    
    BGMAudioDeviceManager* audioDevices;
    BGMMusicPlayers* musicPlayers;
    
    dispatch_queue_t listenerQueue;
    // Have to keep track of the listener block we add so we can remove it later.
    AudioObjectPropertyListenerBlock listenerBlock;
    
    dispatch_queue_t pauseUnpauseMusicQueue;
    
    // True if BGMApp has paused musicPlayer and hasn't unpaused it yet. (Will be out of sync with the music player app if the user
    // has unpaused it themselves.)
    BOOL wePaused;
    // The times, in absolute time, that the BGMDevice last changed its audible state to silent...
    UInt64 wentSilent;
    // ...and to audible.
    UInt64 wentAudible;
}

- (id) initWithAudioDevices:(BGMAudioDeviceManager*)inAudioDevices musicPlayers:(BGMMusicPlayers*)inMusicPlayers {
    if ((self = [super init])) {
        audioDevices = inAudioDevices;
        musicPlayers = inMusicPlayers;
        
        enabled = NO;
        wePaused = NO;
        
        dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_DEFAULT, 0);
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
                SInt32 audibleState = [weakSelf deviceAudibleState];
                
#if DEBUG
                const char audibleStateStr[5] = CA4CCToCString(audibleState);
                DebugMsg("BGMAutoPauseMusic::initListenerBlock: kAudioDeviceCustomPropertyDeviceAudibleState property changed to '%s'",
                         audibleStateStr);
#endif
                
                if (audibleState == kBGMDeviceIsAudible) {
                    [weakSelf queuePauseBlock];
                } else if (audibleState == kBGMDeviceIsSilent) {
                    [weakSelf queueUnpauseBlock];
                } else if (audibleState == kBGMDeviceIsSilentExceptMusic) {
                    // If we pause the music player and then the user unpauses it before the other audio stops, we need to set
                    // wePaused to false at some point before the other audio starts again so we know we should pause
                    wePaused = NO;
                }
                // TODO: Add a fourth audible state, something like "AudibleAndMusicPlaying", and check it here to
                //       handle the user unpausing and then repausing music while also playing other audio?
            }
        }
    };
}

- (SInt32) deviceAudibleState {
    SInt32 audibleState;
    CFNumberRef audibleStateRef = static_cast<CFNumberRef>([audioDevices bgmDevice].GetPropertyData_CFType(kBGMAudibleStateAddress));
    CFNumberGetValue(audibleStateRef, kCFNumberSInt32Type, &audibleState);
    CFRelease(audibleStateRef);
    return audibleState;
}

- (void) queuePauseBlock {
    UInt64 now = mach_absolute_time();
    wentAudible = now;
    UInt64 startedPauseDelay = now;
    
    DebugMsg("BGMAutoPauseMusic::queuePauseBlock: Dispatching pause block at %llu", now);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, kPauseDelayMSecs * NSEC_PER_MSEC),
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
    
    DebugMsg("BGMAutoPauseMusic::queueUnpauseBlock: Dispatched unpause block at %llu", now);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, kUnpauseDelayMSecs * NSEC_PER_MSEC),
                   pauseUnpauseMusicQueue,
                   ^{
                       BOOL stillSilent = ([self deviceAudibleState] == kBGMDeviceIsSilent);
                       
                       DebugMsg("BGMAutoPauseMusic::queueUnpauseBlock: Running unpause block dispatched at %llu.%s%s wentSilent=%llu",
                                startedUnpauseDelay,
                                wePaused ? "" : " Not unpausing because we weren't the one who paused.",
                                stillSilent ? "" : " Not unpausing because the device isn't silent.",
                                wentSilent);
                       
                       // Unpause if we were the one who paused. Also check that this is the most recent unpause block and the
                       // device is still silent, which means the audible state hasn't changed since this block was queued.
                       if (wePaused && (startedUnpauseDelay == wentSilent) && stillSilent) {
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


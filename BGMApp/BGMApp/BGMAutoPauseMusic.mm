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
//  Copyright © 2016, 2017, 2026 Kyle Neideck
//

// Self Include
#import "BGMAutoPauseMusic.h"

// Local Includes
#import "BGM_Types.h"
#import "BGMMusicPlayer.h"
#import "BGM_Utils.h"
#import "CACFArray.h"
#import "CACFDictionary.h"
#import "CACFString.h"

// STL Includes
#import <algorithm>  // std::max, std::min
#include <cmath>


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
    // True if BGMApp has ducked the musicPlayer and hasn't unducked it yet.
    BOOL weDucked;
    // The original volume before ducking.
    int originalVolume;
    // The ducked volume we set.
    int duckedVolume;
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
        weDucked = NO;
        originalVolume = 50;
        duckedVolume = 50;
        
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
        
        [userDefaults addObserver:self
                       forKeyPath:@"autoDuckMusic"
                          options:NSKeyValueObservingOptionNew
                          context:nil];
        [userDefaults addObserver:self
                       forKeyPath:@"autoDuckPercent"
                          options:NSKeyValueObservingOptionNew
                          context:nil];
        [musicPlayers addObserver:self
                       forKeyPath:@"selectedMusicPlayer"
                          options:NSKeyValueObservingOptionNew
                          context:nil];
        
        [self initListenerBlock];
    }
    
    return self;
}

- (void) dealloc {
    [self disable];
    try {
        [userDefaults removeObserver:self forKeyPath:@"autoDuckMusic" context:nil];
    } catch (const std::exception& e) {
    }
    try {
        [userDefaults removeObserver:self forKeyPath:@"autoDuckPercent" context:nil];
    } catch (const std::exception& e) {
    }
    try {
        [musicPlayers removeObserver:self forKeyPath:@"selectedMusicPlayer" context:nil];
    } catch (const std::exception& e) {
    }
}

- (void) initListenerBlock {
    // To avoid retain cycle
    __unsafe_unretained BGMAutoPauseMusic* weakSelf = self;
    
    listenerBlock = ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress * _Nonnull inAddresses) {
        // inAddresses "may contain addresses for properties for which the listener is not signed up to receive notifications",
        // so we have to check them all
        for (int i = 0; i < inNumberAddresses; i++) {
            if (inAddresses[i].mSelector == kAudioDeviceCustomPropertyDeviceAudibleState) {
                BGMAutoPauseMusic* strongSelf = weakSelf;
                if (!strongSelf) return;
                
                BGMDeviceAudibleState audibleState = [strongSelf deviceAudibleState];
                
#if DEBUG
                const char audibleStateStr[5] = CA4CCToCString(audibleState);
                DebugMsg("BGMAutoPauseMusic::initListenerBlock: kAudioDeviceCustomPropertyDeviceAudibleState property changed to '%s'",
                         audibleStateStr);
#endif
                
                // TODO: We shouldn't assume this block will only get called when BGMDevice's audible state changes. (Even if
                //       the Core Audio docs did specify that, there's no reason not to be fault tolerant.)
                if (audibleState == kBGMDeviceIsAudible) {
                    [strongSelf queuePauseBlock];
                } else if (audibleState == kBGMDeviceIsSilent) {
                    [strongSelf queueUnpauseBlock];
                } else if (audibleState == kBGMDeviceIsSilentExceptMusic) {
                    if (strongSelf->weDucked) {
                        [strongSelf queueUnpauseBlock];
                    } else {
                        // If we pause the music player and then the user unpauses it before the other audio stops, we need to set
                        // wePaused to false at some point before the other audio starts again so we know we should pause
                        DebugMsg("BGMAutoPauseMusic: Device is silent except music, resetting wePaused flag");
                        strongSelf->wePaused = NO;
                    }
                }
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
    
    // If pause delay is 0, pause/duck immediately (no delay)
    if (pauseDelayMS == 0) {
        DebugMsg("BGMAutoPauseMusic::queuePauseBlock: Pause/duck delay is 0, pausing/ducking immediately");
        
        // Pause/duck immediately if device is audible and we haven't already paused/ducked
        if (!wePaused && !weDucked && ([self deviceAudibleState] == kBGMDeviceIsAudible)) {
            if (userDefaults.autoDuckMusic) {
                [self duckMusicPlayer];
            } else {
                wePaused = ([musicPlayers.selectedMusicPlayer pause] || wePaused);
            }
        }
        return;
    }
    
    UInt64 pauseDelayNSec = pauseDelayMS * NSEC_PER_MSEC;
    
    DebugMsg("BGMAutoPauseMusic::queuePauseBlock: Dispatching pause/duck block at %llu", now);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, pauseDelayNSec),
                   pauseUnpauseMusicQueue,
                   ^{
                       BOOL stillAudible = ([self deviceAudibleState] == kBGMDeviceIsAudible);
                       
                       DebugMsg("BGMAutoPauseMusic::queuePauseBlock: Running pause/duck block dispatched at %llu.%s wentAudible=%llu",
                                startedPauseDelay,
                                stillAudible ? "" : " Not pausing/ducking because the device isn't audible.",
                                wentAudible);
                       
                       // Pause/duck if this is the most recent pause block and the device is still audible, which means the audible
                       // state hasn't changed since this block was queued. Also set wePaused/weDucked to true if the player wasn't
                       // already paused/ducked.
                       if (!wePaused && !weDucked && (startedPauseDelay == wentAudible) && stillAudible) {
                           if (userDefaults.autoDuckMusic) {
                               [self duckMusicPlayer];
                           } else {
                               wePaused = ([musicPlayers.selectedMusicPlayer pause] || wePaused);
                           }
                       }
                   });
}

- (void) queueUnpauseBlock {
    UInt64 now = mach_absolute_time();
    wentSilent = now;
    UInt64 startedUnpauseDelay = now;
    
    // Get user-configurable max delay
    UInt64 maxUnpauseDelayMS = userDefaults.maxUnpauseDelayMS;
    
    // If max unpause delay is 0, unpause/unduck immediately (no delay)
    if (maxUnpauseDelayMS == 0) {
        DebugMsg("BGMAutoPauseMusic::queueUnpauseBlock: Max unpause/unduck delay is 0, unpausing/unducking immediately");
        
        BGMDeviceAudibleState currentState = [self deviceAudibleState];
        DebugMsg("BGMAutoPauseMusic::queueUnpauseBlock: Immediate unpause/unduck - wePaused=%s, weDucked=%s, currentState=%s", 
                 wePaused ? "YES" : "NO", 
                 weDucked ? "YES" : "NO",
                 currentState == kBGMDeviceIsSilent ? "Silent" : 
                 (currentState == kBGMDeviceIsAudible ? "Audible" : "SilentExceptMusic"));
        
        BOOL silentEnough = (currentState == kBGMDeviceIsSilent) || (weDucked && (currentState == kBGMDeviceIsSilentExceptMusic));
        if (silentEnough) {
            if (wePaused) {
                wePaused = NO;
                [musicPlayers.selectedMusicPlayer unpause];
            } else if (weDucked) {
                [self unduckMusicPlayer];
            }
        }
        return;
    }
    
    // Unpause sooner if we've only been paused/ducked for a short time. This is so a notification sound causing an auto-pause/duck is less of an interruption. See issue #311 for the longer fade-out/fade-in transition idea.
    UInt64 unpauseDelayNsec =
        static_cast<UInt64>(static_cast<Float64>(wentSilent - wentAudible) *
                            kUnpauseDelayWeightingFactor);
    
    // Convert from absolute time to nanos.
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    unpauseDelayNsec = unpauseDelayNsec * info.numer / info.denom;
    
    // Clamp using user-configurable max delay and calculated min delay.
    UInt64 maxUnpauseDelayNSec = maxUnpauseDelayMS * NSEC_PER_MSEC;
    UInt64 minUnpauseDelayNSec = maxUnpauseDelayNSec / 10;
    unpauseDelayNsec = std::min(maxUnpauseDelayNSec, unpauseDelayNsec);
    unpauseDelayNsec = std::max(minUnpauseDelayNSec, unpauseDelayNsec);
    
    DebugMsg("BGMAutoPauseMusic::queueUnpauseBlock: Dispatched unpause/unduck block at %llu. unpauseDelayNsec=%llu",
             now,
             unpauseDelayNsec);
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, unpauseDelayNsec),
                   pauseUnpauseMusicQueue,
                   ^{
                       BGMDeviceAudibleState currentState = [self deviceAudibleState];
                       BOOL silentEnough = (currentState == kBGMDeviceIsSilent) || (weDucked && (currentState == kBGMDeviceIsSilentExceptMusic));
                       BOOL isLatestUnpause = (startedUnpauseDelay == wentSilent);
                       
                       DebugMsg("BGMAutoPauseMusic::queueUnpauseBlock: Running unpause/unduck block dispatched at %llu. wePaused=%s, weDucked=%s, isLatest=%s, currentState=%s, wentSilent=%llu",
                                startedUnpauseDelay,
                                wePaused ? "YES" : "NO",
                                weDucked ? "YES" : "NO",
                                isLatestUnpause ? "YES" : "NO",
                                currentState == kBGMDeviceIsSilent ? "Silent" : 
                                (currentState == kBGMDeviceIsAudible ? "Audible" : "SilentExceptMusic"),
                                wentSilent);
                       
                       // Unpause or unduck if we were the one who paused/ducked. Also check that the device is still silent (or silent-except-music if we ducked), which means the audible state hasn't changed since this block was queued.
                       if (isLatestUnpause && silentEnough) {
                           if (wePaused) {
                               DebugMsg("BGMAutoPauseMusic::queueUnpauseBlock: Unpausing music player");
                               wePaused = NO;
                               [musicPlayers.selectedMusicPlayer unpause];
                           } else if (weDucked) {
                               [self unduckMusicPlayer];
                           }
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

- (void) observeValueForKeyPath:(NSString* __nullable)keyPath
                       ofObject:(id __nullable)object
                         change:(NSDictionary* __nullable)change
                        context:(void* __nullable)context
{
    #pragma unused (object, change, context)
    if ([keyPath isEqualToString:@"autoDuckMusic"]) {
        if (wePaused || weDucked) {
            dispatch_async(pauseUnpauseMusicQueue, ^{
                BGMDeviceAudibleState state = [self deviceAudibleState];
                if (state == kBGMDeviceIsAudible) {
                    if (userDefaults.autoDuckMusic && wePaused) {
                        // Transition from paused to ducked:
                        // 1. Unpause
                        [musicPlayers.selectedMusicPlayer unpause];
                        wePaused = NO;
                        // 2. Duck
                        [self duckMusicPlayer];
                    } else if (!userDefaults.autoDuckMusic && weDucked) {
                        // Transition from ducked to paused:
                        // 1. Unduck
                        [self unduckMusicPlayer];
                        // 2. Pause
                        wePaused = ([musicPlayers.selectedMusicPlayer pause] || wePaused);
                    }
                }
            });
        }
    } else if ([keyPath isEqualToString:@"autoDuckPercent"]) {
        if (weDucked) {
            dispatch_async(pauseUnpauseMusicQueue, ^{
                // Recalculate ducked volume and update the player volume
                float duckingFactor = (float)userDefaults.autoDuckPercent / 100.0f;
                duckedVolume = (int)(static_cast<float>(originalVolume) * duckingFactor);
                if (duckedVolume >= originalVolume && originalVolume > 0) {
                    duckedVolume = originalVolume - 1;
                }
                [self setMusicPlayerVolume:duckedVolume];
            });
        }
    } else if ([keyPath isEqualToString:@"selectedMusicPlayer"]) {
        wePaused = NO;
        weDucked = NO;
    }
}

- (void) duckMusicPlayer {
    id<BGMMusicPlayer> player = musicPlayers.selectedMusicPlayer;
    if (!player.isRunning || !player.isPlaying) {
        return;
    }
    
    originalVolume = [self getMusicPlayerVolume];
    
    float duckingFactor = (float)userDefaults.autoDuckPercent / 100.0f;
    duckedVolume = (int)(static_cast<float>(originalVolume) * duckingFactor);
    if (duckedVolume >= originalVolume && originalVolume > 0) {
        duckedVolume = originalVolume - 1;
    }
    
    DebugMsg("BGMAutoPauseMusic::duckMusicPlayer: originalVolume=%d, duckedVolume=%d", originalVolume, duckedVolume);
    
    [self setMusicPlayerVolume:duckedVolume];
    weDucked = YES;
}

- (void) unduckMusicPlayer {
    if (!weDucked) {
        return;
    }
    
    int currentVolume = [self getMusicPlayerVolume];
    DebugMsg("BGMAutoPauseMusic::unduckMusicPlayer: currentVolume=%d, expectedDuckedVolume=%d, originalVolume=%d",
             currentVolume, duckedVolume, originalVolume);
             
    if (std::abs(currentVolume - duckedVolume) <= 1) {
        DebugMsg("BGMAutoPauseMusic::unduckMusicPlayer: Restoring volume to originalVolume=%d", originalVolume);
        [self setMusicPlayerVolume:originalVolume];
    } else {
        DebugMsg("BGMAutoPauseMusic::unduckMusicPlayer: Volume was changed manually while ducked. Keeping currentVolume=%d", currentVolume);
    }
    
    weDucked = NO;
}

- (int) getMusicPlayerVolume {
    id<BGMMusicPlayer> player = musicPlayers.selectedMusicPlayer;
    NSString* playerBundleID = player.bundleID;
    pid_t playerPid = player.pid ? [player.pid intValue] : -1;
    __block int volume = 50;
    
    if (playerPid == -1 && playerBundleID != nil) {
        NSArray<NSRunningApplication*>* apps = [NSRunningApplication runningApplicationsWithBundleIdentifier:playerBundleID];
        if (apps.count > 0) {
            playerPid = apps.firstObject.processIdentifier;
        }
    }

    BGMLogAndSwallowExceptions("BGMAutoPauseMusic::getMusicPlayerVolume", ([&] {
        CACFArray volumes([audioDevices bgmDevice].GetAppVolumes(), false);
        for (UInt32 i = 0; i < volumes.GetNumberItems(); i++) {
            CACFDictionary appVolume(false);
            volumes.GetCACFDictionary(i, appVolume);

            CACFString bundleID;
            bundleID.DontAllowRelease();
            appVolume.GetCACFString(CFSTR(kBGMAppVolumesKey_BundleID), bundleID);

            pid_t pid;
            appVolume.GetSInt32(CFSTR(kBGMAppVolumesKey_ProcessID), pid);

            if ((playerPid != -1 && playerPid == pid) ||
                (playerBundleID != nil && [playerBundleID isEqualToString:(__bridge NSString*)bundleID.GetCFString()])) {
                appVolume.GetSInt32(CFSTR(kBGMAppVolumesKey_RelativeVolume), volume);
                break;
            }
        }
    }));

    return volume;
}

- (void) setMusicPlayerVolume:(int)volume {
    id<BGMMusicPlayer> player = musicPlayers.selectedMusicPlayer;
    NSString* playerBundleID = player.bundleID;
    pid_t playerPid = player.pid ? [player.pid intValue] : -1;
    
    if (playerPid == -1 && playerBundleID != nil) {
        NSArray<NSRunningApplication*>* apps = [NSRunningApplication runningApplicationsWithBundleIdentifier:playerBundleID];
        if (apps.count > 0) {
            playerPid = apps.firstObject.processIdentifier;
        }
    }
    
    BGMLogAndSwallowExceptions("BGMAutoPauseMusic::setMusicPlayerVolume", ([&] {
        [audioDevices bgmDevice].SetAppVolume(volume, playerPid, (__bridge CFStringRef)playerBundleID);
    }));
}

@end


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
//  BGMMusicPlayers.mm
//  BGMApp
//
//  Copyright © 2016-2019 Kyle Neideck
//

// Self include
#import "BGMMusicPlayers.h"

// Local includes
#import "BGM_Types.h"

// Music player includes
#import "BGMiTunes.h"
#import "BGMSpotify.h"
#import "BGMVLC.h"
#import "BGMVOX.h"
#import "BGMDecibel.h"
#import "BGMHermes.h"
#import "BGMSwinsian.h"
#import "BGMMusic.h"
#import "BGMGooglePlayMusicDesktopPlayer.h"
#import <ApplicationServices/ApplicationServices.h>


#pragma clang assume_nonnull begin

// Netease Cloud Music support uses System Events because this app does not expose a normal scripting
// dictionary in this project.
static NSString* const kNeteaseMusicBundleID = @"com.netease.163music";
static NSString* const kSystemEventsBundleID = @"com.apple.systemevents";
static NSString* const kNeteaseMusicProcessName = @"NeteaseMusic";
static NSString* const kNeteaseMusicControlMenuEnglish = @"Controls";
static NSString* const kNeteaseMusicControlMenuEnglishAlt = @"Control";
static NSString* const kNeteaseMusicControlMenuChinese = @"控制";
static NSString* const kNeteaseMusicControlMenuChineseAlt = @"播放控制";
static NSString* const kNeteaseMusicPauseEnglish = @"Pause";
static NSString* const kNeteaseMusicPauseEnglish2 = @"Pause Playback";
static NSString* const kNeteaseMusicPauseChinese = @"暂停";
static NSString* const kNeteaseMusicPauseChinese2 = @"暂停播放";
static NSString* const kNeteaseMusicPlayEnglish = @"Play";
static NSString* const kNeteaseMusicPlayEnglish2 = @"Resume";
static NSString* const kNeteaseMusicPlayChinese = @"播放";
static NSString* const kNeteaseMusicPlayChinese2 = @"继续播放";

static NSArray<NSString*>* const kNeteaseMusicControlMenuLabels = @[
    kNeteaseMusicControlMenuEnglish,
    kNeteaseMusicControlMenuEnglishAlt,
    kNeteaseMusicControlMenuChinese,
    kNeteaseMusicControlMenuChineseAlt
];
static NSArray<NSString*>* const kNeteaseMusicPauseLabels = @[
    kNeteaseMusicPauseEnglish,
    kNeteaseMusicPauseEnglish2,
    kNeteaseMusicPauseChinese,
    kNeteaseMusicPauseChinese2
];
static NSArray<NSString*>* const kNeteaseMusicPlayLabels = @[
    kNeteaseMusicPlayEnglish,
    kNeteaseMusicPlayEnglish2,
    kNeteaseMusicPlayChinese,
    kNeteaseMusicPlayChinese2
];

static void BGMRequestAccessibilityPermission(void) {
    NSDictionary* options = @{ (__bridge NSString*)kAXTrustedCheckOptionPrompt: @YES };
    AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
}

static void BGMRequestAutomationPermissionForBundle(NSString* bundleID, NSString* playerName) {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101400
    if (@available(macOS 10.14, *)) {
        NSAppleEventDescriptor* targetDescriptor =
            [NSAppleEventDescriptor descriptorWithBundleIdentifier:bundleID];
        if (!targetDescriptor) {
            DebugMsg("BGMRequestAutomationPermissionForBundle: failed to create descriptor for %s",
                     playerName.UTF8String);
            return;
        }

        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
            OSStatus status =
                AEDeterminePermissionToAutomateTarget(targetDescriptor.aeDesc,
                                                      typeWildCard,
                                                      typeWildCard,
                                                      true);
            if (status == noErr) {
                DebugMsg("BGMRequestAutomationPermissionForBundle: permission granted for %s (%s)",
                         playerName.UTF8String,
                         bundleID.UTF8String);
            } else {
                DebugMsg("BGMRequestAutomationPermissionForBundle: permission denied for %s (%s), status=%d",
                         playerName.UTF8String,
                         bundleID.UTF8String,
                         status);
            }
        });
    } else {
        #pragma unused(bundleID, playerName)
    }
#else
    #pragma unused(bundleID, playerName)
#endif
}

static inline NSString* NeteaseMusicEscapeAppleScriptString(NSString* input) {
    return [[input stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"]
            stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
}

@interface BGMNeteaseMusic : BGMMusicPlayerBase<BGMMusicPlayer>

+ (NSUUID*) sharedMusicPlayerID;

@end

@implementation BGMNeteaseMusic

+ (NSUUID*) sharedMusicPlayerID {
    NSUUID* musicPlayerID = [BGMMusicPlayerBase makeID:@"F4E4ABEF-C775-4284-981B-A2B14D19A342"];
    return (NSUUID*)musicPlayerID;
}

- (instancetype) init {
    if ((self = [super initWithMusicPlayerID:[BGMNeteaseMusic sharedMusicPlayerID]
                                      name:@"网易云音乐"
                                  bundleID:kNeteaseMusicBundleID])) {
        // no additional init
    }
    
    return self;
}

- (void) wasSelected {
    [super wasSelected];

    // Netease control goes through System Events UI scripting, so request both permissions when the
    // user actually chooses this player instead of prompting on every app launch.
    BGMRequestAutomationPermissionForBundle(kSystemEventsBundleID, @"System Events");
    BGMRequestAccessibilityPermission();
}

- (NSString* __nullable) currentControlState {
    NSString* controlMenuCandidates =
        kNeteaseMusicControlMenuLabels.count
            ? [NSString stringWithFormat:@"\"%@\"", NeteaseMusicEscapeAppleScriptString(kNeteaseMusicControlMenuLabels[0])]
            : @"\"Controls\"";
    for (NSUInteger i = 1; i < kNeteaseMusicControlMenuLabels.count; i++) {
        controlMenuCandidates = [controlMenuCandidates stringByAppendingFormat:
                                @", \"%@\"",
                                NeteaseMusicEscapeAppleScriptString(kNeteaseMusicControlMenuLabels[i])];
    }
    
    NSString* controlStateCandidates =
        kNeteaseMusicPauseLabels.count
            ? [NSString stringWithFormat:@"\"%@\"", NeteaseMusicEscapeAppleScriptString(kNeteaseMusicPauseLabels[0])]
            : @"\"Pause\"";
    for (NSUInteger i = 1; i < kNeteaseMusicPauseLabels.count; i++) {
        controlStateCandidates = [controlStateCandidates stringByAppendingFormat:
                                 @", \"%@\"",
                                 NeteaseMusicEscapeAppleScriptString(kNeteaseMusicPauseLabels[i])];
    }
    NSString* playStateCandidates =
        kNeteaseMusicPlayLabels.count
            ? [NSString stringWithFormat:@"\"%@\"", NeteaseMusicEscapeAppleScriptString(kNeteaseMusicPlayLabels[0])]
            : @"\"Play\"";
    for (NSUInteger i = 1; i < kNeteaseMusicPlayLabels.count; i++) {
        playStateCandidates = [playStateCandidates stringByAppendingFormat:
                              @", \"%@\"",
                              NeteaseMusicEscapeAppleScriptString(kNeteaseMusicPlayLabels[i])];
    }

    NSString* source =
        [NSString stringWithFormat:
         @"tell application \"System Events\"\n"
         @"if not (exists process \"%@\") then return \"\"\n"
         @"tell process \"%@\"\n"
         @"    set controlMenuName to missing value\n"
         @"    repeat with candidateMenu in {%@}\n"
         @"        if exists menu bar item (contents of candidateMenu) of menu bar 1 then\n"
         @"            set controlMenuName to contents of candidateMenu\n"
         @"            exit repeat\n"
         @"        end if\n"
         @"    end repeat\n"
         @"    if controlMenuName is missing value then return \"\"\n"
         @"    set controlMenu to menu 1 of menu bar item controlMenuName of menu bar 1\n"
         @"    repeat with candidateItem in {%@}\n"
         @"        if exists menu item (contents of candidateItem) of controlMenu then\n"
         @"            return contents of candidateItem\n"
         @"        end if\n"
         @"    end repeat\n"
         @"    repeat with candidateItem in {%@}\n"
         @"        if exists menu item (contents of candidateItem) of controlMenu then\n"
         @"            return contents of candidateItem\n"
         @"        end if\n"
         @"    end repeat\n"
         @"    return \"\"\n"
         @"end tell\n"
         @"end tell\n",
         kNeteaseMusicProcessName,
         kNeteaseMusicProcessName,
         controlMenuCandidates,
         controlStateCandidates,
         playStateCandidates];

    NSDictionary* __nullable error = nil;
    NSAppleScript* script = [[NSAppleScript alloc] initWithSource:source];
    NSAppleEventDescriptor* result = [script executeAndReturnError:&error];
    
    if (error) {
        NSString* errString = error ? [error description] : @"";
        DebugMsg("BGMNeteaseMusic::currentControlState: System Events returned error=%s",
                 errString.UTF8String);
        return nil;
    }
    
    return result.stringValue;
}

- (BOOL) clickControlMenuItemForCandidates:(NSArray<NSString*>*)itemNames {
    if (!itemNames.count) {
        return NO;
    }
    
    NSString* controlMenuCandidates =
        kNeteaseMusicControlMenuLabels.count
            ? [NSString stringWithFormat:@"\"%@\"", NeteaseMusicEscapeAppleScriptString(kNeteaseMusicControlMenuLabels[0])]
            : @"\"Controls\"";
    for (NSUInteger i = 1; i < kNeteaseMusicControlMenuLabels.count; i++) {
        controlMenuCandidates = [controlMenuCandidates stringByAppendingFormat:
                                @", \"%@\"",
                                NeteaseMusicEscapeAppleScriptString(kNeteaseMusicControlMenuLabels[i])];
    }

    NSString* targetCandidates =
        [NSString stringWithFormat:@"\"%@\"", NeteaseMusicEscapeAppleScriptString(itemNames[0])];
    for (NSUInteger i = 1; i < itemNames.count; i++) {
        targetCandidates = [targetCandidates stringByAppendingFormat:
                           @", \"%@\"",
                           NeteaseMusicEscapeAppleScriptString(itemNames[i])];
    }
    
    NSString* source =
        [NSString stringWithFormat:
         @"tell application \"System Events\"\n"
         @"if not (exists process \"%@\") then return \"\"\n"
         @"tell process \"%@\"\n"
         @"    set controlMenuName to missing value\n"
         @"    repeat with candidateMenu in {%@}\n"
         @"        if exists menu bar item (contents of candidateMenu) of menu bar 1 then\n"
         @"            set controlMenuName to contents of candidateMenu\n"
         @"            exit repeat\n"
         @"        end if\n"
         @"    end repeat\n"
         @"    if controlMenuName is not missing value then\n"
         @"        click menu bar item controlMenuName of menu bar 1\n"
         @"        delay 0.1\n"
         @"        set controlMenu to menu 1 of menu bar item controlMenuName of menu bar 1\n"
         @"        repeat with candidateItem in {%@}\n"
         @"            if exists menu item (contents of candidateItem) of controlMenu then\n"
         @"                click menu item (contents of candidateItem) of controlMenu\n"
         @"                delay 0.05\n"
         @"                return \"ok\"\n"
         @"            end if\n"
         @"        end repeat\n"
         @"        key code 53\n"
         @"    end if\n"
         @"    return \"\"\n"
         @"end tell\n"
         @"end tell\n",
         kNeteaseMusicProcessName,
         kNeteaseMusicProcessName,
         controlMenuCandidates,
         targetCandidates];
    
    NSDictionary* __nullable error = nil;
    NSAppleScript* script = [[NSAppleScript alloc] initWithSource:source];
    NSAppleEventDescriptor* result = [script executeAndReturnError:&error];
    
    if (error) {
        NSString* errString = error ? [error description] : @"";
        DebugMsg("BGMNeteaseMusic::clickControlMenuItem: System Events returned error=%s",
                 errString.UTF8String);
        return NO;
    }
    
    return (result != nil && result.stringValue && [result.stringValue length] > 0);
}

- (BOOL) isRunning {
    return [[NSRunningApplication runningApplicationsWithBundleIdentifier:kNeteaseMusicBundleID] count] > 0;
}

- (BOOL) isPlaying {
    if (!self.running) {
        return NO;
    }
    
    NSString* state = [self currentControlState];
    return ([state isEqualToString:kNeteaseMusicPauseEnglish] ||
            [state isEqualToString:kNeteaseMusicPauseEnglish2] ||
            [state isEqualToString:kNeteaseMusicPauseChinese] ||
            [state isEqualToString:kNeteaseMusicPauseChinese2]);
}

- (BOOL) isPaused {
    if (!self.running) {
        return NO;
    }
    
    NSString* state = [self currentControlState];
    return ([state isEqualToString:kNeteaseMusicPlayEnglish] ||
            [state isEqualToString:kNeteaseMusicPlayEnglish2] ||
            [state isEqualToString:kNeteaseMusicPlayChinese] ||
            [state isEqualToString:kNeteaseMusicPlayChinese2]);
}

- (BOOL) pause {
    // isPlaying checks isRunning, so we don't need to check it here.
    BOOL wasPlaying = self.playing;
    
    if (wasPlaying) {
        DebugMsg("BGMNeteaseMusic::pause: Pausing Netease Cloud Music");
        return [self clickControlMenuItemForCandidates:kNeteaseMusicPauseLabels];
    }
    
    return NO;
}

- (BOOL) unpause {
    // isPaused checks isRunning, so we don't need to check it here.
    BOOL wasPaused = self.paused;
    
    if (wasPaused) {
        DebugMsg("BGMNeteaseMusic::unpause: Unpausing Netease Cloud Music");
        return [self clickControlMenuItemForCandidates:kNeteaseMusicPlayLabels];
    }
    
    return NO;
}

@end

@implementation BGMMusicPlayers {
    BGMAudioDeviceManager* audioDevices;
    BGMUserDefaults* userDefaults;
}

@synthesize selectedMusicPlayer = _selectedMusicPlayer;

- (instancetype) initWithAudioDevices:(BGMAudioDeviceManager*)devices
                         userDefaults:(BGMUserDefaults*)defaults {
    // The classes handling each music player we support. If you write a new music player class, add
    // it to this array.
    NSArray<Class<BGMMusicPlayer>>* mpClasses = @[ [BGMVOX class],
                                                   [BGMVLC class],
                                                   [BGMNeteaseMusic class],
                                                   [BGMSpotify class],
                                                   [BGMiTunes class],
                                                   [BGMDecibel class],
                                                   [BGMHermes class],
                                                   [BGMSwinsian class],
                                                   [BGMMusic class] ];

    // We only support Google Play Music Desktop Player on macOS 10.10 and higher.
    if (@available(macOS 10.10, *)) {
        mpClasses = [mpClasses arrayByAddingObject:[BGMGooglePlayMusicDesktopPlayer class]];
    }

    return [self initWithAudioDevices:devices
                 defaultMusicPlayerID:[BGMiTunes sharedMusicPlayerID]
                   musicPlayerClasses:mpClasses
                         userDefaults:defaults];
}

- (instancetype) initWithAudioDevices:(BGMAudioDeviceManager*)devices
                 defaultMusicPlayerID:(NSUUID*)defaultMusicPlayerID
                   musicPlayerClasses:(NSArray<Class<BGMMusicPlayer>>*)musicPlayerClasses
                         userDefaults:(BGMUserDefaults*)defaults {
    if ((self = [super init])) {
        audioDevices = devices;
        userDefaults = defaults;
        
        // Init _musicPlayers, an array containing one object for each music player in BGMApp.
        //
        // Each music player class has a factory method, createInstancesWithDefaults, that returns
        // all the instances of that class BGMApp will use. (Though so far it's always just one
        // instance.)
        NSMutableArray* musicPlayers = [NSMutableArray new];
        for (Class<BGMMusicPlayer> musicPlayerClass in musicPlayerClasses) {
            NSArray<id<BGMMusicPlayer>>* instances =
                    [musicPlayerClass createInstancesWithDefaults:userDefaults];
            [musicPlayers addObjectsFromArray:instances];
        }
        
        _musicPlayers = [NSArray arrayWithArray:musicPlayers];
        
        // Set _selectedMusicPlayer to its setting from last time BGMApp ran. (Unless this is the first run or
        // that music player isn't available this time.)
        [self initSelectedMusicPlayerFromUserDefaults];
        
        if (!_selectedMusicPlayer) {
            // Couldn't set _selectedMusicPlayer from user defaults, so try BGMDevice's music player property.
            [self initSelectedMusicPlayerFromBGMDevice];
        }
        
        if (!_selectedMusicPlayer) {
            // The user hasn't changed the music player yet, so we set the default music player as selected.
            [self setSelectedMusicPlayerByID:defaultMusicPlayerID];
        }
        
        NSAssert(_selectedMusicPlayer, @"BGMMusicPlayers::initWithAudioDevices: !_selectedMusicPlayer");
    }
    
    return self;
}

- (void) initSelectedMusicPlayerFromUserDefaults {
    // Load the selected music player setting from user defaults.
    
    NSString* __nullable selectedMusicPlayerIDStr = userDefaults.selectedMusicPlayerID;
    NSUUID* __nullable selectedMusicPlayerID = nil;
    
    if (selectedMusicPlayerIDStr) {
        NSString* idStrNN = selectedMusicPlayerIDStr;
        selectedMusicPlayerID = [[NSUUID alloc] initWithUUIDString:idStrNN];
        
        NSAssert(selectedMusicPlayerID,
                 @"BGMMusicPlayers::initSelectedMusicPlayerFromUserDefaults: !selectedMusicPlayerID");
    }
    
    if (selectedMusicPlayerID) {
        NSUUID* idNN = selectedMusicPlayerID;
        BOOL didChangeMusicPlayer = [self setSelectedMusicPlayerByID:idNN];
        
#if DEBUG
        DebugMsg("BGMMusicPlayers::initSelectedMusicPlayerFromUserDefaults: %s selectedMusicPlayerIDStr=%s",
                 (didChangeMusicPlayer ?
                     "Selected music player restored from user defaults." :
                     "The selected music player setting found in user defaults didn't match an available music player."),
                 selectedMusicPlayerIDStr.UTF8String);
#else
        #pragma unused (didChangeMusicPlayer)
#endif
    }
}

- (void) initSelectedMusicPlayerFromBGMDevice {
    // When the selected music player setting hasn't been stored in user defaults yet, we get the music player
    // bundle ID from the driver and look for the music player with that bundle ID. This is mainly done for
    // backwards compatibility.
    
    NSString* __nullable bundleID =
        (__bridge_transfer NSString* __nullable)[audioDevices bgmDevice].GetMusicPlayerBundleID();

    DebugMsg("BGMMusicPlayers::initSelectedMusicPlayerFromBGMDevice: "
             "Trying to set selected music player by bundle ID (from BGMDriver). bundleID=%s",
             (bundleID ? bundleID.UTF8String : "(null)"));
    
    if (bundleID && ![bundleID isEqualToString:@""]) {
        // Find any music players with a bundle ID matching the one from BGMDriver.
        NSArray<id<BGMMusicPlayer>>* matchingMusicPlayers = @[ ];
        
        for (id<BGMMusicPlayer> musicPlayer in _musicPlayers) {
            NSString* bundleIDNN = bundleID;
            if ([musicPlayer.bundleID isEqualToString:bundleIDNN]) {
                DebugMsg("BGMMusicPlayers::initSelectedMusicPlayerFromBGMDevice: Bundle ID on BGMDevice matches %s",
                         musicPlayer.name.UTF8String);
                
                matchingMusicPlayers = [matchingMusicPlayers arrayByAddingObject:musicPlayer];
            }
        }
        
        // Currently, the music players all have different bundle IDs, but that might change at some point. We
        // might want to consider some websites as music players, for example. So we don't change the setting
        // unless the bundle ID only matches one music player.
        if (matchingMusicPlayers.count == 1) {
            // (Use setSelectedMusicPlayerImpl to avoid setSelectedMusicPlayer being called in init.)
            [self setSelectedMusicPlayerImpl:matchingMusicPlayers[0]];
        }
    }
}

- (id<BGMMusicPlayer>) selectedMusicPlayer {
    return _selectedMusicPlayer;
}

- (void) setSelectedMusicPlayer:(id<BGMMusicPlayer>)newSelectedMusicPlayer {
    // Apparently you shouldn't call properties' setter methods in init (KVO notifications might trigger, etc.)
    // so the actual work is done in setSelectedMusicPlayerImpl.
    [self setSelectedMusicPlayerImpl:newSelectedMusicPlayer];
    
    NSAssert(self.selectedMusicPlayer == newSelectedMusicPlayer,
             @"BGMMusicPlayers::setSelectedMusicPlayer: selectedMusicPlayer wasn't set to the object expected");
}

- (BOOL) setSelectedMusicPlayerByID:(NSUUID*)newSelectedMusicPlayerID {
    id<BGMMusicPlayer> __nullable newSelectedMusicPlayer = nil;
    
    // Find the music player with the given ID, if there is one.
    for (id<BGMMusicPlayer> musicPlayer in _musicPlayers) {
        if ([musicPlayer.musicPlayerID isEqual:newSelectedMusicPlayerID]) {
            NSAssert(!newSelectedMusicPlayer, @"BGMMusicPlayers::setSelectedMusicPlayerByID: Non-unique musicPlayerID");
            
            newSelectedMusicPlayer = musicPlayer;
        }
    }
    
    if (newSelectedMusicPlayer) {
        // (Use setSelectedMusicPlayerImpl to avoid setSelectedMusicPlayer being called in init.)
        id<BGMMusicPlayer> newPlayerNN = newSelectedMusicPlayer;
        [self setSelectedMusicPlayerImpl:newPlayerNN];
        
        return YES;
    } else {
        return NO;
    }
}

- (void) setSelectedMusicPlayerImpl:(id<BGMMusicPlayer>)newSelectedMusicPlayer {
    NSAssert([_musicPlayers containsObject:newSelectedMusicPlayer],
             @"BGMMusicPlayers::setSelectedMusicPlayerImpl: Only the music players in the musicPlayers array can be selected. "
              "newSelectedMusicPlayer=%@",
             newSelectedMusicPlayer.name);

    if (_selectedMusicPlayer == newSelectedMusicPlayer) {
        DebugMsg("BGMMusicPlayers::setSelectedMusicPlayerImpl: %s is already the selected music "
                 "player.",
                 _selectedMusicPlayer.name.UTF8String);
        return;
    }

    // Tell the current music player (object) a different player has been selected.
    [_selectedMusicPlayer wasDeselected];
    
    _selectedMusicPlayer = newSelectedMusicPlayer;
    
    DebugMsg("BGMMusicPlayers::setSelectedMusicPlayerImpl: Set selected music player to %s",
             _selectedMusicPlayer.name.UTF8String);
    
    // Update the selected music player on the driver.
    [self updateBGMDeviceMusicPlayerProperties];
    
    // Save the new setting in user defaults.
    userDefaults.selectedMusicPlayerID = _selectedMusicPlayer.musicPlayerID.UUIDString;

    // Tell the music player (object) it's been selected.
    [_selectedMusicPlayer wasSelected];
}

- (void) updateBGMDeviceMusicPlayerProperties {
    // Send the music player's PID and/or bundle ID to the driver.
    
    NSAssert(self.selectedMusicPlayer.pid || self.selectedMusicPlayer.bundleID,
             @"BGMMusicPlayers::updateBGMDeviceMusicPlayerProperties: Music player has neither bundle ID nor PID");

    if (self.selectedMusicPlayer.pid) {
        [audioDevices bgmDevice].SetMusicPlayerProcessID((__bridge CFNumberRef)self.selectedMusicPlayer.pid);
    }
    
    if (self.selectedMusicPlayer.bundleID) {
        [audioDevices bgmDevice].SetMusicPlayerBundleID((__bridge CFStringRef)self.selectedMusicPlayer.bundleID);
    }
}

@end

#pragma clang assume_nonnull end

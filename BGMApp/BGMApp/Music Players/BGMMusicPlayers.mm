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
//  Copyright © 2016-2018 Kyle Neideck
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


#pragma clang assume_nonnull begin

@implementation BGMMusicPlayers {
    BGMAudioDeviceManager* audioDevices;
    BGMUserDefaults* userDefaults;
}

@synthesize selectedMusicPlayer = _selectedMusicPlayer;

- (instancetype) initWithAudioDevices:(BGMAudioDeviceManager*)devices
                         userDefaults:(BGMUserDefaults*)defaults {
    return [self initWithAudioDevices:devices
                 defaultMusicPlayerID:[BGMiTunes sharedMusicPlayerID]
                   // If you write a new music player class, add it to this array.
                   musicPlayerClasses:@[ [BGMVOX class],
                                         [BGMVLC class],
                                         [BGMSpotify class],
                                         [BGMiTunes class],
                                         [BGMDecibel class],
                                         [BGMHermes class],
                                         [BGMSwinsian class] ]
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
        // Each music player class has a factory method, createInstances, that returns all the instances of that
        // class BGMApp will use. (Though so far it's always just one instance.)
        NSMutableArray* musicPlayers = [NSMutableArray new];
        for (Class<BGMMusicPlayer> musicPlayerClass in musicPlayerClasses) {
            [musicPlayers addObjectsFromArray:[musicPlayerClass createInstances]];
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
    // backwards compatability.
    
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
    [_selectedMusicPlayer onDeselect];
    
    _selectedMusicPlayer = newSelectedMusicPlayer;
    
    DebugMsg("BGMMusicPlayers::setSelectedMusicPlayerImpl: Set selected music player to %s",
             _selectedMusicPlayer.name.UTF8String);
    
    // Update the selected music player on the driver.
    [self updateBGMDeviceMusicPlayerProperties];
    
    // Save the new setting in user defaults.
    userDefaults.selectedMusicPlayerID = _selectedMusicPlayer.musicPlayerID.UUIDString;

    // Tell the music player (object) it's been selected.
    [_selectedMusicPlayer onSelect];
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


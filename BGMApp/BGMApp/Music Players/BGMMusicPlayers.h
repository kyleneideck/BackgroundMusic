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
//  BGMMusicPlayers.h
//  BGMApp
//
//  Copyright © 2016, 2019 Kyle Neideck
//
//  Holds the music players (i.e. BGMMusicPlayer objects) available in BGMApp. Also keeps track of
//  which music player is currently selected by the user.
//

// Local Includes
#import "BGMAudioDeviceManager.h"
#import "BGMMusicPlayer.h"
#import "BGMUserDefaults.h"

// System Includes
#import <Foundation/Foundation.h>


#pragma clang assume_nonnull begin

@interface BGMMusicPlayers : NSObject

// Calls initWithAudioDevices:musicPlayers: with sensible defaults.
- (instancetype) initWithAudioDevices:(BGMAudioDeviceManager*)devices
                         userDefaults:(BGMUserDefaults*)defaults;

// defaultMusicPlayerID is the musicPlayerID (see BGMMusicPlayer.h) of the music player that should be
// selected by default.
//
// The createInstancesWithDefaults method of each class in musicPlayerClasses will be called and
// the results will be stored in the musicPlayers property.
- (instancetype) initWithAudioDevices:(BGMAudioDeviceManager*)devices
                 defaultMusicPlayerID:(NSUUID*)defaultMusicPlayerID
                   musicPlayerClasses:(NSArray<Class<BGMMusicPlayer>>*)musicPlayerClasses
                         userDefaults:(BGMUserDefaults*)defaults;

@property (readonly) NSArray<id<BGMMusicPlayer>>* musicPlayers;

// The music player currently selected in the preferences menu. BGMDevice is informed when this property
// is changed.
@property id<BGMMusicPlayer> selectedMusicPlayer;

@end

#pragma clang assume_nonnull end


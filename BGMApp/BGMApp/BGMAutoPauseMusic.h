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
//  BGMAutoPauseMusic.h
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//
//  When enabled, BGMAutoPauseMusic listens for notifications from BGMDevice to tell when music is playing and
//  pauses the music player if other audio starts.
//

// Local Includes
#import "BGMAudioDeviceManager.h"
#import "BGMMusicPlayers.h"
#import "BGMUserDefaults.h"

// System Includes
#import <Foundation/Foundation.h>


#pragma clang assume_nonnull begin

@interface BGMAutoPauseMusic : NSObject

- (id) initWithAudioDevices:(BGMAudioDeviceManager*)inAudioDevices musicPlayers:(BGMMusicPlayers*)inMusicPlayers userDefaults:(BGMUserDefaults*)inUserDefaults;

- (void) enable;
- (void) disable;

@end

#pragma clang assume_nonnull end


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
//  BGMAutoPauseMusicPrefs.h
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//

// Local Includes
#import "BGMAudioDeviceManager.h"
#import "BGMMusicPlayers.h"

// System Includes
#import <Cocoa/Cocoa.h>


#pragma clang assume_nonnull begin

@interface BGMAutoPauseMusicPrefs : NSObject

- (id) initWithPreferencesMenu:(NSMenu*)inPrefsMenu
                  audioDevices:(BGMAudioDeviceManager*)inAudioDevices
                  musicPlayers:(BGMMusicPlayers*)inMusicPlayers;

@end

#pragma clang assume_nonnull end 


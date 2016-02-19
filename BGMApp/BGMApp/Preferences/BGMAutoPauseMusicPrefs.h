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

// PublicUtility Includes
#include "BGMAudioDeviceManager.h"

// System Includes
#import <AppKit/AppKit.h>


@interface BGMAutoPauseMusicPrefs : NSObject

// Note that toggleAutoPauseMusicMenuItem is the item in the main menu that enables/disables auto-pausing, rather than the
// disabled "Auto-pause" menu item in the preferences menu that acts as a section heading. This class updates the text of
// toggleAutoPauseMusicMenuItem when the user changes the music player.
- (id) initWithPreferencesMenu:(NSMenu*)inPrefsMenu
  toggleAutoPauseMusicMenuItem:(NSMenuItem*)inToggleAutoPauseMusicMenuItem
                  audioDevices:(BGMAudioDeviceManager*)inAudioDevices;

@end


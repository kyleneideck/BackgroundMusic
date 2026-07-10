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
//  BGMSleepTimer.h
//  BGMApp
//
//  Copyright © 2026 Background Music contributors
//
//  A countdown timer the user sets from the status bar menu. When it expires, BGMApp pauses the
//  selected music player and, optionally, mutes BGMDevice's output so everything goes silent. The
//  use case is falling asleep to music.
//

// Local Includes
#import "BGMAudioDeviceManager.h"
#import "BGMMusicPlayers.h"
#import "BGMUserDefaults.h"

// System Includes
#import <Cocoa/Cocoa.h>


#pragma clang assume_nonnull begin

@interface BGMSleepTimer : NSObject

// Creates the "Sleep Timer" menu item (with its submenu). Add menuItem to the main menu to show it.
- (instancetype) initWithAudioDevices:(BGMAudioDeviceManager*)audioDevices
                         musicPlayers:(BGMMusicPlayers*)musicPlayers
                         userDefaults:(BGMUserDefaults*)userDefaults;

- (instancetype) init NS_UNAVAILABLE;

// The top-level menu item. Its title shows the remaining time while a timer is active.
@property (readonly) NSMenuItem* menuItem;

// Called when the menu containing menuItem is about to be shown, so the remaining time in the title
// can be refreshed.
- (void) parentMenuNeedsUpdate;

@end

#pragma clang assume_nonnull end

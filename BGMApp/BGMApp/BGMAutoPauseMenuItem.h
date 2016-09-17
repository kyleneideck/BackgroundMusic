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
//  BGMAutoPauseMenuItem.h
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//

// Local Includes
#import "BGMAutoPauseMusic.h"
#import "BGMMusicPlayers.h"
#import "BGMUserDefaults.h"

// System Includes
#import <Cocoa/Cocoa.h>


#pragma clang assume_nonnull begin

@interface BGMAutoPauseMenuItem : NSObject

- (instancetype) initWithMenuItem:(NSMenuItem*)item
                   autoPauseMusic:(BGMAutoPauseMusic*)autoPause
                     musicPlayers:(BGMMusicPlayers*)players
                     userDefaults:(BGMUserDefaults*)defaults;

// Handle events passed along by the delegate (NSMenuDelegate) of the menu containing this menu item.
- (void) parentMenuNeedsUpdate;
- (void) parentMenuItemWillHighlight:(NSMenuItem* __nullable)item;

@end

#pragma clang assume_nonnull end


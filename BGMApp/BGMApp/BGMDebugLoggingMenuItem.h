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
//  BGMDebugLoggingMenuItem.h
//  BGMApp
//
//  Copyright © 2020 Kyle Neideck
//
//  A menu item in the main menu that enables/disables debug logging. Only visible if you hold the
//  option down when you click the status bar icon to reveal the main menu.
//
//  TODO: It would be better to have this menu item in the Preferences menu (maybe in an Advanced
//        section) and always visible, but first we'd need to add something that tells the user how
//        to view the log messages. Or better yet, something that automatically opens them.
//

// System Includes
#import <Cocoa/Cocoa.h>


#pragma clang assume_nonnull begin

@interface BGMDebugLoggingMenuItem : NSObject

- (instancetype) initWithMenuItem:(NSMenuItem*)menuItem;

// True if the main menu is showing hidden items/options because the user held the option key when
// they clicked the icon. This class makes the debug logging menu item visible if this property has
// been set true or if debug logging is enabled.
@property (nonatomic) BOOL menuShowingExtraOptions;

@end

#pragma clang assume_nonnull end


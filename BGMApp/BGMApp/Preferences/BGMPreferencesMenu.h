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
//  BGMPreferencesMenu.h
//  BGMApp
//
//  Copyright © 2016, 2018 Kyle Neideck
//
//  Handles the preferences menu UI. The user's preference changes are often passed directly to the driver rather
//  than to other BGMApp classes.
//

// Local Includes
#import "BGMAudioDeviceManager.h"
#import "BGMMusicPlayers.h"

// System Includes
#import <Cocoa/Cocoa.h>


NS_ASSUME_NONNULL_BEGIN

@interface BGMPreferencesMenu : NSObject

- (id) initWithBGMMenu:(NSMenu*)inBGMMenu
          audioDevices:(BGMAudioDeviceManager*)inAudioDevices
          musicPlayers:(BGMMusicPlayers*)inMusicPlayers
            aboutPanel:(NSPanel*)inAboutPanel
 aboutPanelLicenseView:(NSTextView*)inAboutPanelLicenseView;

@end

NS_ASSUME_NONNULL_END


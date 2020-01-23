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
//  BGMAppDelegate.h
//  BGMApp
//
//  Copyright © 2016, 2017, 2020 Kyle Neideck
//
//  Sets up and tears down the app.
//

// Local Includes
#import "BGMAudioDeviceManager.h"

// System Includes
#import <Cocoa/Cocoa.h>


// Tags for UI elements in MainMenu.xib
static NSInteger const kVolumesHeadingMenuItemTag = 3;
static NSInteger const kSeparatorBelowVolumesMenuItemTag = 4;

@interface BGMAppDelegate : NSObject <NSApplicationDelegate, NSMenuDelegate>

@property (weak) IBOutlet NSMenu* bgmMenu;

@property (weak) IBOutlet NSView* outputVolumeView;
@property (weak) IBOutlet NSTextField* outputVolumeLabel;
@property (weak) IBOutlet NSSlider* outputVolumeSlider;

@property (weak) IBOutlet NSView* systemSoundsView;
@property (weak) IBOutlet NSSlider* systemSoundsSlider;

@property (weak) IBOutlet NSView* appVolumeView;

@property (weak) IBOutlet NSPanel* aboutPanel;
@property (unsafe_unretained) IBOutlet NSTextView* aboutPanelLicenseView;

@property (weak) IBOutlet NSMenuItem* autoPauseMenuItemUnwrapped;
@property (weak) IBOutlet NSMenuItem* debugLoggingMenuItemUnwrapped;

@property (readonly) BGMAudioDeviceManager* audioDevices;

@end


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
//  BGMOutputVolumeMenuItem.h
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//

// Local Includes
#import "BGMAudioDeviceManager.h"

// System Includes
#import <Cocoa/Cocoa.h>


#pragma clang assume_nonnull begin

@interface BGMOutputVolumeMenuItem : NSMenuItem

// A menu item with a slider for controlling the volume of the output device. Similar to the one in
// macOS's Volume menu extra.
//
// view, slider and deviceLabel are the UI elements from MainMenu.xib.
- (instancetype) initWithAudioDevices:(BGMAudioDeviceManager*)devices
                                 view:(NSView*)view
                               slider:(NSSlider*)slider
                          deviceLabel:(NSTextField*)label;

- (void) outputDeviceDidChange;

@end

#pragma clang assume_nonnull end


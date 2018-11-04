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
//  BGMOutputDeviceMenuSection.h
//  BGMApp
//
//  Copyright © 2016, 2018 Kyle Neideck
//

// Local Includes
#import "BGMAudioDeviceManager.h"
#import "BGMPreferredOutputDevices.h"

// System Includes
#import <AppKit/AppKit.h>


#pragma clang assume_nonnull begin

@interface BGMOutputDeviceMenuSection : NSObject

- (instancetype) initWithBGMMenu:(NSMenu*)inBGMMenu
                    audioDevices:(BGMAudioDeviceManager*)inAudioDevices
                preferredDevices:(BGMPreferredOutputDevices*)inPreferredDevices;

// To be called when BGMApp has been set to use a different output device. For example, when a new
// device is connected and BGMPreferredOutputDevices decides BGMApp should switch to it.
- (void) outputDeviceDidChange;

@end

#pragma clang assume_nonnull end


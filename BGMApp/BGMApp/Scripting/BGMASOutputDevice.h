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
//  BGMASOutputDevice.h
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//
//  An AppleScript class for the output devices that can be selected in the preferences menu.
//

// Local Includes
#import "BGMAudioDeviceManager.h"

// System Includes
#import <Foundation/Foundation.h>


#pragma clang assume_nonnull begin

@interface BGMASOutputDevice : NSObject

- (instancetype) initWithAudioObjectID:(AudioObjectID)objID
                          audioDevices:(BGMAudioDeviceManager*)devices
                       parentSpecifier:(NSScriptObjectSpecifier* __nullable)parentSpecifier;

@property (readonly) NSString* name;
@property BOOL selected;  // is this the device to be used for audio output?

@end

#pragma clang assume_nonnull end


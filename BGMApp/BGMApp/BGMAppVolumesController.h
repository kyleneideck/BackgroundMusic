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
//  BGMAppVolumesController.h
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//

// Local Includes
#import "BGMAudioDeviceManager.h"

// System Includes
#import <Cocoa/Cocoa.h>


#pragma clang assume_nonnull begin

@interface BGMAppVolumesController : NSObject

- (id) initWithMenu:(NSMenu*)menu
      appVolumeView:(NSView*)view
       audioDevices:(BGMAudioDeviceManager*)audioDevices;

// See BGMBackgroundMusicDevice::SetAppVolume.
- (void)  setVolume:(SInt32)volume
forAppWithProcessID:(pid_t)processID
           bundleID:(NSString* __nullable)bundleID;

// See BGMBackgroundMusicDevice::SetPanVolume.
- (void) setPanPosition:(SInt32)pan
    forAppWithProcessID:(pid_t)processID
               bundleID:(NSString* __nullable)bundleID;

@end

#pragma clang assume_nonnull end


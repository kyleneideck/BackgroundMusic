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
//  BGMAppVolumes.h
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//

// Local Includes
#import "BGMAudioDeviceManager.h"

// System Includes
#import <Cocoa/Cocoa.h>


@interface BGMAppVolumes : NSObject // todo: rename to BGMAppVolumesMenu 

- (id) initWithMenu:(NSMenu*)menu appVolumeView:(NSView*)view audioDevices:(BGMAudioDeviceManager*)audioDevices;

@end

// Protocol for the UI custom classes

@protocol BGMAppVolumeSubview <NSObject>

- (void) setUpWithApp:(NSRunningApplication*)app context:(BGMAppVolumes*)ctx;

@end

// Custom classes for the UI elements in the app volume menu items

@interface BGMAVM_AppIcon : NSImageView <BGMAppVolumeSubview>
@end

@interface BGMAVM_AppNameLabel : NSTextField <BGMAppVolumeSubview>
@end

@interface BGMAVM_VolumeSlider : NSSlider <BGMAppVolumeSubview>

- (void) setRelativeVolume:(NSNumber*)relativeVolume;

@end


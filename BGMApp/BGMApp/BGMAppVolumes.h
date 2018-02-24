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
//  Copyright © 2016, 2017 Kyle Neideck
//

// Local Includes
#import "BGMAppVolumesController.h"

// System Includes
#import <Cocoa/Cocoa.h>


#pragma clang assume_nonnull begin

@interface BGMAppVolumes : NSObject

- (id) initWithController:(BGMAppVolumesController*)inController
                  bgmMenu:(NSMenu*)inMenu
            appVolumeView:(NSView*)inView;

// Pass -1 for initialVolume or initialPan to leave the volume/pan at its default level.
- (void) insertMenuItemForApp:(NSRunningApplication*)app
                initialVolume:(int)volume
                   initialPan:(int)pan;

- (void) removeMenuItemForApp:(NSRunningApplication*)app;

- (void) removeAllAppVolumeMenuItems;

@end

// Protocol for the UI custom classes

@protocol BGMAppVolumeMenuItemSubview <NSObject>

- (void) setUpWithApp:(NSRunningApplication*)app
              context:(BGMAppVolumes*)ctx
           controller:(BGMAppVolumesController*)ctrl
             menuItem:(NSMenuItem*)item;

@end

// Custom classes for the UI elements in the app volume menu items

@interface BGMAVM_AppIcon : NSImageView <BGMAppVolumeMenuItemSubview>
@end

@interface BGMAVM_AppNameLabel : NSTextField <BGMAppVolumeMenuItemSubview>
@end

@interface BGMAVM_ShowMoreControlsButton : NSButton <BGMAppVolumeMenuItemSubview>
@end

@interface BGMAVM_VolumeSlider : NSSlider <BGMAppVolumeMenuItemSubview>

- (void) setRelativeVolume:(int)relativeVolume;

@end

@interface BGMAVM_PanSlider : NSSlider <BGMAppVolumeMenuItemSubview>

- (void) setPanPosition:(int)panPosition;

@end

#pragma clang assume_nonnull end



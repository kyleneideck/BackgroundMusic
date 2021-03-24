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
//  BGMASApplication.h
//  BGMApp
//
//  Copyright © 2021 Marcus Wu
//  Copyright © 2021 Kyle Neideck
//
//  An AppleScript class for volume and pan settings for running applications.
//


// Local Includes
#import "BGMAppVolumesController.h"

// System Includes
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>


NS_ASSUME_NONNULL_BEGIN

@interface BGMASApplication : NSObject

- (instancetype) initWithApplication:(NSRunningApplication*)app
                          volumeController:(BGMAppVolumesController*)volumeController
                     parentSpecifier:(NSScriptObjectSpecifier* __nullable)parentSpecifier
                               index:(int)i;

@property (readonly) NSString* name;
@property (readonly) NSString* bundleID;
@property int volume;
@property int pan;
@end

NS_ASSUME_NONNULL_END

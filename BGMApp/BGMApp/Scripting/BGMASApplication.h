//
//  BGMASApplication.h
//  Background Music
//
//  Created by Marcus Wu on 3/17/21.
//  Copyright © 2021 Background Music contributors. All rights reserved.
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
@property int volume;
@property int pan;
@end

NS_ASSUME_NONNULL_END

//
//  BGMASApplication.m
//  Background Music
//
//  Created by Marcus Wu on 3/17/21.
//  Copyright © 2021 Background Music contributors. All rights reserved.
//

// Self Include
#import "BGMASApplication.h"

@implementation BGMASApplication {
    NSScriptObjectSpecifier* parentSpecifier;
    NSRunningApplication *application;
    BGMAppVolumesController* appVolumesController;
    int index;
}

- (instancetype) initWithApplication:(NSRunningApplication*)app
                          volumeController:(BGMAppVolumesController*)volumeController
                     parentSpecifier:(NSScriptObjectSpecifier* __nullable)parent
                               index:(int)i {
    if ((self = [super init])) {
        parentSpecifier = parent;
        application = app;
        appVolumesController = volumeController;
        index = i;
    }

    return self;
}

- (NSString*) name {
    return [NSString stringWithFormat:@"%@", [application localizedName]];
}

- (int) volume {
    return [appVolumesController getVolumeAndPanForApp:application].volume;
}

- (void) setVolume:(int)vol {
    BGMAppVolumeAndPan volume = {
        .volume = vol,
        .pan = -1
    };
    [appVolumesController setVolumeAndPan:volume forApp:application];
}

- (int) pan {
    return [appVolumesController getVolumeAndPanForApp:application].pan;
}

- (void) setPan:(int)pan {
    BGMAppVolumeAndPan thePan = {
        .volume = -1,
        .pan = pan
    };
    [appVolumesController setVolumeAndPan:thePan forApp:application];
}

- (NSScriptObjectSpecifier* __nullable) objectSpecifier {
    NSScriptClassDescription* parentClassDescription = [parentSpecifier keyClassDescription];
    return [[NSNameSpecifier alloc] initWithContainerClassDescription:parentClassDescription
                                                   containerSpecifier:parentSpecifier
                                                                  key:@"applications"
                                                                 name:self.name];
}

@end

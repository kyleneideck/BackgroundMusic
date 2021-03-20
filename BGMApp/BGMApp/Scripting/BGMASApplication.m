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
//  BGMASApplication.mm
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//  Created by Marcus Wu on 3/17/21.
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

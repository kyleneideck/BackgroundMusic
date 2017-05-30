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
//  BGMASOutputDevice.mm
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//

// Self Include
#import "BGMASOutputDevice.h"

// Local Includes
#import "BGMAudioDevice.h"

// PublicUtility Includes
#import "CADebugMacros.h"


#pragma clang assume_nonnull begin

@implementation BGMASOutputDevice {
    NSScriptObjectSpecifier* parentSpecifier;
    BGMAudioDevice device;
    BGMAudioDeviceManager* audioDevices;
}

- (instancetype) initWithAudioObjectID:(AudioObjectID)objID
                          audioDevices:(BGMAudioDeviceManager*)devices
                       parentSpecifier:(NSScriptObjectSpecifier* __nullable)parent {
    if ((self = [super init])) {
        parentSpecifier = parent;
        device = objID;
        audioDevices = devices;
    }

    return self;
}

- (NSString*) name {
    return (NSString*)CFBridgingRelease(device.CopyName());
}

- (BOOL) selected {
    return [audioDevices isOutputDevice:device];
}

- (void) setSelected:(BOOL)selected {
    if (selected && ![self selected]) {
        DebugMsg("BGMASOutputDevice::setSelected: A script is setting output device to %s",
                 [[self name] UTF8String]);

        NSError* err = [audioDevices setOutputDeviceWithID:device revertOnFailure:YES];
        (void)err;  // TODO: Return an error to the script somehow if this isn't nil. Also, should
                    //       we return an error if the script tries to set this property to false?     
    }
}

- (NSScriptObjectSpecifier* __nullable) objectSpecifier {
    NSScriptClassDescription* parentClassDescription = [parentSpecifier keyClassDescription];
    return [[NSNameSpecifier alloc] initWithContainerClassDescription:parentClassDescription
                                                   containerSpecifier:parentSpecifier
                                                                  key:@"output devices"
                                                                 name:self.name];
}

@end

#pragma clang assume_nonnull end


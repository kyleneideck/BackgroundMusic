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
//  BGMAppDelegate+AppleScript.mm
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//  Copyright © 2021 Marcus Wu
//

// Self Include
#import "BGMAppDelegate+AppleScript.h"

// Local Includes
#import "BGMAudioDevice.h"

// PublicUtility Includes
#import "CAHALAudioSystemObject.h"
#import "CAAutoDisposer.h"

const AudioObjectPropertyScope kScope                   = kAudioDevicePropertyScopeOutput;

#pragma clang assume_nonnull begin

@implementation BGMAppDelegate (AppleScript)

- (BOOL) application:(NSApplication*)sender delegateHandlesKey:(NSString*)key {
    #pragma unused (sender)

    if (![key isEqual:@"_keyWindow"]) {
        DebugMsg("BGMAppDelegate:application:delegateHandlesKey: Key queried: '%s'",
                 [key UTF8String]);
    }

    return [@[@"selectedOutputDevice", @"outputDevices", @"mainVolume", @"applications"] containsObject:key];
}

- (BGMASOutputDevice*) selectedOutputDevice {
    AudioObjectID outputDeviceID = [self.audioDevices outputDevice].GetObjectID();

    return [[BGMASOutputDevice alloc] initWithAudioObjectID:outputDeviceID
                                               audioDevices:self.audioDevices
                                            parentSpecifier:[self objectSpecifier]];
}

- (void) setSelectedOutputDevice:(BGMASOutputDevice*)device {
    [device setSelected:YES];
}

- (NSArray<BGMASOutputDevice*>*) outputDevices {
    UInt32 numDevices = CAHALAudioSystemObject().GetNumberAudioDevices();
    
    CAAutoArrayDelete<AudioObjectID> devices(numDevices);
    CAHALAudioSystemObject().GetAudioDevices(numDevices, devices);

    NSMutableArray<BGMASOutputDevice*>* outputDevices =
        [NSMutableArray arrayWithCapacity:numDevices];

    for (UInt32 i = 0; i < numDevices; i++) {
        BGMAudioDevice device(devices[i]);

        if (device.CanBeOutputDeviceInBGMApp()) {
            BGMASOutputDevice* outputDevice =
                [[BGMASOutputDevice alloc] initWithAudioObjectID:device.GetObjectID()
                                                    audioDevices:self.audioDevices
                                                 parentSpecifier:[self objectSpecifier]];

            [outputDevices addObject:outputDevice];
        }
    }

    return outputDevices;
}

- (double) mainVolume {
    BGMAudioDevice bgmDevice = [self.audioDevices bgmDevice];
    return bgmDevice.GetVolumeControlScalarValue(kScope, kMasterChannel);
}

- (void) setMainVolume:(double)mainVolume {
    BGMAudioDevice bgmDevice = [self.audioDevices bgmDevice];
    bgmDevice.SetMasterVolumeScalar(kScope, (Float32)mainVolume);
    [self.outputVolumeSlider setFloatValue:(float)mainVolume];
}

- (NSArray<BGMASApplication*>*) applications {
    NSArray<NSRunningApplication*>* apps = [[NSWorkspace sharedWorkspace] runningApplications];
    NSMutableArray<BGMASApplication*>* applications = [NSMutableArray arrayWithCapacity:[apps count]];

    for (UInt32 i = 0; i < [apps count]; i++) {
        BGMASApplication *app = [[BGMASApplication alloc] initWithApplication:apps[i] volumeController:self.appVolumes parentSpecifier:[self objectSpecifier] index:i];

        [applications addObject:app];
    }

    return applications;
}

@end

#pragma clang assume_nonnull end


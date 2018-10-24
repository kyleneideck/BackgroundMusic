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
//  BGMPreferredOutputDevices.m
//  BGMApp
//
//  Copyright © 2018 Kyle Neideck
//

// Self Include
#import "BGMPreferredOutputDevices.h"

// Local Includes
#import "BGM_Types.h"
#import "BGM_Utils.h"
#import "BGMAudioDevice.h"

// PublicUtility Includes
#import "CAAutoDisposer.h"
#import "CAHALAudioSystemObject.h"
#import "CAPropertyAddress.h"


#pragma clang assume_nonnull begin

// The Plist file CoreAudio stores the preferred devices list in. It isn't part of the public API,
// but if this class fails to read/parse it, BGMApp will continue without it.
NSString* const kAudioSystemSettingsPlist =
    @"/Library/Preferences/Audio/com.apple.audio.SystemSettings.plist";

@implementation BGMPreferredOutputDevices {
    NSRecursiveLock* _stateLock;

    // Used to change BGMApp's output device.
    BGMAudioDeviceManager* _devices;

    // The UIDs of the preferred devices, in order of preference. The most-preferred device is at
    // index 0.
    NSArray<NSString*>* _preferredDevices;

    // Called when a device is connected or disconnected.
    AudioObjectPropertyListenerBlock _deviceListListener;
}

- (instancetype) initWithDevices:(BGMAudioDeviceManager*)devices {
    if ((self = [super init])) {
        _stateLock = [NSRecursiveLock new];
        _devices = devices;
        _preferredDevices = [self readPreferredDevices];

        DebugMsg("BGMPreferredOutputDevices::initWithDevices: Preferred devices: %s",
                 _preferredDevices.debugDescription.UTF8String);

        [self listenForDevicesAddedOrRemoved];
    }

    return self;
}

- (void) dealloc {
    @try {
        [_stateLock lock];

        // Tell CoreAudio not to call the listener block anymore.
        CAHALAudioSystemObject().RemovePropertyListenerBlock(
            CAPropertyAddress(kAudioHardwarePropertyDevices),
            dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0),
            _deviceListListener);
    } @finally {
        [_stateLock unlock];
    }
}

// Reads the preferred devices list from CoreAudio's Plist file.
- (NSArray<NSString*>*) readPreferredDevices {
    // Read the Plist file into a dictionary.
    //
    // TODO: If this file doesn't exist, try paths used by older versions of macOS.
    NSURL* url = [NSURL fileURLWithPath:kAudioSystemSettingsPlist];
    NSError* error;
    NSData* data = [NSData dataWithContentsOfURL:url options:0 error:&error];
    NSDictionary* settings = [NSPropertyListSerialization propertyListWithData:data
                                                                       options:0
                                                                        format:nil
                                                                         error:&error];
    // Check the Plist dict includes the preferred devices.
    if (error) {
        LogWarning("BGMPreferredOutputDevices::readPreferredDevices: Couldn't read %s. Error: %s",
                   kAudioSystemSettingsPlist.UTF8String,
                   error.debugDescription.UTF8String);
        return @[];
    }

    if (!settings[@"preferred devices"]) {
        LogWarning("BGMPreferredOutputDevices::readPreferredDevices: No preferred devices in %s",
                   settings.debugDescription.UTF8String);
        return @[];
    }

    if (!settings[@"preferred devices"][@"output"]) {
        LogWarning("BGMPreferredOutputDevices::readPreferredDevices: "
                   "No preferred output devices in %s",
                   settings.debugDescription.UTF8String);
        return @[];
    }

    // Copy the preferred devices out of the Plist dict.
    NSArray<NSString*>* deviceUIDs = @[];

    for (NSDictionary* deviceInfo in settings[@"preferred devices"][@"output"]) {
        if ([deviceInfo[@"uid"] isKindOfClass:NSString.class]) {
            // Add this preferred device's UID to the list.
            NSString* uid = deviceInfo[@"uid"];

            // Skip any Background Music devices.
            if (![uid isEqualToString:@kBGMDeviceUID] &&
                ![uid isEqualToString:@kBGMDeviceUID_UISounds] &&
                ![uid isEqualToString:@kBGMNullDeviceUID]) {
                deviceUIDs = [deviceUIDs arrayByAddingObject:uid];
            }
        } else {
            LogWarning("BGMPreferredOutputDevices::readPreferredDevices: No UID in %s",
                       deviceInfo.debugDescription.UTF8String);
        }
    }

    return deviceUIDs;
}

- (void) listenForDevicesAddedOrRemoved {
    // Create the block that will run when a device is added or removed.
    BGMPreferredOutputDevices* __weak weakSelf = self;

    _deviceListListener = ^(UInt32 inNumberAddresses,
                            const AudioObjectPropertyAddress* inAddresses) {
#pragma unused (inNumberAddresses, inAddresses)

        BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] () {
            [weakSelf connectedDeviceListChanged];
        });
    };

    // Register the listener block with CoreAudio.
    CAHALAudioSystemObject().AddPropertyListenerBlock(
        CAPropertyAddress(kAudioHardwarePropertyDevices),
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0),
        _deviceListListener);
}

- (void) connectedDeviceListChanged {
    @try {
        [_stateLock lock];

        // Decide which device should be the output device now. If a device has been connected and
        // it's preferred over the current output device, we'll change to that device. If the
        // current output device has been removed, we'll change to the next most-preferred device.
        AudioObjectID preferredDevice = [self findPreferredDevice];

        if (preferredDevice == kAudioObjectUnknown) {
            // The current output device was disconnected and there are no preferred devices
            // connected, so pick one arbitrarily.
            DebugMsg("BGMPreferredOutputDevices::connectedDeviceListChanged: "
                     "Changing to an arbitrary output device.");
            [_devices setOutputDeviceByLatency];
        } else if (_devices.outputDevice.GetObjectID() != preferredDevice) {
            // Change to the preferred device.
            DebugMsg("BGMPreferredOutputDevices::connectedDeviceListChanged: "
                     "Changing output device to %d.",
                     preferredDevice);
            NSError* __nullable error = [_devices setOutputDeviceWithID:preferredDevice
                                                        revertOnFailure:YES];
            if (error) {
                // There's not much we can do if this happens.
                LogError("BGMPreferredOutputDevices::connectedDeviceListChanged: "
                         "Failed to change to preferred device. Error: %s",
                         error.debugDescription.UTF8String);
            }
        }
    } @finally {
        [_stateLock unlock];
    }
}

// Returns the most-preferred device currently connected. If no preferred devices are connected,
// returns either the current output device or, if it's been disconnected, kAudioObjectUnknown.
- (AudioObjectID) findPreferredDevice {
    CAHALAudioSystemObject audioSystem;
    UInt32 numDevices = audioSystem.GetNumberAudioDevices();

    if (numDevices == 0) {
        LogWarning("BGMPreferredOutputDevices::findPreferredDevice: No devices found.");
        return kAudioObjectUnknown;
    }

    // Get the list of currently connected audio devices.
    CAAutoArrayDelete<AudioObjectID> devices(numDevices);
    audioSystem.GetAudioDevices(numDevices, devices);

    // Look through the preferred devices list to see if one's connected. Return the first one we
    // find because they're stored in order of preference.
    for (int position = 0; position < _preferredDevices.count; position++) {
        // Compare the current preferred device to each connected device by UID.
        for (UInt32 i = 0; i < numDevices; i++) {
            // Skip devices we can't use, e.g. BGMDevice.
            if (!BGMAudioDevice(devices[i]).CanBeOutputDeviceInBGMApp()) {
                continue;
            }

            // Get the connected device's UID.
            NSString* __nullable connectedDeviceUID =
                (__bridge NSString* __nullable)CAHALAudioDevice(devices[i]).CopyDeviceUID();

            // If the UIDs match, the current preferred device is connected.
            if ([connectedDeviceUID isEqualToString:_preferredDevices[position]]) {
                // We're iterating through the preferred devices from most to least-preferred, so
                // we've found the device to use.
                DebugMsg("BGMPreferredOutputDevices::findPreferredDevice: "
                         "Found preferred device '%s' at position %d",
                         _preferredDevices[position].UTF8String,
                         position);
                return devices[i];
            }
        }

        DebugMsg("BGMPreferredOutputDevices::findPreferredDevice: "
                 "Preferred device not connected: %s",
                 _preferredDevices[position].UTF8String);
    }

    DebugMsg("BGMPreferredOutputDevices::findPreferredDevice: No preferred devices found.");
    return [self currentOutputDeviceStillConnected:devices numDevices:numDevices] ?
                _devices.outputDevice.GetObjectID() : kAudioObjectUnknown;
}

- (bool) currentOutputDeviceStillConnected:(const CAAutoArrayDelete<AudioObjectID>&)devices
                                numDevices:(UInt32)numDevices {
    // Look for the current output device in the list of connected devices.
    for (UInt32 i = 0; i < numDevices; i++) {
        if (_devices.outputDevice.GetObjectID() == devices[i]) {
            return true;
        }
    }

    DebugMsg("BGMPreferredOutputDevices::currentOutputDeviceStillConnected: "
             "Output device was removed.");
    return false;
}

- (void) userChangedOutputDeviceTo:(AudioObjectID)device {
    @try {
        [_stateLock lock];

        BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] () {
            // Add the new output device to the list.
            NSString* __nullable outputDeviceUID =
                (__bridge NSString* __nullable)CAHALAudioDevice(device).CopyDeviceUID();

            if (outputDeviceUID) {
                // Limit the list to three devices because that's what macOS does.
                if (_preferredDevices.count >= 2) {
                    _preferredDevices = @[BGMNN(outputDeviceUID),
                                          _preferredDevices[0],
                                          _preferredDevices[1]];
                } else if (_preferredDevices.count >= 1) {
                    _preferredDevices = @[BGMNN(outputDeviceUID), _preferredDevices[0]];
                } else {
                    _preferredDevices = @[BGMNN(outputDeviceUID)];
                }

                DebugMsg("BGMPreferredOutputDevices::outputDeviceWillChangeTo: "
                         "Preferred devices: %s",
                         _preferredDevices.debugDescription.UTF8String);
            } else {
                LogWarning("BGMPreferredOutputDevices::outputDeviceWillChangeTo: "
                           "Output device has no UID");
            }
        });
    } @finally {
        [_stateLock unlock];
    }
}

@end

#pragma clang assume_nonnull end


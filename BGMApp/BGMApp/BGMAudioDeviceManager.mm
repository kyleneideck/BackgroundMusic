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
//  BGMAudioDeviceManager.mm
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#import "BGMAudioDeviceManager.h"

// Local Includes
#include "BGM_Types.h"
#include "BGMDeviceControlSync.h"
#include "BGMPlayThrough.h"

// PublicUtility Includes
#include "CAHALAudioSystemObject.h"
#include "CAAutoDisposer.h"


int const kBGMErrorCode_BGMDeviceNotFound = 0;
int const kBGMErrorCode_OutputDeviceNotFound = 1;

// Hack/workaround that adds a default constructor to CAHALAudioDevice so we don't have to use pointers for the instance variables
class BGMAudioDevice : public CAHALAudioDevice {
    using CAHALAudioDevice::CAHALAudioDevice;
public:
    BGMAudioDevice() : CAHALAudioDevice(kAudioDeviceUnknown) { }
};

@implementation BGMAudioDeviceManager {
    BGMAudioDevice bgmDevice;
    BGMAudioDevice outputDevice;
    BGMDeviceControlSync deviceControlSync;
    BGMPlayThrough playThrough;
}

#pragma mark Construction/Destruction

- (id) initWithError:(NSError**)error {
    if ((self = [super init])) {
        bgmDevice = BGMAudioDevice(CFSTR(kBGMDeviceUID));
        
        if (bgmDevice.GetObjectID() == kAudioObjectUnknown) {
            DebugMsg("BGMAudioDeviceManager::initWithError: BGMDevice not found");
            if (error) {
                *error = [NSError errorWithDomain:@kBGMAppBundleID code:kBGMErrorCode_BGMDeviceNotFound userInfo:nil];
            }
            self = nil;
            return self;
        }
        
        [self initOutputDevice];
        
        
        if (outputDevice.GetObjectID() == kAudioDeviceUnknown) {
            DebugMsg("BGMAudioDeviceManager::initWithError: output device not found");
            if (error) {
                *error = [NSError errorWithDomain:@kBGMAppBundleID code:kBGMErrorCode_OutputDeviceNotFound userInfo:nil];
            }
            self = nil;
            return self;
        }
    }
    
    return self;
}

- (void) initOutputDevice {
    CAHALAudioSystemObject audioSystem;
    // outputDevice = BGMAudioDevice(CFSTR("AppleHDAEngineOutput:1B,0,1,1:0"));
    AudioObjectID defaultDeviceID = audioSystem.GetDefaultAudioDevice(false, false);
    if (defaultDeviceID == bgmDevice.GetObjectID()) {
        // TODO: If BGMDevice is already the default (because BGMApp didn't shutdown properly or it was set manually)
        //       we should temporarily disable BGMDevice so we can find out what the previous default was.
        
        // For now, just pick the device with the lowest latency
        UInt32 numDevices = audioSystem.GetNumberAudioDevices();
        if (numDevices > 0) {
            SInt32 minLatencyDeviceIdx = -1;
            UInt32 minLatency = UINT32_MAX;
            CAAutoArrayDelete<AudioObjectID> devices(numDevices);
            audioSystem.GetAudioDevices(numDevices, devices);
            
            for (UInt32 i = 0; i < numDevices; i++) {
                BGMAudioDevice device(devices[i]);
                
                BOOL isBGMDevice = device.GetObjectID() == bgmDevice.GetObjectID();
                BOOL hasOutputChannels = device.GetTotalNumberChannels(/* inIsInput = */ false) > 0;
                
                if (!isBGMDevice && hasOutputChannels) {
                    if (minLatencyDeviceIdx == -1) {
                        // First, look for any device other than BGMDevice
                        minLatencyDeviceIdx = i;
                    } else if (device.GetLatency(false) < minLatency) {
                        // Then compare the devices by their latencies
                        minLatencyDeviceIdx = i;
                        minLatency = device.GetLatency(false);
                    }
                }
            }
            
            [self setOutputDeviceWithID:devices[minLatencyDeviceIdx] revertOnFailure:NO];
        }
    } else {
        [self setOutputDeviceWithID:defaultDeviceID revertOnFailure:NO];
    }
    
    assert(outputDevice.GetObjectID() != bgmDevice.GetObjectID());
    
    // Log message
    if (outputDevice.GetObjectID() == kAudioDeviceUnknown) {
        CFStringRef outputDeviceUID = outputDevice.CopyDeviceUID();
        DebugMsg("BGMAudioDeviceManager::initDevices: Set output device to %s",
                 CFStringGetCStringPtr(outputDeviceUID, kCFStringEncodingUTF8));
        CFRelease(outputDeviceUID);
    }
}

#pragma mark Systemwide Default Device

- (void) setBGMDeviceAsOSDefault {
    CAHALAudioSystemObject audioSystem;
    
    @synchronized (self) {
        if (audioSystem.GetDefaultAudioDevice(false, true) == outputDevice.GetObjectID()) {
            // The default system device was the same as the default device, so change that as well
            audioSystem.SetDefaultAudioDevice(false, true, bgmDevice.GetObjectID());
        }
        
        audioSystem.SetDefaultAudioDevice(false, false, bgmDevice.GetObjectID());
    }
}

- (void) unsetBGMDeviceAsOSDefault {
    CAHALAudioSystemObject audioSystem;
    
    @synchronized (self) {
        if (audioSystem.GetDefaultAudioDevice(false, true) == bgmDevice.GetObjectID()) {
            // We changed the system output device to BGMDevice, which we only do if it initially matches the
            // default output device, so change it back
            audioSystem.SetDefaultAudioDevice(false, true, outputDevice.GetObjectID());
        }
        
        if (audioSystem.GetDefaultAudioDevice(false, false) == bgmDevice.GetObjectID()) {
            audioSystem.SetDefaultAudioDevice(false, false, outputDevice.GetObjectID());
        }
    }
}

#pragma mark Accessors

- (CAHALAudioDevice) bgmDevice {
    return bgmDevice;
}

- (BOOL) isOutputDevice:(AudioObjectID)deviceID {
    @synchronized (self) {
        return deviceID == outputDevice.GetObjectID();
    }
}

- (BOOL) setOutputDeviceWithID:(AudioObjectID)deviceID revertOnFailure:(BOOL)revertOnFailure {
    DebugMsg("BGMAudioDeviceManager::setOutputDeviceWithID: Setting output device. deviceID=%u", deviceID);
    
    // Set up playthrough and control sync
    BGMAudioDevice newOutputDevice(deviceID);
    
    try {
        @synchronized (self) {
            // Mirror changes in BGMDevice's controls to the new output device's.
            deviceControlSync = BGMDeviceControlSync(bgmDevice, newOutputDevice);
            
            // Stream audio from BGMDevice to the output device.
            //
            // TODO: Should this be done async? Some output devices take a long time to start IO (e.g. AirPlay) and I
            //       assume this blocks the main thread. Haven't tried it to check, though.
            playThrough = BGMPlayThrough(bgmDevice, newOutputDevice);

            outputDevice = BGMAudioDevice(deviceID);
        }
        
        // Start playthrough because audio might be playing.
        //
        // TODO: If audio isn't playing, this makes playthrough run until the user plays audio and then stops it again,
        //       which wastes CPU. I think we could just have Start() call StopIfIdle(), but I haven't tried it yet.
        playThrough.Start();
    } catch (CAException e) {
        // Using LogWarning from PublicUtility instead of NSLog here crashes from a bad access. Not sure why.
        NSLog(@"BGMAudioDeviceManager::setOutputDeviceWithID: Couldn't set device with ID %u as output device. %s %s%d.",
              newOutputDevice.GetObjectID(),
              (revertOnFailure ? "Will attempt to revert to the previous device." : ""),
              "Error: ", e.GetError());
        
        if (revertOnFailure) {
            // Try to reactivate the original device listener and playthrough
            [self setOutputDeviceWithID:outputDevice.GetObjectID() revertOnFailure:NO];
            return NO;
        } else {
            // TODO: Handle in callers. (Maybe show an error dialog and try to set the original default device as the
            //       output device.)
            Throw(e);
        }
    }
    
    return YES;
}

- (OSStatus) waitForOutputDeviceToStart {
    @synchronized (self) {
        return playThrough.WaitForOutputDeviceToStart();
    }
}

@end


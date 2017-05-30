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
//  Copyright © 2016, 2017 Kyle Neideck
//

// Self Include
#import "BGMAudioDeviceManager.h"

// Local Includes
#include "BGM_Types.h"
#include "BGM_Utils.h"
#include "BGMDeviceControlSync.h"
#include "BGMPlayThrough.h"
#include "BGMAudioDevice.h"

// PublicUtility Includes
#include "CAHALAudioSystemObject.h"
#include "CAAutoDisposer.h"


int const kBGMErrorCode_BGMDeviceNotFound = 0;
int const kBGMErrorCode_OutputDeviceNotFound = 1;

@implementation BGMAudioDeviceManager {
    BGMAudioDevice bgmDevice;
    BGMAudioDevice outputDevice;
    
    BGMDeviceControlSync deviceControlSync;
    BGMPlayThrough playThrough;
    
    NSRecursiveLock* stateLock;
}

#pragma mark Construction/Destruction

- (id) initWithError:(NSError**)error {
    if ((self = [super init])) {
        stateLock = [NSRecursiveLock new];
        
        bgmDevice = BGMAudioDevice(CFSTR(kBGMDeviceUID));
        
        if (bgmDevice.GetObjectID() == kAudioObjectUnknown) {
            LogError("BGMAudioDeviceManager::initWithError: BGMDevice not found");
            
            if (error) {
                *error = [NSError errorWithDomain:@kBGMAppBundleID code:kBGMErrorCode_BGMDeviceNotFound userInfo:nil];
            }
            
            self = nil;
            return self;
        }
        
        [self initOutputDevice];
        
        if (outputDevice.GetObjectID() == kAudioObjectUnknown) {
            LogError("BGMAudioDeviceManager::initWithError: output device not found");
            
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
            
            BGMLogUnexpectedExceptionsMsg("BGMAudioDeviceManager::initOutputDevice",
                                          "setOutputDeviceWithID:devices[minLatencyDeviceIdx]", [&]() {
                // TODO: On error, try a different output device.
                [self setOutputDeviceWithID:devices[minLatencyDeviceIdx] revertOnFailure:NO];
            });
        }
    } else {
        BGMLogUnexpectedExceptionsMsg("BGMAudioDeviceManager::initOutputDevice",
                                      "setOutputDeviceWithID:defaultDeviceID", [&]() {
            // TODO: Return the error from setOutputDeviceWithID so it can be returned by initWithError.
            [self setOutputDeviceWithID:defaultDeviceID revertOnFailure:NO];
        });
    }
    
    assert(outputDevice.GetObjectID() != bgmDevice.GetObjectID());
    
    // Log message
    if (outputDevice.GetObjectID() == kAudioObjectUnknown) {
        CFStringRef outputDeviceUID = outputDevice.CopyDeviceUID();
        DebugMsg("BGMAudioDeviceManager::initDevices: Set output device to %s",
                 CFStringGetCStringPtr(outputDeviceUID, kCFStringEncodingUTF8));
        CFRelease(outputDeviceUID);
    }
}

#pragma mark Systemwide Default Device

// Note that there are two different "default" output devices on OS X: "output" and "system output". See
// AudioHardwarePropertyDefaultSystemOutputDevice in AudioHardware.h.

- (NSError* __nullable) setBGMDeviceAsOSDefault {
    DebugMsg("BGMAudioDeviceManager::setBGMDeviceAsOSDefault: Setting the system's default audio "
             "device to BGMDevice");

    CAHALAudioSystemObject audioSystem;
    
    AudioDeviceID bgmDeviceID = kAudioObjectUnknown;
    AudioDeviceID outputDeviceID = kAudioObjectUnknown;
    
    @try {
        [stateLock lock];

        bgmDeviceID = bgmDevice.GetObjectID();
        outputDeviceID = outputDevice.GetObjectID();
    } @finally {
        [stateLock unlock];
    }

    if (outputDeviceID == kAudioObjectUnknown) {
        return [NSError errorWithDomain:@kBGMAppBundleID code:kBGMErrorCode_OutputDeviceNotFound userInfo:nil];
    }
    if (bgmDeviceID == kAudioObjectUnknown) {
        return [NSError errorWithDomain:@kBGMAppBundleID code:kBGMErrorCode_BGMDeviceNotFound userInfo:nil];
    }

    try {
        AudioDeviceID currentDefault = audioSystem.GetDefaultAudioDevice(false, true);
        
        try {
            if (currentDefault == outputDeviceID) {
                // The default system device was the same as the default device, so change that as well
                audioSystem.SetDefaultAudioDevice(false, true, bgmDeviceID);
            }
        
            audioSystem.SetDefaultAudioDevice(false, false, bgmDeviceID);
        } catch (CAException e) {
            NSLog(@"SetDefaultAudioDevice threw CAException (%d)", e.GetError());
            return [NSError errorWithDomain:@kBGMAppBundleID code:e.GetError() userInfo:nil];
        }
    } catch (...) {
        NSLog(@"Unexpected exception");
        return [NSError errorWithDomain:@kBGMAppBundleID code:-1 userInfo:nil];
    }
    
    return nil;
}

- (NSError* __nullable) unsetBGMDeviceAsOSDefault {
    CAHALAudioSystemObject audioSystem;
    
    bool bgmDeviceIsDefault = true;
    bool bgmDeviceIsSystemDefault = true;
    
    AudioDeviceID bgmDeviceID = kAudioObjectUnknown;
    AudioDeviceID outputDeviceID = kAudioObjectUnknown;
    
    @try {
        [stateLock lock];

        bgmDeviceID = bgmDevice.GetObjectID();
        outputDeviceID = outputDevice.GetObjectID();

        BGMLogAndSwallowExceptions("unsetBGMDeviceAsOSDefault", [&]() {
            bgmDeviceIsDefault =
                (audioSystem.GetDefaultAudioDevice(false, false) == bgmDeviceID);
            
            bgmDeviceIsSystemDefault =
                (audioSystem.GetDefaultAudioDevice(false, true) == bgmDeviceID);
        });
    } @finally {
        [stateLock unlock];
    }

    if (outputDeviceID == kAudioObjectUnknown) {
        return [NSError errorWithDomain:@kBGMAppBundleID code:kBGMErrorCode_OutputDeviceNotFound userInfo:nil];
    }
    if (bgmDeviceID == kAudioObjectUnknown) {
        return [NSError errorWithDomain:@kBGMAppBundleID code:kBGMErrorCode_BGMDeviceNotFound userInfo:nil];
    }
    
    if (bgmDeviceIsDefault) {
        DebugMsg("BGMAudioDeviceManager::unsetBGMDeviceAsOSDefault: Setting the system's default output "
                 "device back to device %d", outputDeviceID);
        
        try {
            audioSystem.SetDefaultAudioDevice(false, false, outputDeviceID);
        } catch (CAException e) {
            return [NSError errorWithDomain:@kBGMAppBundleID code:e.GetError() userInfo:nil];
        } catch (...) {
            BGMLogUnexpectedExceptionIn("BGMAudioDeviceManager::unsetBGMDeviceAsOSDefault "
                                        "SetDefaultAudioDevice (output)");
        }
    }
    
    // If we changed the default system output device to BGMDevice, which we only do if it's set to
    // the same device as the default output device, change it back to the previous device.
    if (bgmDeviceIsSystemDefault) {
        DebugMsg("BGMAudioDeviceManager::unsetBGMDeviceAsOSDefault: Setting the system's default system "
                 "output device back to device %d", outputDeviceID);
        
        try {
            audioSystem.SetDefaultAudioDevice(false, true, outputDeviceID);
        } catch (CAException e) {
            return [NSError errorWithDomain:@kBGMAppBundleID code:e.GetError() userInfo:nil];
        } catch (...) {
            BGMLogUnexpectedExceptionIn("BGMAudioDeviceManager::unsetBGMDeviceAsOSDefault "
                                        "SetDefaultAudioDevice (system output)");
        }
    }
    
    return nil;
}

#pragma mark Accessors

- (CAHALAudioDevice) bgmDevice {
    return bgmDevice;
}

- (CAHALAudioDevice) outputDevice {
    return outputDevice;
}

- (BOOL) isOutputDevice:(AudioObjectID)deviceID {
    @try {
        [stateLock lock];
        return deviceID == outputDevice.GetObjectID();
    } @finally {
        [stateLock unlock];
    }
}

- (BOOL) isOutputDataSource:(UInt32)dataSourceID {
    @try {
        [stateLock lock];
        
        try {
            AudioObjectPropertyScope scope = kAudioDevicePropertyScopeOutput;
            UInt32 channel = 0;
            
            return outputDevice.HasDataSourceControl(scope, channel) &&
                (dataSourceID == outputDevice.GetCurrentDataSourceID(scope, channel));
        } catch (CAException e) {
            BGMLogException(e);
            return false;
        }
    } @finally {
        [stateLock unlock];
    }
}


- (NSError* __nullable) setOutputDeviceWithID:(AudioObjectID)deviceID
                              revertOnFailure:(BOOL)revertOnFailure {
    return [self setOutputDeviceWithIDImpl:deviceID
                              dataSourceID:nil
                           revertOnFailure:revertOnFailure];
}

- (NSError* __nullable) setOutputDeviceWithID:(AudioObjectID)deviceID
                                 dataSourceID:(UInt32)dataSourceID
                              revertOnFailure:(BOOL)revertOnFailure {
    return [self setOutputDeviceWithIDImpl:deviceID
                              dataSourceID:&dataSourceID
                           revertOnFailure:revertOnFailure];
}

- (NSError* __nullable) setOutputDeviceWithIDImpl:(AudioObjectID)newDeviceID
                                     dataSourceID:(UInt32* __nullable)dataSourceID
                                  revertOnFailure:(BOOL)revertOnFailure {
    DebugMsg("BGMAudioDeviceManager::setOutputDeviceWithID: Setting output device. newDeviceID=%u",
             newDeviceID);
    
    AudioDeviceID currentDeviceID = outputDevice.GetObjectID();  // (GetObjectID doesn't throw.)
    
    // Set up playthrough and control sync
    BGMAudioDevice newOutputDevice(newDeviceID);
    
    @try {
        [stateLock lock];
        
        try {
            // Re-read the device ID after entering the monitor. (The initial read is because
            // currentDeviceID is used in the catch blocks.)
            currentDeviceID = outputDevice.GetObjectID();
            
            if (newDeviceID != currentDeviceID) {
                // Deactivate playthrough rather than stopping it so it can't be started by HAL
                // notifications while we're updating deviceControlSync.
                playThrough.Deactivate();

                deviceControlSync.SetDevices(bgmDevice, newOutputDevice);
                deviceControlSync.Activate();

                // Stream audio from BGMDevice to the new output device. This blocks while the old device
                // stops IO.
                playThrough.SetDevices(&bgmDevice, &newOutputDevice);
                playThrough.Activate();

                outputDevice = newOutputDevice;
            }
            
            // Set the output device to use the new data source.
            if (dataSourceID) {
                // TODO: If this fails, ideally we'd still start playthrough and return an error, but not
                //       revert the device. It would probably be a bit awkward, though.
                [self setDataSource:*dataSourceID device:outputDevice];
            }
            
            if (newDeviceID != currentDeviceID) {
                // We successfully changed to the new device. Start playthrough on it, since audio might be
                // playing. (If we only changed the data source, playthrough will already be running if it
                // needs to be.)
                playThrough.Start();
                // But stop playthrough if audio isn't playing, since it uses CPU.
                playThrough.StopIfIdle();
            }
        } catch (CAException e) {
            BGMAssert(e.GetError() != kAudioHardwareNoError,
                      "CAException with kAudioHardwareNoError");
            
            return [self failedToSetOutputDevice:newDeviceID
                                       errorCode:e.GetError()
                                        revertTo:(revertOnFailure ? &currentDeviceID : nullptr)];
        } catch (...) {
            return [self failedToSetOutputDevice:newDeviceID
                                       errorCode:kAudioHardwareUnspecifiedError
                                        revertTo:(revertOnFailure ? &currentDeviceID : nullptr)];
        }
    } @finally {
        [stateLock unlock];
    }

    return nil;
}

- (void) setDataSource:(UInt32)dataSourceID device:(BGMAudioDevice)device {
    BGMLogAndSwallowExceptions("BGMAudioDeviceManager::setDataSource", [&]() {
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeOutput;
        UInt32 channel = 0;

        if (device.DataSourceControlIsSettable(scope, channel)) {
            DebugMsg("BGMAudioDeviceManager::setOutputDeviceWithID: Setting dataSourceID=%u",
                     dataSourceID);
            
            device.SetCurrentDataSourceByID(scope, channel, dataSourceID);
        }
    });
}

- (NSError*) failedToSetOutputDevice:(AudioDeviceID)deviceID
                           errorCode:(OSStatus)errorCode
                            revertTo:(AudioDeviceID*)revertTo {
    // Using LogWarning from PublicUtility instead of NSLog here crashes from a bad access. Not sure why.
    NSLog(@"BGMAudioDeviceManager::failedToSetOutputDevice: Couldn't set device with ID %u as output device. "
          "%s%d. %@",
          deviceID,
          "Error: ", errorCode,
          (revertTo ? [NSString stringWithFormat:@"Will attempt to revert to the previous device. "
                                                  "Previous device ID: %u.", *revertTo] : @""));
    
    NSDictionary* __nullable info = nil;
    
    if (revertTo) {
        // Try to reactivate the original device listener and playthrough. (Sorry about the mutual recursion.)
        NSError* __nullable revertError = [self setOutputDeviceWithID:*revertTo revertOnFailure:NO];
        
        if (revertError) {
            info = @{ @"revertError": (NSError*)revertError };
        }
    } else {
        // TODO: Handle this error better in callers. Maybe show an error dialog and try to set the original
        //       default device as the output device.
        NSLog(@"BGMAudioDeviceManager::failedToSetOutputDevice: Failed to revert to the previous device.");
    }
    
    return [NSError errorWithDomain:@kBGMAppBundleID code:errorCode userInfo:info];
}

- (OSStatus) waitForOutputDeviceToStart {
    // We can only try for stateLock because setOutputDeviceWithID might have already taken it, then made a
    // HAL request to BGMDevice and is now waiting for the response. Some of the requests setOutputDeviceWithID
    // makes to BGMDevice block in the HAL if another thread is in BGM_Device::StartIO.
    //
    // Since BGM_Device::StartIO calls this method (via XPC), waiting for setOutputDeviceWithID to release
    // stateLock could cause deadlocks. Instead we return early with an error code that BGMDriver knows to
    // ignore, since the output device is (almost certainly) being changed and we can't avoid dropping frames
    // while the output device starts up.
    OSStatus err;
    BOOL gotLock;
    
    @try {
        gotLock = [stateLock tryLock];
        
        if (gotLock) {
            err = playThrough.WaitForOutputDeviceToStart();
        } else {
            LogWarning("BGMAudioDeviceManager::waitForOutputDeviceToStart: Didn't get state lock. Returning "
                       "early with kDeviceNotStarting.");
            err = BGMPlayThrough::kDeviceNotStarting;
        }
        
        if (err == BGMPlayThrough::kDeviceNotStarting) {
            // I'm not sure if this block is currently reachable, but BGMDriver only starts waiting on the
            // output device when IO is starting, so we should start playthrough even if BGMApp hasn't been
            // notified by the HAL yet.
            LogWarning("BGMAudioDeviceManager::waitForOutputDeviceToStart: Playthrough wasn't starting the "
                       "output device. Will tell it to and then return early with kDeviceNotStarting.");

            dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
                @try {
                    [stateLock lock];
                    
                    BGMLogAndSwallowExceptions("BGMAudioDeviceManager::waitForOutputDeviceToStart", [&]() {
                        playThrough.Start();
                        playThrough.StopIfIdle();
                    });
                } @finally {
                    [stateLock unlock];
                }
            });
        }
    } @finally {
        if (gotLock) {
            [stateLock unlock];
        }
    }
    
    return err;
}

@end


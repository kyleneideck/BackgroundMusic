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
//  Copyright © 2017 Andrew Tonner
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
#include "CACFDictionary.h"
#include "CACFArray.h"


@implementation BGMAudioDeviceManager {
    BGMAudioDevice bgmDevice;
    BGMAudioDevice bgmDevice_UISounds;
    BGMAudioDevice outputDevice;
    
    BGMDeviceControlSync deviceControlSync;
    BGMPlayThrough playThrough;
    BGMPlayThrough playThrough_UISounds;
    
    NSRecursiveLock* stateLock;
}

#pragma mark Construction/Destruction

- (id) initWithError:(NSError**)error {
    if ((self = [super init])) {
        stateLock = [NSRecursiveLock new];
        
        bgmDevice = BGMAudioDevice(CFSTR(kBGMDeviceUID));
        bgmDevice_UISounds = BGMAudioDevice(CFSTR(kBGMDeviceUID_UISounds));
        
        if ((bgmDevice == kAudioObjectUnknown) || (bgmDevice_UISounds == kAudioObjectUnknown)) {
            LogError("BGMAudioDeviceManager::initWithError: BGMDevice not found");
            
            if (error) {
                *error = [NSError errorWithDomain:@kBGMAppBundleID code:kBGMErrorCode_BGMDeviceNotFound userInfo:nil];
            }
            
            self = nil;
            return self;
        }

        try {
            [self initOutputDevice];
        } catch (const CAException& e) {
            LogError("BGMAudioDeviceManager::initWithError: failed to init output device (%u)",
                     e.GetError());
            outputDevice.SetObjectID(kAudioObjectUnknown);
        }
        
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

// Throws a CAException if it fails to set the output device.
- (void) initOutputDevice {
    CAHALAudioSystemObject audioSystem;
    // outputDevice = BGMAudioDevice(CFSTR("AppleHDAEngineOutput:1B,0,1,1:0"));
    BGMAudioDevice defaultDevice = audioSystem.GetDefaultAudioDevice(false, false);

    if (defaultDevice.IsBGMDeviceInstance()) {
        // BGMDevice is already the default (it could have been set manually or BGMApp could have
        // failed to change it back the last time it closed), so just pick the device with the
        // lowest latency.
        //
        // TODO: Temporarily disable BGMDevice so we can find out what the previous default was and
        //       use that instead.
        [self setOutputDeviceByLatency];
    } else {
        // TODO: Return the error from setOutputDeviceWithID so it can be returned by initWithError.
        [self setOutputDeviceWithID:defaultDevice revertOnFailure:NO];
    }

    if (outputDevice == kAudioObjectUnknown) {
        LogError("BGMAudioDeviceManager::initOutputDevice: Failed to set output device");
        Throw(CAException(kAudioHardwareUnspecifiedError));
    }

    if (outputDevice.IsBGMDeviceInstance()) {
        LogError("BGMAudioDeviceManager::initOutputDevice: Failed to change output device from "
                 "BGMDevice");
        Throw(CAException(kAudioHardwareUnspecifiedError));
    }
    
    // Log message
    CFStringRef outputDeviceUID = outputDevice.CopyDeviceUID();
    DebugMsg("BGMAudioDeviceManager::initOutputDevice: Set output device to %s",
             CFStringGetCStringPtr(outputDeviceUID, kCFStringEncodingUTF8));
    CFRelease(outputDeviceUID);
}

- (void) setOutputDeviceByLatency {
    CAHALAudioSystemObject audioSystem;
    UInt32 numDevices = audioSystem.GetNumberAudioDevices();

    if (numDevices > 0) {
        BGMAudioDevice minLatencyDevice = kAudioObjectUnknown;
        UInt32 minLatency = UINT32_MAX;

        CAAutoArrayDelete<AudioObjectID> devices(numDevices);
        audioSystem.GetAudioDevices(numDevices, devices);

        for (UInt32 i = 0; i < numDevices; i++) {
            BGMAudioDevice device(devices[i]);

            if (!device.IsBGMDeviceInstance()) {
                BOOL hasOutputChannels = NO;

                BGMLogAndSwallowExceptionsMsg("BGMAudioDeviceManager::setOutputDeviceByLatency",
                                              "GetTotalNumberChannels", ([&] {
                    hasOutputChannels = device.GetTotalNumberChannels(/* inIsInput = */ false) > 0;
                }));

                if (hasOutputChannels) {
                    BGMLogAndSwallowExceptionsMsg("BGMAudioDeviceManager::setOutputDeviceByLatency",
                                                  "GetLatency", ([&] {
                        UInt32 latency = device.GetLatency(false);

                        if (latency < minLatency) {
                            minLatencyDevice = devices[i];
                            minLatency = latency;
                        }
                    }));
                }
            }
        }

        if (minLatencyDevice != kAudioObjectUnknown) {
            // TODO: On error, try a different output device.
            [self setOutputDeviceWithID:minLatencyDevice revertOnFailure:NO];
        }
    }
}

#pragma mark Systemwide Default Device

// Note that there are two different "default" output devices on OS X: "output" and "system output". See
// AudioHardwarePropertyDefaultSystemOutputDevice in AudioHardware.h.

- (NSError* __nullable) setBGMDeviceAsOSDefault {
    DebugMsg("BGMAudioDeviceManager::setBGMDeviceAsOSDefault: Setting the system's default audio "
             "device to BGMDevice");

    CAHALAudioSystemObject audioSystem;

    // Copy the device IDs so we can call the HAL without holding stateLock. See startPlayThroughSync.
    AudioDeviceID bgmDeviceID = kAudioObjectUnknown;
    AudioDeviceID bgmDeviceUISoundsID = kAudioObjectUnknown;
    AudioDeviceID outputDeviceID = kAudioObjectUnknown;
    
    @try {
        [stateLock lock];

        bgmDeviceID = bgmDevice.GetObjectID();
        bgmDeviceUISoundsID = bgmDevice_UISounds.GetObjectID();
        outputDeviceID = outputDevice.GetObjectID();
    } @finally {
        [stateLock unlock];
    }

    if (outputDeviceID == kAudioObjectUnknown) {
        return [NSError errorWithDomain:@kBGMAppBundleID code:kBGMErrorCode_OutputDeviceNotFound userInfo:nil];
    }

    if ((bgmDeviceID == kAudioObjectUnknown) || (bgmDeviceUISoundsID == kAudioObjectUnknown)) {
        return [NSError errorWithDomain:@kBGMAppBundleID code:kBGMErrorCode_BGMDeviceNotFound userInfo:nil];
    }

    try {
        AudioDeviceID currentDefault = audioSystem.GetDefaultAudioDevice(false, true);
        
        try {
            if (currentDefault == outputDeviceID) {
                // The default system device was the same as the default device, so change that as well
                //
                // Use the UI sounds instance of BGMDevice because the default system output device
                // is the device "to use for system related sound". The allows BGMDriver to tell
                // when the audio it receives is UI-related.
                audioSystem.SetDefaultAudioDevice(false, true, bgmDeviceUISoundsID);
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

    // Copy the device IDs so we can call the HAL without holding stateLock. See startPlayThroughSync.
    AudioDeviceID bgmDeviceID = kAudioObjectUnknown;
    AudioDeviceID bgmDeviceUISoundsID = kAudioObjectUnknown;
    AudioDeviceID outputDeviceID = kAudioObjectUnknown;
    
    @try {
        [stateLock lock];

        bgmDeviceID = bgmDevice.GetObjectID();
        bgmDeviceUISoundsID = bgmDevice_UISounds.GetObjectID();
        outputDeviceID = outputDevice.GetObjectID();

        BGMLogAndSwallowExceptions("unsetBGMDeviceAsOSDefault", [&] {
            bgmDeviceIsDefault =
                (audioSystem.GetDefaultAudioDevice(false, false) == bgmDeviceID);
            
            bgmDeviceIsSystemDefault =
                (audioSystem.GetDefaultAudioDevice(false, true) == bgmDeviceUISoundsID);
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
    BOOL isOutputDataSource = NO;

    @try {
        [stateLock lock];
        
        try {
            AudioObjectPropertyScope scope = kAudioDevicePropertyScopeOutput;
            UInt32 channel = 0;
            
            isOutputDataSource =
                    outputDevice.HasDataSourceControl(scope, channel) &&
                            (dataSourceID == outputDevice.GetCurrentDataSourceID(scope, channel));
        } catch (CAException e) {
            BGMLogException(e);
        }
    } @finally {
        [stateLock unlock];
    }

    return isOutputDataSource;
}

#pragma mark App Volumes

- (void) sendAppVolumeToBGMDevice:(SInt32)newVolume
                     appProcessID:(pid_t)appProcessID
                      appBundleID:(NSString*)appBundleID {
    [self sendAppVolumePanToBGMDevice:newVolume
                        volumeTypeKey:CFSTR(kBGMAppVolumesKey_RelativeVolume)
                         appProcessID:appProcessID
                          appBundleID:appBundleID];
}

- (void) sendAppPanPositionToBGMDevice:(SInt32)newPanPosition
                          appProcessID:(pid_t)appProcessID
                           appBundleID:(NSString*)appBundleID {
    [self sendAppVolumePanToBGMDevice:newPanPosition
                        volumeTypeKey:CFSTR(kBGMAppVolumesKey_PanPosition)
                         appProcessID:appProcessID
                          appBundleID:appBundleID];
}

- (void) sendAppVolumePanToBGMDevice:(SInt32)newValue
                       volumeTypeKey:(CFStringRef)typeKey
                        appProcessID:(pid_t)appProcessID
                         appBundleID:(NSString*)appBundleID {
    CACFArray appVolumeChanges(true);

    auto addVolumeChange = [&] (pid_t pid, NSString* bundleID) {
        CACFDictionary appVolumeChange(true);

        appVolumeChange.AddSInt32(CFSTR(kBGMAppVolumesKey_ProcessID), pid);
        appVolumeChange.AddString(CFSTR(kBGMAppVolumesKey_BundleID),
                                  (__bridge CFStringRef)bundleID);
        appVolumeChange.AddSInt32(typeKey, newValue);

        appVolumeChanges.AppendDictionary(appVolumeChange.GetDict());
    };

    addVolumeChange(appProcessID, appBundleID);

    // Add the same change for each process the app is responsible for.
    for (NSString* responsibleBundleID :
            [BGMAudioDeviceManager responsibleBundleIDsOf:appBundleID]) {
        // Send -1 as the PID so this volume will only ever be matched by bundle ID.
        addVolumeChange(-1, responsibleBundleID);
    }

    CFPropertyListRef changesPList = appVolumeChanges.AsPropertyList();

    // Send the change to BGMDevice.
    bgmDevice.SetPropertyData_CFType(kBGMAppVolumesAddress, changesPList);

    // Also send it to the instance of BGMDevice that handles UI sounds.
    bgmDevice_UISounds.SetPropertyData_CFType(kBGMAppVolumesAddress, changesPList);
}

// This is a temporary solution that lets us control the volumes of some multiprocess apps, i.e.
// apps that play their audio from a process with a different bundle ID.
//
// We can't just check the child processes of the apps' main processes because they're usually
// created with launchd rather than being actual child processes. There's a private API to get the
// processes that an app is "responsible for", so we'll try to use it in the proper fix and only use
// this list if the API doesn't work.
+ (NSArray<NSString*>*) responsibleBundleIDsOf:(NSString*)parentBundleID {
    NSDictionary<NSString*, NSArray<NSString*>*>* bundleIDMap = @{
            // Finder
            @"com.apple.finder": @[@"com.apple.quicklook.ui.helper"],
            // Safari
            @"com.apple.Safari": @[@"com.apple.WebKit.WebContent"],
            // Firefox
            @"org.mozilla.firefox": @[@"org.mozilla.plugincontainer"],
            // Firefox Nightly
            @"org.mozilla.nightly": @[@"org.mozilla.plugincontainer"],
            // VMWare Fusion
            @"com.vmware.fusion": @[@"com.vmware.vmware-vmx"],
            // Parallels
            @"com.parallels.desktop.console": @[@"com.parallels.vm"],
            // MPlayer OSX Extended
            @"hu.mplayerhq.mplayerosx.extended": @[@"ch.sttz.mplayerosx.extended.binaries.officialsvn"]
    };

    // Parallels' VM "dock helper" apps have bundle IDs like
    // com.parallels.winapp.87f6bfc236d64d70a81c47f6243add4c.f5a25fdede514f7aa0a475a1873d3287.fs
    if ([parentBundleID hasPrefix:@"com.parallels.winapp."]) {
        return @[@"com.parallels.vm"];
    }

    return bundleIDMap[parentBundleID];
}

#pragma mark Output Device

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
    DebugMsg("BGMAudioDeviceManager::setOutputDeviceWithIDImpl: Setting output device. newDeviceID=%u",
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
                playThrough_UISounds.Deactivate();

                deviceControlSync.SetDevices(bgmDevice, newOutputDevice);
                deviceControlSync.Activate();

                // Stream audio from BGMDevice to the new output device. This blocks while the old device
                // stops IO.
                playThrough.SetDevices(&bgmDevice, &newOutputDevice);
                playThrough.Activate();

                // TODO: Support setting different devices as the default output device and the
                //       default system output device, the way OS X does.
                playThrough_UISounds.SetDevices(&bgmDevice_UISounds, &newOutputDevice);
                playThrough_UISounds.Activate();

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
                playThrough_UISounds.Start();
                // But stop playthrough if audio isn't playing, since it uses CPU.
                playThrough.StopIfIdle();
                playThrough_UISounds.StopIfIdle();
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

- (OSStatus) startPlayThroughSync:(BOOL)forUISoundsDevice {
    // We can only try for stateLock because setOutputDeviceWithID might have already taken it, then made a
    // HAL request to BGMDevice and be waiting for the response. Some of the requests setOutputDeviceWithID
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
            BGMPlayThrough& pt = (forUISoundsDevice ? playThrough_UISounds : playThrough);

            // Playthrough might not have been notified that BGMDevice is starting yet, so make sure
            // playthrough is starting. This way we won't drop any frames while waiting for the HAL to send
            // that notification. We can't be completely sure this is safe from deadlocking, though, since
            // CoreAudio is closed-source.
            //
            // TODO: Test this on older OS X versions. Differences in the CoreAudio implementations could
            //       cause deadlocks.
            BGMLogAndSwallowExceptionsMsg("BGMAudioDeviceManager::startPlayThroughSync",
                                          "Starting playthrough", [&] {
                pt.Start();
            });

            err = pt.WaitForOutputDeviceToStart();
            BGMAssert(err != BGMPlayThrough::kDeviceNotStarting, "Playthrough didn't start");
        } else {
            LogWarning("BGMAudioDeviceManager::startPlayThroughSync: Didn't get state lock. Returning "
                       "early with kBGMErrorCode_ReturningEarly.");
            err = kBGMErrorCode_ReturningEarly;

            // TODO: The QOS_CLASS_USER_INTERACTIVE constant isn't available on OS X 10.9.
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
                @try {
                    [stateLock lock];

                    BGMPlayThrough& pt = (forUISoundsDevice ? playThrough_UISounds : playThrough);
                    
                    BGMLogAndSwallowExceptionsMsg("BGMAudioDeviceManager::startPlayThroughSync",
                                                  "Starting playthrough (dispatched)", [&] {
                        pt.Start();
                    });

                    BGMLogAndSwallowExceptions("BGMAudioDeviceManager::startPlayThroughSync", [&] {
                        pt.StopIfIdle();
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


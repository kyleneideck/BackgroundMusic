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
//  Copyright © 2016-2018 Kyle Neideck
//

// Self Include
#import "BGMAudioDeviceManager.h"

// Local Includes
#import "BGM_Types.h"
#import "BGM_Utils.h"
#import "BGMAudioDevice.h"
#import "BGMDeviceControlSync.h"
#import "BGMOutputDeviceMenuSection.h"
#import "BGMOutputVolumeMenuItem.h"
#import "BGMPlayThrough.h"
#import "BGMXPCProtocols.h"

// PublicUtility Includes
#import "CAAtomic.h"
#import "CAAutoDisposer.h"
#import "CAHALAudioSystemObject.h"


#pragma clang assume_nonnull begin

@implementation BGMAudioDeviceManager {
    // This ivar is a pointer so that BGMBackgroundMusicDevice's constructor doesn't get called
    // during [BGMAudioDeviceManager alloc] when the ivars are initialised. It queries the HAL for
    // BGMDevice's AudioObject ID, which might throw a CAException, most likely because BGMDevice
    // isn't installed.
    //
    // That would be the only way for [BGMAudioDeviceManager alloc] to throw a CAException, so we
    // could wrap that call in a try/catch block instead, but it would make the code a bit
    // confusing.
    BGMBackgroundMusicDevice* bgmDevice;
    BGMAudioDevice outputDevice;
    
    BGMDeviceControlSync deviceControlSync;
    BGMPlayThrough playThrough;
    BGMPlayThrough playThrough_UISounds;

    // A connection to BGMXPCHelper so we can send it the ID of the output device.
    NSXPCConnection* __nullable bgmXPCHelperConnection;

    BGMOutputVolumeMenuItem* __nullable outputVolumeMenuItem;
    BGMOutputDeviceMenuSection* __nullable outputDeviceMenuSection;

    NSRecursiveLock* stateLock;
}

#pragma mark Construction/Destruction

- (instancetype) init {
    if ((self = [super init])) {
        stateLock = [NSRecursiveLock new];
        bgmXPCHelperConnection = nil;
        outputVolumeMenuItem = nil;
        outputDeviceMenuSection = nil;
        outputDevice = kAudioObjectUnknown;

        try {
            bgmDevice = new BGMBackgroundMusicDevice;
        } catch (const CAException& e) {
            LogError("BGMAudioDeviceManager::init: BGMDevice not found. (%d)", e.GetError());
            self = nil;
            return self;
        }
    }
    
    return self;
}

- (void) dealloc {
    @try {
        [stateLock lock];

        if (bgmDevice) {
            delete bgmDevice;
            bgmDevice = nullptr;
        }
    } @finally {
        [stateLock unlock];
    }
}

- (void) setOutputVolumeMenuItem:(BGMOutputVolumeMenuItem*)item {
    outputVolumeMenuItem = item;
}

- (void) setOutputDeviceMenuSection:(BGMOutputDeviceMenuSection*)menuSection {
    outputDeviceMenuSection = menuSection;
}

#pragma mark Systemwide Default Device

// Note that there are two different "default" output devices on OS X: "output" and "system output". See
// kAudioHardwarePropertyDefaultSystemOutputDevice in AudioHardware.h.

- (NSError* __nullable) setBGMDeviceAsOSDefault {
    try {
        // Intentionally avoid taking stateLock before making calls to the HAL. See
        // startPlayThroughSync.
        CAMemoryBarrier();
        bgmDevice->SetAsOSDefault();
    } catch (const CAException& e) {
        BGMLogExceptionIn("BGMAudioDeviceManager::setBGMDeviceAsOSDefault", e);
        return [NSError errorWithDomain:@kBGMAppBundleID code:e.GetError() userInfo:nil];
    }

    return nil;
}

- (NSError* __nullable) unsetBGMDeviceAsOSDefault {
    // Copy the devices so we can call the HAL without holding stateLock. See startPlayThroughSync.
    BGMBackgroundMusicDevice* bgmDeviceCopy;
    AudioDeviceID outputDeviceID;

    @try {
        [stateLock lock];
        bgmDeviceCopy = bgmDevice;
        outputDeviceID = outputDevice.GetObjectID();
    } @finally {
        [stateLock unlock];
    }

    if (outputDeviceID == kAudioObjectUnknown) {
        return [NSError errorWithDomain:@kBGMAppBundleID
                                   code:kBGMErrorCode_OutputDeviceNotFound
                               userInfo:nil];
    }

    try {
        bgmDeviceCopy->UnsetAsOSDefault(outputDeviceID);
    } catch (const CAException& e) {
        BGMLogExceptionIn("BGMAudioDeviceManager::unsetBGMDeviceAsOSDefault", e);
        return [NSError errorWithDomain:@kBGMAppBundleID code:e.GetError() userInfo:nil];
    }
    
    return nil;
}

#pragma mark Accessors

- (BGMBackgroundMusicDevice) bgmDevice {
    return *bgmDevice;
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
        } catch (const CAException& e) {
            BGMLogException(e);
        }
    } @finally {
        [stateLock unlock];
    }

    return isOutputDataSource;
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
    
    @try {
        [stateLock lock];

        AudioDeviceID currentDeviceID = outputDevice.GetObjectID();  // (Doesn't throw.)

        try {
            [self setOutputDeviceWithIDImpl:newDeviceID
                               dataSourceID:dataSourceID
                            currentDeviceID:currentDeviceID];
        } catch (const CAException& e) {
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

        // Tell other classes and BGMXPCHelper that we changed the output device.
        [self propagateOutputDeviceChange];
    } @finally {
        [stateLock unlock];
    }

    return nil;
}

// Throws CAException.
- (void) setOutputDeviceWithIDImpl:(AudioObjectID)newDeviceID
                      dataSourceID:(UInt32* __nullable)dataSourceID
                   currentDeviceID:(AudioObjectID)currentDeviceID {
    if (newDeviceID != currentDeviceID) {
        BGMAudioDevice newOutputDevice(newDeviceID);
        [self setOutputDeviceForPlaythroughAndControlSync:newOutputDevice];
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

    CFStringRef outputDeviceUID = outputDevice.CopyDeviceUID();
    DebugMsg("BGMAudioDeviceManager::setOutputDeviceWithIDImpl: Set output device to %s (%d)",
             CFStringGetCStringPtr(outputDeviceUID, kCFStringEncodingUTF8),
             outputDevice.GetObjectID());
    CFRelease(outputDeviceUID);
}

// Changes the output device that playthrough plays audio to and that BGMDevice's controls are
// kept in sync with. Throws CAException.
- (void) setOutputDeviceForPlaythroughAndControlSync:(const BGMAudioDevice&)newOutputDevice {
    // Deactivate playthrough rather than stopping it so it can't be started by HAL notifications
    // while we're updating deviceControlSync.
    playThrough.Deactivate();
    playThrough_UISounds.Deactivate();

    deviceControlSync.SetDevices(*bgmDevice, newOutputDevice);
    deviceControlSync.Activate();

    // Stream audio from BGMDevice to the new output device. This blocks while the old device stops
    // IO.
    playThrough.SetDevices(bgmDevice, &newOutputDevice);
    playThrough.Activate();

    // TODO: Support setting different devices as the default output device and the default system
    //       output device the way OS X does?
    BGMAudioDevice uiSoundsDevice = bgmDevice->GetUISoundsBGMDeviceInstance();
    playThrough_UISounds.SetDevices(&uiSoundsDevice, &newOutputDevice);
    playThrough_UISounds.Activate();
}

- (void) setDataSource:(UInt32)dataSourceID device:(BGMAudioDevice&)device {
    BGMLogAndSwallowExceptions("BGMAudioDeviceManager::setDataSource", ([&] {
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeOutput;
        UInt32 channel = 0;

        if (device.DataSourceControlIsSettable(scope, channel)) {
            DebugMsg("BGMAudioDeviceManager::setOutputDeviceWithID: Setting dataSourceID=%u",
                     dataSourceID);
            
            device.SetCurrentDataSourceByID(scope, channel, dataSourceID);
        }
    }));
}

- (void) propagateOutputDeviceChange {
    // Tell BGMXPCHelper that the output device has changed.
    [self sendOutputDeviceToBGMXPCHelper];

    // Update the menu item for the volume of the output device.
    [outputVolumeMenuItem outputDeviceDidChange];
    [outputDeviceMenuSection outputDeviceDidChange];
}

- (NSError*) failedToSetOutputDevice:(AudioDeviceID)deviceID
                           errorCode:(OSStatus)errorCode
                            revertTo:(AudioDeviceID*)revertTo {
    // Using LogWarning from PublicUtility instead of NSLog here crashes from a bad access. Not sure why.
    // TODO: Possibly caused by a bug in CADebugMacros.cpp. See commit ab9d4cd.
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

            dispatch_async(BGMGetDispatchQueue_PriorityUserInteractive(), ^{
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

#pragma mark BGMXPCHelper Communication

- (void) setBGMXPCHelperConnection:(NSXPCConnection* __nullable)connection {
    bgmXPCHelperConnection = connection;

    // Tell BGMXPCHelper which device is the output device, since it might not be up-to-date.
    [self sendOutputDeviceToBGMXPCHelper];
}

- (void) sendOutputDeviceToBGMXPCHelper {
    NSXPCConnection* __nullable connection = bgmXPCHelperConnection;

    if (connection)
    {
        id<BGMXPCHelperXPCProtocol> helperProxy =
                [connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
                    // We could wait a bit and try again, but it isn't that important.
                    NSLog(@"BGMAudioDeviceManager::sendOutputDeviceToBGMXPCHelper: Connection"
                           "error: %@", error);
                }];

        [helperProxy setOutputDeviceToMakeDefaultOnAbnormalTermination:outputDevice.GetObjectID()];
    }
}

@end

#pragma clang assume_nonnull end



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
//  Copyright © 2016-2018, 2026 Kyle Neideck
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
#import "CAPropertyAddress.h"


#pragma clang assume_nonnull begin

static OSStatus BGMDeviceListenerProc(AudioObjectID inObjectID,
                                      UInt32 inNumberAddresses,
                                      const AudioObjectPropertyAddress* inAddresses,
                                      void* __nullable inClientData);

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

    BOOL bgmDeviceListenersRegistered;

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
        bgmDeviceListenersRegistered = NO;

        try {
            bgmDevice = new BGMBackgroundMusicDevice;
        } catch (const CAException& e) {
            LogError("BGMAudioDeviceManager::init: BGMDevice not found. (%d)", e.GetError());
            self = nil;
            return self;
        }

        // Start listening for IO state changes on BGMDevice so we can start and stop playthrough.
        // The handlers do nothing until an output device has been set.
        [self registerBGMDeviceListeners];
    }
    
    return self;
}

// BGMAudioDeviceManager is a singleton and lives for the whole process. We still remove the
// listeners and free bgmDevice here, so deallocating in a test (or if init fails) is safe.
//
// The listener proc's client data is an unretained pointer to the BGMAudioDeviceManager instance,
// so it's not safe to deallocate it while the listeners are registered.
// (The docs for AudioObjectRemovePropertyListener don't say whether it waits for in-flight
// callbacks to finish, so even removing the listeners here wouldn't be fully safe.)
- (void) dealloc {
    BGMAssert(NSClassFromString(@"XCTestCase") != nil,  // Returns nil unless running in a test
              "BGMAudioDeviceManager is not safe to destroy (except in unit tests).");

    @try {
        [stateLock lock];

        [self removeBGMDeviceListeners];

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

- (void) prepareForTermination {
    DebugMsg("BGMAudioDeviceManager::prepareForTermination: Deactivating control sync and "
             "playthrough before termination");

    @try {
        [stateLock lock];

        // Deactivate control sync BEFORE changing the default device. This removes the volume/mute
        // listeners so that the default device change doesn't trigger any stale volume sync that
        // could leave the output device at the wrong volume.
        // See https://github.com/kyleneideck/BackgroundMusic/issues/841
        BGMLogAndSwallowExceptions("BGMAudioDeviceManager::prepareForTermination", [&] {
            deviceControlSync.Deactivate();
        });

        // Stop playthrough so we don't try to pass audio during shutdown.
        BGMLogAndSwallowExceptions("BGMAudioDeviceManager::prepareForTermination", [&] {
            playThrough.Deactivate();
        });
        BGMLogAndSwallowExceptions("BGMAudioDeviceManager::prepareForTermination", [&] {
            playThrough_UISounds.Deactivate();
        });
    } @finally {
        [stateLock unlock];
    }
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

        // TODO: It's not worth keeping the tryLock path just for macOS older than 11. We should
        //       delete it and rename this method to startPlayThrough. If CoreAudio ever makes it
        //       possible to block StartIO until another device starts again, we can bring it back.
        BOOL isBigSur = NO;
        if (@available(macOS 11.0, *)) {
            isBigSur = YES;
        }

        // Always start playthrough asynchronously. Temp workaround for deadlock on Big Sur.
        if (!isBigSur && gotLock) {
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

#pragma mark BGMDevice Listeners

// TODO: This class is too big and has too many responsibilities. We should extract the playthrough
//       listeners (or a playthrough controller) into a new class.

// TODO: Listen for changes to the sample rate of the output device and update BGMDevice to match.

// Register property listeners on BGMDevice and the UI sounds BGMDevice instance.
//
// Listeners are registered here, rather than by BGMPlayThrough, so the listener proc's client data
// pointer always remains valid. BGMPlayThrough instances currently only get destroyed when BGMApp
// is closing (I don't remember why, but we have exit-time C++ destructors enabled), so it's
// unlikely to cause a crash, but this way is more correct and easier to understand.
- (void) registerBGMDeviceListeners {
    if (bgmDeviceListenersRegistered) {
        return;
    }

    void* bridgeSelf = (__bridge void*)self;

    auto addListeners = [&](AudioObjectID deviceID) {
        BGMLogAndSwallowExceptions("BGMAudioDeviceManager::registerBGMDeviceListeners", [&] {
            BGMAudioDevice device(deviceID);
            device.AddPropertyListener(
                    CAPropertyAddress(kAudioDevicePropertyDeviceIsRunningSomewhere),
                    &BGMDeviceListenerProc, bridgeSelf);
            device.AddPropertyListener(
                    CAPropertyAddress(kAudioDeviceProcessorOverload),
                    &BGMDeviceListenerProc, bridgeSelf);
            device.AddPropertyListener(
                    kBGMRunningSomewhereOtherThanBGMAppAddress,
                    &BGMDeviceListenerProc, bridgeSelf);
        });
    };

    addListeners(bgmDevice->GetObjectID());
    addListeners(bgmDevice->GetUISoundsBGMDeviceInstance().GetObjectID());

    bgmDeviceListenersRegistered = YES;
}

- (void) removeBGMDeviceListeners {
    if (!bgmDeviceListenersRegistered) {
        return;
    }

    void* bridgeSelf = (__bridge void*)self;

    auto removeListeners = [&](AudioObjectID deviceID) {
        // There's not much we can do if these calls throw. The docs for
        // AudioObjectRemovePropertyListener just say that means it failed.
        BGMLogAndSwallowExceptions("BGMAudioDeviceManager::removeBGMDeviceListeners", [&] {
            BGMAudioDevice device(deviceID);
            device.RemovePropertyListener(
                    CAPropertyAddress(kAudioDevicePropertyDeviceIsRunningSomewhere),
                    &BGMDeviceListenerProc, bridgeSelf);
            device.RemovePropertyListener(
                    CAPropertyAddress(kAudioDeviceProcessorOverload),
                    &BGMDeviceListenerProc, bridgeSelf);
            device.RemovePropertyListener(
                    kBGMRunningSomewhereOtherThanBGMAppAddress,
                    &BGMDeviceListenerProc, bridgeSelf);
        });
    };

    removeListeners(bgmDevice->GetObjectID());
    removeListeners(bgmDevice->GetUISoundsBGMDeviceInstance().GetObjectID());

    bgmDeviceListenersRegistered = NO;
}

// Handles property change notifications from BGMDevice (and the UI sounds BGMDevice instance).
- (void) handleBGMDeviceNotification:(AudioObjectID)inDeviceID
                        numAddresses:(UInt32)inNumberAddresses
                           addresses:(const AudioObjectPropertyAddress*)inAddresses {
    BGMAssert(inDeviceID == bgmDevice->GetObjectID() ||
              inDeviceID == bgmDevice->GetUISoundsBGMDeviceInstance().GetObjectID(),
              "BGMAudioDeviceManager::handleBGMDeviceNotification: "
              "Notified about an unexpected audio object (%u)",
              inDeviceID);

    for(UInt32 i = 0; i < inNumberAddresses; i++)
    {
        switch(inAddresses[i].mSelector)
        {
            case kAudioDeviceProcessorOverload:
                // These warnings are common when you use the UI if you're running a debug build or
                // have "Debug executable" checked. You shouldn't be seeing them otherwise.
                DebugMsg("BGMAudioDeviceManager: WARNING! Got kAudioDeviceProcessorOverload "
                         "notification for device %u",
                         inDeviceID);
                LogWarning("Background Music: CPU overload reported");
                break;

            // These cases are dispatched to avoid causing deadlocks by triggering one of the
            // following notifications in the process of handling one. Deadlocks could happen if
            // these were handled synchronously:
            //     - a handler takes stateLock and requests some data from the HAL,
            //     - the request makes the HAL send notifications, which it sends on a different
            //       thread/queue,
            //     - the second handler callback waits for stateLock, and
            //     - the HAL waits for the second handler callback to return before returning the
            //       data requested by the first one.

            case kAudioDevicePropertyDeviceIsRunningSomewhere:
                [self handleDeviceIsRunningSomewhere:inDeviceID];
                break;

            case kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp:
                [self handleDeviceIsRunningSomewhereOtherThanBGMApp:inDeviceID];
                break;

            default:
                // We might get properties we didn't ask for, so we just ignore them.
                break;
        }
    }
}

// Returns the BGMPlayThrough for the given BGMDevice instance.
- (BGMPlayThrough&) playThroughForDeviceID:(AudioObjectID)inDeviceID {
    return (inDeviceID == bgmDevice->GetUISoundsBGMDeviceInstance().GetObjectID()) ?
            playThrough_UISounds : playThrough;
}

// Start playthrough when a client starts IO on BGMDevice.
//
// Note that kAudioDevicePropertyDeviceIsRunningSomewhere fires as soon as any client starts
// doing IO on BGMDevice. kAudioDevicePropertyDeviceIsRunning wouldn't work for this because
// it only fires when BGMApp's own IO procs start or stop.
//
// Either this notification or BGMXPCListener::startPlayThroughSyncWithReply will start
// playthrough, whichever we get first, in case one is faster than the other. It also allows
// playthrough to start even if BGMXPCHelper isn't working for some reason.
- (void) handleDeviceIsRunningSomewhere:(AudioObjectID)inDeviceID {
    DebugMsg("BGMAudioDeviceManager::handleDeviceIsRunningSomewhere: Got notification for "
             "device %u",
             inDeviceID);

    // This is dispatched because it can block and BGMXPCListener::startPlayThroughSyncWithReply
    // might get called on the same thread just before this and time out waiting for this to run.
    dispatch_async(BGMGetDispatchQueue_PriorityUserInteractive(), ^{
        @try {
            [stateLock lock];

            // Don't try to start playthrough before the output device has been set.
            if (outputDevice.GetObjectID() == kAudioObjectUnknown) {
                return;
            }

            BGMPlayThrough& pt = [self playThroughForDeviceID:inDeviceID];

            // Default to true to try to start playthrough anyway if we fail to get this property
            // from BGMDevice.
            bool isRunningSomewhere = true;

            BGMLogAndSwallowExceptions("handleDeviceIsRunningSomewhere", [&]() {
                isRunningSomewhere =
                        (BGMAudioDevice(inDeviceID).GetPropertyData_UInt32(
                                CAPropertyAddress(kAudioDevicePropertyDeviceIsRunningSomewhere)) != 0);
            });

            DebugMsg("BGMAudioDeviceManager::handleDeviceIsRunningSomewhere: "
                     "Device %u is %srunning somewhere",
                     inDeviceID,
                     isRunningSomewhere ? "" : "not ");

            if(isRunningSomewhere)
            {
                // TODO: Handle expected exceptions (mostly CAExceptions from PublicUtility
                //       classes) in Start. For any that can't be handled sensibly in Start,
                //       catch them here and retry a few times (with a very short delay) before
                //       handling them by showing an unobtrusive error message or something.
                //       Then try a different device or just set the system device back to the
                //       real device.
                BGMLogAndSwallowExceptions("handleDeviceIsRunningSomewhere", [&]() {
                    pt.Start();
                });
            }
        } @finally {
            [stateLock unlock];
        }
    });
}

// Stop playthrough when BGMApp is the only client left doing IO.
//
// We don't need a delay before stopping because BGMDriver only sends this notification after
// non-BGMApp clients have been inactive for 2 seconds (see ClientsOtherThanBGMAppRunningIO in
// BGM_Clients). That's enough time for BGMDriver to update
// kAudioDeviceCustomPropertyDeviceAudibleState, which needs
// kDeviceAudibleStateMinChangedFramesForUpdate frames. That's about 0.1 to 0.5 s, depending on
// sample rate.
//
// If we change BGMDevice so that BGMApp's IO stops before the audible state can update, e.g. by
// splitting out the input stream to a separate device or replacing it with shared memory, we'll
// need to make BGMDriver update the audible state when IO stops.
// TODO: We should probably do that either way if it's simple enough. It's less error prone.
- (void) handleDeviceIsRunningSomewhereOtherThanBGMApp:(AudioObjectID)inDeviceID {
    DebugMsg("BGMAudioDeviceManager::handleDeviceIsRunningSomewhereOtherThanBGMApp: "
             "Got notification for device %u",
             inDeviceID);

    // Dispatched so we can take stateLock without risking deadlock.
    // Stopping playthrough doesn't need to be done quickly, so default priority is fine. And it
    // doesn't need to be done on the same thread as starting because we only stop if idle.
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @try {
            [stateLock lock];

            if (outputDevice.GetObjectID() == kAudioObjectUnknown) {
                return;
            }

            BGMPlayThrough& pt = [self playThroughForDeviceID:inDeviceID];

            // TODO: Handle expected exceptions (mostly CAExceptions from PublicUtility
            //       classes) in StopIfIdle.
            BGMLogUnexpectedExceptions("handleDeviceIsRunningSomewhereOtherThanBGMApp", [&]() {
                pt.StopIfIdle();
            });
        } @finally {
            [stateLock unlock];
        }
    });
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

// static
OSStatus    BGMDeviceListenerProc(AudioObjectID inObjectID,
                                  UInt32 inNumberAddresses,
                                  const AudioObjectPropertyAddress* inAddresses,
                                  void* __nullable inClientData)
{
    // TODO: For kAudioDeviceProcessorOverload, the HAL can call this synchronously on a real-time
    //       (IO) thread, and Objective-C message sends aren't real-time safe. (The other properties
    //       we listen for aren't delivered on the IO thread.)
    BGMAudioDeviceManager* manager = (__bridge BGMAudioDeviceManager*)inClientData;
    [manager handleBGMDeviceNotification:inObjectID
                            numAddresses:inNumberAddresses
                               addresses:inAddresses];
    // From AudioHardware.h: "The return value is currently unused and should always be 0."
    return 0;
}

#pragma clang assume_nonnull end



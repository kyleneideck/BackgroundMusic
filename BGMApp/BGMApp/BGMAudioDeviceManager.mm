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
#import "BGMUserDefaults.h"
#import "BGMXPCProtocols.h"

// PublicUtility Includes
#import "CAAtomic.h"
#import "CAAutoDisposer.h"
#import "CAHALAudioSystemObject.h"
#import "CAPropertyAddress.h"

// STL Includes
#include <cmath>  // For fabs
#include <vector>


#pragma clang assume_nonnull begin

static OSStatus BGMDeviceListenerProc(AudioObjectID inObjectID,
                                      UInt32 inNumberAddresses,
                                      const AudioObjectPropertyAddress* inAddresses,
                                      void* __nullable inClientData);

// The gain (0.0 to 1.0) to use for an output device that has no saved settings, e.g. when the
// user enables a device for the first time or on the app's first launch.
static const float kDefaultOutputDeviceGain = 0.5f;

// The state of an output device.
struct BGMOutputDeviceState {
    AudioObjectID deviceID;
    BOOL          enabled;
    float         gain;   // 0.0 to 1.0
    bool          muted;  // True to silence the device (its saved gain is preserved).
};

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
    // The primary output device (the first enabled one). Used for unsetting the OS default device
    // and telling BGMXPCHelper which device to restore if BGMApp terminates abnormally.
    BGMAudioDevice outputDevice;
    
    // The state of each output device, in order. Guarded by stateLock.
    std::vector<BGMOutputDeviceState> outputDeviceStates;
    
    BGMDeviceControlSync deviceControlSync;
    BGMPlayThrough playThrough;
    BGMPlayThrough playThrough_UISounds;

    BOOL bgmDeviceListenersRegistered;

    // A connection to BGMXPCHelper so we can send it the ID of the output device.
    NSXPCConnection* __nullable bgmXPCHelperConnection;

    BGMOutputVolumeMenuItem* __nullable outputVolumeMenuItem;
    BGMOutputDeviceMenuSection* __nullable outputDeviceMenuSection;

    // Used to persist the per-output-device settings (which devices are enabled and their gains).
    BGMUserDefaults* __nullable userDefaults;

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
        return [self isDeviceEnabled:deviceID];
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

- (void) setUserDefaults:(BGMUserDefaults*)inUserDefaults {
    @try {
        [stateLock lock];
        userDefaults = inUserDefaults;
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
    DebugMsg("BGMAudioDeviceManager::setOutputDeviceWithIDImpl: Setting output device. newDeviceID=%u",
             newDeviceID);
    
    @try {
        [stateLock lock];

        AudioDeviceID currentDeviceID = outputDevice.GetObjectID();  // (Doesn't throw.)

        try {
            // Make newDeviceID the only enabled output device.
            [self setEnabledDeviceIDsOnly:newDeviceID];

            // Set the output device to use the new data source.
            if (dataSourceID) {
                // TODO: If this fails, ideally we'd still start playthrough and return an error, but not
                //       revert the device. It would probably be a bit awkward, though.
                [self setDataSource:*dataSourceID device:outputDevice];
            }
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

        [self persistOutputDeviceSettings];

        // Tell other classes and BGMXPCHelper that we changed the output device.
        [self propagateOutputDeviceChange];
    } @finally {
        [stateLock unlock];
    }

    return nil;
}

// Sets the enabled output devices to exactly one, inDeviceID. Throws CAException.
// Must be called with stateLock held.
- (void) setEnabledDeviceIDsOnly:(AudioObjectID)inDeviceID {
    for (auto& state : outputDeviceStates) {
        state.enabled = (state.deviceID == inDeviceID);
    }

    // Make sure there's a state entry for the new device.
    if (![self findStateForDeviceID:inDeviceID]) {
        outputDeviceStates.push_back({ inDeviceID, YES, kDefaultOutputDeviceGain, false });
    }

    [self rebuildPlaythrough];
}

// Reconfigures the playthroughs to play audio to the enabled output devices, at their gains, with
// the master volume applied in real time. Throws CAException.
// Must be called with stateLock held.
- (void) rebuildPlaythrough {
    std::vector<BGMAudioDevice> enabledDevices;
    for (const auto& state : outputDeviceStates) {
        if (state.enabled) {
            enabledDevices.push_back(BGMAudioDevice(state.deviceID));
        }
    }

    if (enabledDevices.empty()) {
        // No enabled output devices. Keep the current configuration. (The caller should restore a
        // preferred device if it needs one.)
        LogWarning("BGMAudioDeviceManager::rebuildPlaythrough: No enabled output devices. Keeping "
                   "the current configuration.");
        return;
    }

    // Update the primary output device.
    outputDevice = enabledDevices.front();

    // Deactivate playthrough rather than stopping it so it can't be started by HAL notifications
    // while we're updating deviceControlSync.
    playThrough.Deactivate();
    playThrough_UISounds.Deactivate();

    // Normalise the output devices' volume controls so the playthrough's software volume is the
    // only attenuation.
    deviceControlSync.SetDevices(bgmDevice->GetObjectID(), enabledDevices);
    deviceControlSync.Activate();

    // Stream audio from BGMDevice to the output devices. This blocks while the old devices stop
    // IO.
    playThrough.SetDevices(BGMAudioDevice(bgmDevice->GetObjectID()), enabledDevices);
    playThrough.Activate();

    // TODO: Support setting different devices as the default output device and the default system
    //       output device the way OS X does?
    BGMAudioDevice uiSoundsDevice = bgmDevice->GetUISoundsBGMDeviceInstance();
    playThrough_UISounds.SetDevices(uiSoundsDevice, enabledDevices);
    playThrough_UISounds.Activate();

    // (Re)apply the per-device gains and the master volume. Muted devices play silence.
    for (const auto& state : outputDeviceStates) {
        Float32 effectiveGain = (state.muted ? 0.0f : (Float32)state.gain);
        playThrough.SetOutputDeviceGain(state.deviceID, effectiveGain);
        playThrough_UISounds.SetOutputDeviceGain(state.deviceID, effectiveGain);
    }
    [self updatePlayThroughMasterVolumes];

    // Start playthrough, since audio might be playing.
    playThrough.Start();
    playThrough_UISounds.Start();
    // But stop playthrough if audio isn't playing, since it uses CPU.
    playThrough.StopIfIdle();
    playThrough_UISounds.StopIfIdle();

    CFStringRef outputDeviceUID = outputDevice.CopyDeviceUID();
    DebugMsg("BGMAudioDeviceManager::rebuildPlaythrough: Set primary output device to %s (%d)",
             CFStringGetCStringPtr(outputDeviceUID, kCFStringEncodingUTF8),
             outputDevice.GetObjectID());
    CFRelease(outputDeviceUID);
}

#pragma mark Per-Output-Device Control

// Must be called with stateLock held.
- (BGMOutputDeviceState* __nullable) findStateForDeviceID:(AudioObjectID)deviceID {
    for (auto& state : outputDeviceStates) {
        if (state.deviceID == deviceID) {
            return &state;
        }
    }

    return nullptr;
}

- (BOOL) setDevice:(AudioObjectID)deviceID enabled:(BOOL)enabled {
    @try {
        [stateLock lock];

        BGMOutputDeviceState* state = [self findStateForDeviceID:deviceID];
        if (!state) {
            outputDeviceStates.push_back({ deviceID, enabled, kDefaultOutputDeviceGain, false });
            state = &outputDeviceStates.back();
        }

        if (state->enabled == enabled) {
            return YES;  // No change.
        }

        // Don't allow disabling the last enabled device, since BGMApp needs at least one output
        // device to play audio to (and to restore the OS's default device on exit).
        if (!enabled) {
            NSUInteger numEnabled = 0;
            for (const auto& s : outputDeviceStates) {
                if (s.enabled) {
                    numEnabled++;
                }
            }

            if (numEnabled <= 1) {
                LogWarning("BGMAudioDeviceManager::setDevice:enabled: Refusing to disable the "
                           "last enabled output device (%u)", deviceID);
                return NO;
            }
        }

        state->enabled = enabled;

        NSError* __nullable error = nil;

        try {
            [self rebuildPlaythrough];
        } catch (const CAException& e) {
            // Revert the change and try to restore the previous configuration.
            state->enabled = !enabled;
            BGMLogAndSwallowExceptions("BGMAudioDeviceManager::setDevice:enabled:", [&] {
                [self rebuildPlaythrough];
            });
            error = [NSError errorWithDomain:@kBGMAppBundleID code:e.GetError() userInfo:nil];
        } catch (...) {
            state->enabled = !enabled;
            BGMLogAndSwallowExceptions("BGMAudioDeviceManager::setDevice:enabled:", [&] {
                [self rebuildPlaythrough];
            });
            error =
                [NSError errorWithDomain:@kBGMAppBundleID
                                    code:kAudioHardwareUnspecifiedError
                                userInfo:nil];
        }

        if (error) {
            return NO;
        }

        [self persistOutputDeviceSettings];
        [self propagateOutputDeviceChange];

        return YES;
    } @finally {
        [stateLock unlock];
    }
}

- (void) setDevice:(AudioObjectID)deviceID gain:(float)gain {
    gain = (float)MAX(0.0, MIN(1.0, gain));

    @try {
        [stateLock lock];

        BGMOutputDeviceState* state = [self findStateForDeviceID:deviceID];
        if (!state) {
            outputDeviceStates.push_back({ deviceID, NO, gain, false });
            state = &outputDeviceStates.back();
        }
        state->gain = gain;

        // Changing the slider un-mutes the device.
        state->muted = false;

        // Apply the gain without restarting the IO. (No-op if the device isn't currently one of
        // the playthroughs' output devices.)
        playThrough.SetOutputDeviceGain(deviceID, (Float32)gain);
        playThrough_UISounds.SetOutputDeviceGain(deviceID, (Float32)gain);

        [self persistOutputDeviceSettings];
    } @finally {
        [stateLock unlock];
    }
}

- (void) setDevice:(AudioObjectID)deviceID muted:(BOOL)muted {
    @try {
        [stateLock lock];

        BGMOutputDeviceState* state = [self findStateForDeviceID:deviceID];
        if (!state) {
            outputDeviceStates.push_back({ deviceID, NO, kDefaultOutputDeviceGain, (bool)muted });
            state = &outputDeviceStates.back();
        }

        if (state->muted == (bool)muted) {
            return;  // No change.
        }

        state->muted = (bool)muted;

        // Apply without restarting the IO. (No-op if the device isn't currently one of the
        // playthroughs' output devices.)
        Float32 effectiveGain = (muted ? 0.0f : (Float32)state->gain);
        playThrough.SetOutputDeviceGain(deviceID, effectiveGain);
        playThrough_UISounds.SetOutputDeviceGain(deviceID, effectiveGain);

        [self persistOutputDeviceSettings];
    } @finally {
        [stateLock unlock];
    }
}

- (NSError* __nullable) sampleRateConflictErrorForDevice:(AudioObjectID)deviceID {
    @try {
        [stateLock lock];

        BGMOutputDeviceState* state = [self findStateForDeviceID:deviceID];

        if (!(state && state->enabled)) {
            NSError* __nullable conflict = nil;

            try {
                BGMAudioDevice device(deviceID);
                Float64 deviceRate = device.GetNominalSampleRate();

                for (const auto& s : outputDeviceStates) {
                    if (!s.enabled) {
                        continue;
                    }

                    BGMAudioDevice other(s.deviceID);
                    Float64 otherRate = other.GetNominalSampleRate();

                    if (fabs(deviceRate - otherRate) > 0.5) {
                        conflict =
                            [NSError errorWithDomain:@"com.bearisdriving.BGM.App.SampleRateConflict"
                                                 code:kAudioHardwareUnsupportedOperationError
                                             userInfo:@{
                                NSLocalizedDescriptionKey:
                                    [NSString stringWithFormat:
                                     @"%@ (%.0f Hz) has a different sample rate than the "
                                     @"enabled output device %@ (%.0f Hz). All enabled output "
                                     @"devices must have the same sample rate to play the same "
                                     @"audio.",
                                     CFBridgingRelease(device.CopyName()),
                                     deviceRate,
                                     CFBridgingRelease(other.CopyName()),
                                     otherRate],
                            }];
                        break;
                    }
                }
            } catch (const CAException& e) {
                BGMLogExceptionIn("sampleRateConflictErrorForDevice", e);
            } catch (...) {
                // Can't determine a conflict. Let the caller try the normal path.
            }

            return conflict;
        }

        return nil;
    } @catch (NSException* exception) {
        return nil;
    } @finally {
        [stateLock unlock];
    }
}

- (BOOL) isDeviceEnabled:(AudioObjectID)deviceID {
    @try {
        [stateLock lock];

        BGMOutputDeviceState* state = [self findStateForDeviceID:deviceID];

        return (state ? state->enabled : NO);
    } @finally {
        [stateLock unlock];
    }
}

- (float) gainForDevice:(AudioObjectID)deviceID {
    @try {
        [stateLock lock];

        BGMOutputDeviceState* state = [self findStateForDeviceID:deviceID];

        return (state ? state->gain : kDefaultOutputDeviceGain);
    } @finally {
        [stateLock unlock];
    }
}

- (BOOL) isDeviceMuted:(AudioObjectID)deviceID {
    @try {
        [stateLock lock];

        BGMOutputDeviceState* state = [self findStateForDeviceID:deviceID];

        return (state ? (BOOL)state->muted : NO);
    } @finally {
        [stateLock unlock];
    }
}

- (NSArray<NSNumber*>* __nullable) enabledOutputDeviceIDs {
    @try {
        [stateLock lock];

        NSMutableArray<NSNumber*>* ids = [NSMutableArray new];

        for (const auto& state : outputDeviceStates) {
            if (state.enabled) {
                [ids addObject:@(state.deviceID)];
            }
        }

        return ids;
    } @finally {
        [stateLock unlock];
    }
}

- (BOOL) enablePersistedOutputDevices {
    @try {
        [stateLock lock];

        if (!userDefaults) {
            return NO;
        }

        NSDictionary* __nullable settings = [userDefaults outputDeviceSettings];

        if (![settings isKindOfClass:[NSDictionary class]] || settings.count == 0) {
            return NO;
        }

        // Get the IDs of the connected devices.
        CAHALAudioSystemObject audioSystem;
        UInt32 numDevices = audioSystem.GetNumberAudioDevices();
        CAAutoArrayDelete<AudioObjectID> deviceIDs(numDevices);
        audioSystem.GetAudioDevices(numDevices, deviceIDs);

        // Enable the devices the user previously enabled, restoring their gains.
        std::vector<AudioObjectID> enabledIDs;

        for (NSString* uid in settings) {
            id entry = settings[uid];

            if (![entry isKindOfClass:[NSDictionary class]] ||
                ![entry[@"enabled"] boolValue]) {
                continue;
            }

            for (UInt32 i = 0; i < numDevices; i++) {
                bool match = false;

                BGMLogAndSwallowExceptions("enablePersistedOutputDevices", [&] {
                    CFStringRef deviceUID = BGMAudioDevice(deviceIDs[i]).CopyDeviceUID();
                    match = (CFStringCompare(deviceUID, (__bridge CFStringRef)uid, 0) == kCFCompareEqualTo);
                    CFRelease(deviceUID);
                });

                if (match) {
                    enabledIDs.push_back(deviceIDs[i]);

                    BGMOutputDeviceState* state = [self findStateForDeviceID:deviceIDs[i]];
                    if (!state) {
                        outputDeviceStates.push_back({ deviceIDs[i], YES, kDefaultOutputDeviceGain, false });
                        state = &outputDeviceStates.back();
                    }

                    state->enabled = YES;

                    // Restore the saved gain. A saved gain of 0.0 is valid (the user set the
                    // slider to zero), so only use the default if there's no saved gain at all.
                    NSNumber* __nullable savedGain = entry[@"gain"];
                    if ([savedGain isKindOfClass:[NSNumber class]]) {
                        float gain = (float)[savedGain floatValue];
                        state->gain = MAX(0.0f, MIN(1.0f, gain));
                    } else {
                        state->gain = kDefaultOutputDeviceGain;
                    }

                    // Older saved settings don't have a "muted" key; treat it as unmuted.
                    state->muted = [entry[@"muted"] boolValue];
                    break;
                }
            }
        }

        if (enabledIDs.empty()) {
            return NO;
        }

        // All enabled output devices must share a sample rate (see BGMPlayThrough::Activate). If
        // the persisted set mixes rates (e.g. it was saved before this constraint existed), fall
        // back to the preferred single device rather than activating a broken configuration.
        try {
            Float64 firstRate = BGMAudioDevice(enabledIDs.front()).GetNominalSampleRate();
            for (AudioObjectID id : enabledIDs) {
                if (fabs(BGMAudioDevice(id).GetNominalSampleRate() - firstRate) > 0.5) {
                    LogWarning("BGMAudioDeviceManager::enablePersistedOutputDevices: Persisted "
                               "output devices have different sample rates. Falling back to the "
                               "preferred device.");
                    return NO;
                }
            }
        } catch (const CAException& e) {
            // Can't check the rates. Fall through; rebuildPlaythrough will surface the real error.
        }

        try {
            [self rebuildPlaythrough];
        } catch (const CAException& e) {
            BGMLogExceptionIn("BGMAudioDeviceManager::enablePersistedOutputDevices", e);
            return NO;
        } catch (...) {
            LogError("BGMAudioDeviceManager::enablePersistedOutputDevices: Unknown exception");
            return NO;
        }

        [self persistOutputDeviceSettings];
        [self propagateOutputDeviceChange];

        return YES;
    } @finally {
        [stateLock unlock];
    }
}

- (void) pruneDisconnectedOutputDevices {
    @try {
        [stateLock lock];

        bool changed = false;
        std::vector<BGMOutputDeviceState> kept;

        for (const auto& state : outputDeviceStates) {
            bool exists = false;

            BGMLogAndSwallowExceptions("pruneDisconnectedOutputDevices", [&] {
                exists = CAHALAudioObject::ObjectExists(state.deviceID);
            });

            if (exists) {
                kept.push_back(state);
            } else {
                changed = true;
                LogWarning("BGMAudioDeviceManager::pruneDisconnectedOutputDevices: Output device "
                           "%u is no longer connected", state.deviceID);
            }
        }

        if (!changed || kept.empty()) {
            return;
        }

        outputDeviceStates = std::move(kept);

        bool anyEnabled = false;
        for (const auto& state : outputDeviceStates) {
            if (state.enabled) {
                anyEnabled = true;
                break;
            }
        }

        if (anyEnabled) {
            BGMLogAndSwallowExceptions("BGMAudioDeviceManager::pruneDisconnectedOutputDevices", [&] {
                [self rebuildPlaythrough];
            });
            [self persistOutputDeviceSettings];
            [self propagateOutputDeviceChange];
        }
        // Otherwise no enabled devices are left. BGMPreferredOutputDevices will restore a
        // preferred one.
    } @finally {
        [stateLock unlock];
    }
}

#pragma mark Output Device Persistence and Master Volume

// Persists the per-output-device settings (enabled state, gain, and mute state), keyed by device
// UID.
// Must be called with stateLock held.
- (void) persistOutputDeviceSettings {
    if (!userDefaults) {
        return;
    }

    NSMutableDictionary<NSString*, NSDictionary*>* settings = [NSMutableDictionary new];

    for (const auto& state : outputDeviceStates) {
        @try {
            NSString* uid =
                CFBridgingRelease(BGMAudioDevice(state.deviceID).CopyDeviceUID());
            if (uid) {
                settings[uid] = @{ @"enabled": @(state.enabled),
                                  @"gain": @(state.gain),
                                  @"muted": @(state.muted) };
            }
        } @catch (NSException* exception) {
            // The device threw. Skip it.
        }
    }

    [userDefaults setOutputDeviceSettings:settings];
}

// Pushes BGMDevice's current volume and mute (the master volume) to the playthroughs, so it's
// applied to the output devices in real time. Must be called with stateLock held.
- (void) updatePlayThroughMasterVolumes {
    float master = 1.0f;

    try {
        BGMAudioDevice bgm = *bgmDevice;

        if (bgm.HasSettableMainVolume(kAudioObjectPropertyScopeOutput)) {
            master = (float)bgm.GetVolumeControlScalarValue(kAudioObjectPropertyScopeOutput,
                                                            kMainChannel);

            if (bgm.HasSettableMainMute(kAudioObjectPropertyScopeOutput) &&
                bgm.GetMuteControlValue(kAudioObjectPropertyScopeOutput, kMainChannel)) {
                master = 0.0f;
            }
        }
    } catch (const CAException& e) {
        BGMLogException(e);
    }

    playThrough.SetMasterVolume((Float32)master);
    playThrough_UISounds.SetMasterVolume((Float32)master);
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

    // BGMDevice's volume (the master volume) and mute are applied by BGMPlayThrough in real time,
    // so listen for changes to them and push them to the playthroughs.
    BGMLogAndSwallowExceptions("BGMAudioDeviceManager::registerBGMDeviceListeners", [&] {
        BGMAudioDevice device(bgmDevice->GetObjectID());
        device.AddPropertyListener(
                CAPropertyAddress(kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput),
                &BGMDeviceListenerProc, bridgeSelf);
        device.AddPropertyListener(
                CAPropertyAddress(kAudioDevicePropertyMute, kAudioDevicePropertyScopeOutput),
                &BGMDeviceListenerProc, bridgeSelf);
    });

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

    BGMLogAndSwallowExceptions("BGMAudioDeviceManager::removeBGMDeviceListeners", [&] {
        BGMAudioDevice device(bgmDevice->GetObjectID());
        device.RemovePropertyListener(
                CAPropertyAddress(kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput),
                &BGMDeviceListenerProc, bridgeSelf);
        device.RemovePropertyListener(
                CAPropertyAddress(kAudioDevicePropertyMute, kAudioDevicePropertyScopeOutput),
                &BGMDeviceListenerProc, bridgeSelf);
    });

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

            case kAudioDevicePropertyVolumeScalar:
            case kAudioDevicePropertyMute:
                // BGMDevice's volume (the master volume) or mute changed. (We only listen for these
                // on the main BGMDevice instance, not the UI sounds instance.)
                if (inDeviceID == bgmDevice->GetObjectID()) {
                    [self handleMasterVolumeChanged];
                }
                break;

            default:
                // We might get properties we didn't ask for, so we just ignore them.
                break;
        }
    }
}

// BGMDevice's volume (the master volume) or mute changed. Push the new master volume to the
// playthroughs so it's applied to the output devices in real time.
- (void) handleMasterVolumeChanged {
    // Dispatched so we can take stateLock without risking deadlock.
    dispatch_async(BGMGetDispatchQueue_PriorityUserInteractive(), ^{
        @try {
            [stateLock lock];

            // Don't try to update the playthroughs before an output device has been set.
            if (outputDevice.GetObjectID() == kAudioObjectUnknown) {
                return;
            }

            [self updatePlayThroughMasterVolumes];
        } @finally {
            [stateLock unlock];
        }
    });
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



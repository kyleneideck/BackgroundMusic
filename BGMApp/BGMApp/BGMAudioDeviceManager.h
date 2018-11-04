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
//  BGMAudioDeviceManager.h
//  BGMApp
//
//  Copyright © 2016-2018 Kyle Neideck
//
//  Manages BGMDevice and the output device. Sets the system's current default device as the output
//  device on init, then starts playthrough and mirroring the devices' controls.
//

#if defined(__cplusplus)

// Local Includes
#import "BGMBackgroundMusicDevice.h"

// PublicUtility Includes
#import "CAHALAudioDevice.h"

#endif /* defined(__cplusplus) */

// System Includes
#import <Foundation/Foundation.h>
#import <CoreAudio/AudioHardwareBase.h>

// Forward Declarations
@class BGMOutputVolumeMenuItem;
@class BGMOutputDeviceMenuSection;


#pragma clang assume_nonnull begin

static const int kBGMErrorCode_OutputDeviceNotFound = 1;
static const int kBGMErrorCode_ReturningEarly       = 2;

@interface BGMAudioDeviceManager : NSObject

// Returns nil if BGMDevice isn't installed.
- (instancetype) init;

// Set the BGMOutputVolumeMenuItem to be notified when the output device is changed.
- (void) setOutputVolumeMenuItem:(BGMOutputVolumeMenuItem*)item;

// Set the BGMOutputDeviceMenuSection to be notified when the output device is changed.
- (void) setOutputDeviceMenuSection:(BGMOutputDeviceMenuSection*)menuSection;

// Set BGMDevice as the default audio device for all processes
- (NSError* __nullable) setBGMDeviceAsOSDefault;
// Replace BGMDevice as the default device with the output device
- (NSError* __nullable) unsetBGMDeviceAsOSDefault;

#ifdef __cplusplus
// The virtual device published by BGMDriver.
- (BGMBackgroundMusicDevice) bgmDevice;

// The device BGMApp will play audio through, making it, from the user's perspective, the system's
// default output device.
- (CAHALAudioDevice) outputDevice;
#endif

- (BOOL) isOutputDevice:(AudioObjectID)deviceID;
- (BOOL) isOutputDataSource:(UInt32)dataSourceID;

// Set the audio output device that BGMApp uses.
//
// Returns an error if the output device couldn't be changed. If revertOnFailure is true in that case,
// this method will attempt to set the output device back to the original device. If it fails to
// revert, an additional error will be included in the error's userInfo with the key "revertError".
//
// Both errors' codes will be the code of the exception that caused the failure, if any, generally one
// of the error constants from AudioHardwareBase.h.
//
// Blocks while the old device stops IO (if there was one).
- (NSError* __nullable) setOutputDeviceWithID:(AudioObjectID)deviceID
                              revertOnFailure:(BOOL)revertOnFailure;

// As above, but also sets the new output device's data source. See kAudioDevicePropertyDataSource in
// AudioHardware.h.
- (NSError* __nullable) setOutputDeviceWithID:(AudioObjectID)deviceID
                                 dataSourceID:(UInt32)dataSourceID
                              revertOnFailure:(BOOL)revertOnFailure;

// Start playthrough synchronously. Blocks until IO has started on the output device and playthrough
// is running. See BGMPlayThrough.
//
// Returns one of the error codes defined by this class or BGMPlayThrough, or an AudioHardware error
// code received from the HAL.
- (OSStatus) startPlayThroughSync:(BOOL)forUISoundsDevice;

// When the output device is changed, BGMAudioDeviceManager will send the ID of the new output
// device to BGMXPCHelper through this connection.
- (void) setBGMXPCHelperConnection:(NSXPCConnection* __nullable)connection;

@end

#pragma clang assume_nonnull end


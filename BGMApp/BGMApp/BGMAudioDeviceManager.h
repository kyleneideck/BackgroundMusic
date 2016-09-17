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
//  Copyright © 2016 Kyle Neideck
//
//  Manages the BGMDevice and the output device. Sets the system's current default device as the output device on init, then
//  starts playthrough and mirroring the devices' controls. The output device can be changed but the BGMDevice is fixed.
//

// PublicUtility Includes
#ifdef __cplusplus
#include "CAHALAudioDevice.h"
#endif

// System Includes
#import <Foundation/Foundation.h>
#include <CoreAudio/AudioHardwareBase.h>


extern int const kBGMErrorCode_BGMDeviceNotFound;
extern int const kBGMErrorCode_OutputDeviceNotFound;

@interface BGMAudioDeviceManager : NSObject

- (id) initWithError:(NSError**)error;

// Set BGMDevice as the default audio device for all processes
- (void) setBGMDeviceAsOSDefault;
// Replace BGMDevice as the default device with the output device
- (void) unsetBGMDeviceAsOSDefault;

#ifdef __cplusplus
- (CAHALAudioDevice) bgmDevice;
#endif

- (BOOL) isOutputDevice:(AudioObjectID)deviceID;
// Returns NO if the output device couldn't be changed and has been reverted
- (BOOL) setOutputDeviceWithID:(AudioObjectID)deviceID revertOnFailure:(BOOL)revertOnFailure;

// Returns when IO has started running on the output device (for playthrough).
- (OSStatus) waitForOutputDeviceToStart;

@end


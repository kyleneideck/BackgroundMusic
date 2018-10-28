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
//  BGMPreferredOutputDevices.h
//  BGMApp
//
//  Copyright © 2018 Kyle Neideck
//
//  Tries to change BGMApp's output device when the user plugs in or unplugs an audio device, in the
//  same way macOS would change its default device if Background Music wasn't running.
//
//  For example, if you plug in some USB headphones, make them your default device and then unplug
//  them, macOS will change its default device to the previous default device. Then, if you plug
//  them back in, macOS will make them the default device again.
//
//  This class isn't always able to figure out what macOS would do, in which case it leaves BGMApp's
//  output device as it is.
//

// Local Includes
#import "BGMAudioDeviceManager.h"
#import "BGMUserDefaults.h"

// System Includes
#import <CoreAudio/AudioHardwareBase.h>
#import <Foundation/Foundation.h>


#pragma clang assume_nonnull begin

@interface BGMPreferredOutputDevices : NSObject

// Starts responding to device connections/disconnections immediately. Stops if/when the instance is
// deallocated.
- (instancetype) initWithDevices:(BGMAudioDeviceManager*)devices
                    userDefaults:(BGMUserDefaults*)userDefaults;

// Returns the most-preferred device that's currently connected. If no preferred devices are
// connected, returns the current output device. If the current output device has been disconnected,
// returns an arbitrary device.
//
// If none of the connected devices can be used as the output device, or if it can't find a device
// to use because the HAL returned errors when queried, returns kAudioObjectUnknown.
- (AudioObjectID) findPreferredDevice;

- (void) userChangedOutputDeviceTo:(AudioObjectID)device;

@end

#pragma clang assume_nonnull end


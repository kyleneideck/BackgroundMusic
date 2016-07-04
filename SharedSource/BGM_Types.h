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
//  BGM_Types.h
//  SharedSource
//
//  Copyright © 2016 Kyle Neideck
//

#ifndef __SharedSource__BGM_Types__
#define __SharedSource__BGM_Types__

#include <CoreAudio/AudioServerPlugIn.h>


#pragma mark IDs

// TODO: Change these and the other defines to const strings?
#define kBGMDriverBundleID           "com.bearisdriving.BGM.Driver"
#define kBGMAppBundleID              "com.bearisdriving.BGM.App"
#define kBGMXPCHelperBundleID        "com.bearisdriving.BGM.XPCHelper"

#define kBGMDeviceUID                "BGMDevice"
#define kBGMDeviceModelUID           "BGMDeviceModelUID"

// The object IDs for the audio objects this driver implements.
//
// BGMDevice always publishes this fixed set of objects (regardless of the wrapped device). We might need to
// change that at some point, but so far it hasn't caused any problems and it makes the driver much simpler.
enum
{
	kObjectID_PlugIn					= kAudioObjectPlugInObject,
	kObjectID_Device					= 2,
	kObjectID_Stream_Input				= 3,
	kObjectID_Stream_Output				= 4,
	kObjectID_Volume_Output_Master		= 5,
	kObjectID_Mute_Output_Master		= 6
};

#pragma mark BGMDevice Custom Properties

enum
{
    // TODO: Combine the two music player properties
    
    // The process ID of the music player as a CFNumber. Setting this property will also clear the value of
    // kAudioDeviceCustomPropertyMusicPlayerBundleID. We use 0 to mean unset.
    //
    // There is currently no way for a client to tell whether the process it has set as the music player is a
    // client of the BGMDevice.
    kAudioDeviceCustomPropertyMusicPlayerProcessID                    = 'mppi',
    // The music player's bundle ID as a CFString (UTF8), or the empty string if it's unset/null. Setting this
    // property will also clear the value of kAudioDeviceCustomPropertyMusicPlayerProcessID.
    kAudioDeviceCustomPropertyMusicPlayerBundleID                     = 'mpbi',
    // A CFNumber that specifies whether the device is silent, playing only music (i.e. the client set as the
    // music player is the only client playing audio) or audible. See enum values below. This property is only
    // updated after the audible state has been different for kDeviceAudibleStateMinChangedFramesForUpdate
    // consecutive frames. (To avoid excessive CPU use if for some reason the audible state starts changing
    // very often.)
    kAudioDeviceCustomPropertyDeviceAudibleState                      = 'daud',
    // A CFBoolean similar to kAudioDevicePropertyDeviceIsRunning except it ignores whether IO is running for
    // BGMApp. This is so BGMApp knows when it can stop doing IO to save CPU.
    kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp = 'runo',
    // A CFArray of CFDictionaries that each contain an app's pid, bundle ID and volume relative to other
    // running apps. See the dictionary keys below for more info.
    //
    // Getting this property will only return apps with volumes other than the default. Setting this property
    // will add new app volumes or replace existing ones, but there's currently no way to delete an app from
    // the internal collection.
    kAudioDeviceCustomPropertyAppVolumes                              = 'apvs'
};

// The number of silent/audible frames before BGMDriver will change kAudioDeviceCustomPropertyDeviceAudibleState
#define kDeviceAudibleStateMinChangedFramesForUpdate (2 << 12)

enum
{
    // kAudioDeviceCustomPropertyDeviceAudibleState values
    //
    // No audio is playing on the device's streams (regardless of whether IO is running or not)
    kBGMDeviceIsSilent              = 'silt',
    // The client whose bundle ID matches the current value of kCustomAudioDevicePropertyMusicPlayerBundleID is the
    // only audible client
    kBGMDeviceIsSilentExceptMusic   = 'olym',
    kBGMDeviceIsAudible             = 'audi'
};

// kAudioDeviceCustomPropertyAppVolumes keys
//
// A CFNumber<SInt32> between kAppRelativeVolumeMinRawValue and kAppRelativeVolumeMaxRawValue. A value greater than
// the midpoint increases the client's volume and a value less than the midpoint decreases it. A volume curve is
// applied to kBGMAppVolumesKey_RelativeVolume when it's first set and then each of the app's samples are multiplied
// by it.
#define kBGMAppVolumesKey_RelativeVolume    "rvol"
// The app's pid as a CFNumber. May be omitted if kBGMAppVolumesKey_BundleID is present.
#define kBGMAppVolumesKey_ProcessID         "pid"
// The app's bundle ID as a CFString. May be omitted if kBGMAppVolumesKey_ProcessID is present.
#define kBGMAppVolumesKey_BundleID          "bid"

// Volume curve range for app volumes
#define kAppRelativeVolumeMaxRawValue   100
#define kAppRelativeVolumeMinRawValue   0
#define kAppRelativeVolumeMinDbValue    -96.0f
#define kAppRelativeVolumeMaxDbValue	0.0f

#pragma mark BGMDevice Custom Property Addresses

// For convenience.

static const AudioObjectPropertyAddress kBGMMusicPlayerProcessIDAddress = {
    kAudioDeviceCustomPropertyMusicPlayerProcessID,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};

static const AudioObjectPropertyAddress kBGMMusicPlayerBundleIDAddress = {
    kAudioDeviceCustomPropertyMusicPlayerBundleID,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};

static const AudioObjectPropertyAddress kBGMAudibleStateAddress = {
    kAudioDeviceCustomPropertyDeviceAudibleState,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};

static const AudioObjectPropertyAddress kBGMRunningSomewhereOtherThanBGMAppAddress = {
    kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};

static const AudioObjectPropertyAddress kBGMAppVolumesAddress = {
    kAudioDeviceCustomPropertyAppVolumes,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};

#pragma mark XPC Return Codes

enum {
    kBGMXPC_Success,
    kBGMXPC_MessageFailure,
    kBGMXPC_Timeout,
    kBGMXPC_BGMAppStateError,
    kBGMXPC_HardwareError,
    kBGMXPC_InternalError
};

#pragma mark Exceptions

#if defined(__cplusplus)

class BGM_InvalidClientException { };
class BGM_InvalidClientPIDException { };
class BGM_InvalidClientRelativeVolumeException { };
class BGM_DeviceNotSetException { };
class BGM_RuntimeException { };

#endif

#endif /* __SharedSource__BGM_Types__ */


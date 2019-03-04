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
//  Copyright © 2016, 2017, 2019 Kyle Neideck
//

#ifndef SharedSource__BGM_Types
#define SharedSource__BGM_Types

// STL Includes
#if defined(__cplusplus)
#include <stdexcept>
#endif

// System Includes
#include <CoreAudio/AudioServerPlugIn.h>


#pragma mark Project URLs

static const char* const kBGMProjectURL = "https://github.com/kyleneideck/BackgroundMusic";
static const char* const kBGMIssueTrackerURL = "https://github.com/kyleneideck/BackgroundMusic/issues";

#pragma mark IDs

// TODO: Change these and the other defines to const strings?
#define kBGMDriverBundleID           "com.bearisdriving.BGM.Driver"
#define kBGMAppBundleID              "com.bearisdriving.BGM.App"
#define kBGMXPCHelperBundleID        "com.bearisdriving.BGM.XPCHelper"

#define kBGMDeviceUID                "BGMDevice"
#define kBGMDeviceModelUID           "BGMDeviceModelUID"
#define kBGMDeviceUID_UISounds       "BGMDevice_UISounds"
#define kBGMDeviceModelUID_UISounds  "BGMDeviceModelUID_UISounds"
#define kBGMNullDeviceUID            "BGMNullDevice"
#define kBGMNullDeviceModelUID       "BGMNullDeviceModelUID"

// The object IDs for the audio objects this driver implements.
//
// BGMDevice always publishes this fixed set of objects (except when BGMDevice's volume or mute
// controls are disabled). We might need to change that at some point, but so far it hasn't caused
// any problems and it makes the driver much simpler.
enum
{
	kObjectID_PlugIn                            = kAudioObjectPlugInObject,
    // BGMDevice
	kObjectID_Device                            = 2,   // Belongs to kObjectID_PlugIn
	kObjectID_Stream_Input                      = 3,   // Belongs to kObjectID_Device
	kObjectID_Stream_Output                     = 4,   // Belongs to kObjectID_Device
	kObjectID_Volume_Output_Master              = 5,   // Belongs to kObjectID_Device
	kObjectID_Mute_Output_Master                = 6,   // Belongs to kObjectID_Device
    // Null Device
    kObjectID_Device_Null                       = 7,   // Belongs to kObjectID_PlugIn
    kObjectID_Stream_Null                       = 8,   // Belongs to kObjectID_Device_Null
    // BGMDevice for UI sounds
    kObjectID_Device_UI_Sounds                  = 9,   // Belongs to kObjectID_PlugIn
    kObjectID_Stream_Input_UI_Sounds            = 10,  // Belongs to kObjectID_Device_UI_Sounds
    kObjectID_Stream_Output_UI_Sounds           = 11,  // Belongs to kObjectID_Device_UI_Sounds
    kObjectID_Volume_Output_Master_UI_Sounds    = 12,  // Belongs to kObjectID_Device_UI_Sounds
};

// AudioObjectPropertyElement docs: "Elements are numbered sequentially where 0 represents the
// master element."
static const AudioObjectPropertyElement kMasterChannel = kAudioObjectPropertyElementMaster;

#pragma BGM Plug-in Custom Properties

enum
{
    // A CFBoolean. True if the null device is enabled. Settable, false by default.
    kAudioPlugInCustomPropertyNullDeviceActive = 'nuld'
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
    kAudioDeviceCustomPropertyAppVolumes                              = 'apvs',
    // A CFArray of CFBooleans indicating which of BGMDevice's controls are enabled. All controls are enabled
    // by default. This property is settable. See the array indices below for more info.
    kAudioDeviceCustomPropertyEnabledOutputControls                   = 'bgct'
};

// The number of silent/audible frames before BGMDriver will change kAudioDeviceCustomPropertyDeviceAudibleState
#define kDeviceAudibleStateMinChangedFramesForUpdate (2 << 11)

enum BGMDeviceAudibleState : SInt32
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
// A CFNumber<SInt32> between kAppPanLeftRawValue and kAppPanRightRawValue. A negative value has a higher proportion
// of left channel, and a positive value has a higher proportion of right channel.
#define kBGMAppVolumesKey_PanPosition       "ppos"
// The app's pid as a CFNumber. May be omitted if kBGMAppVolumesKey_BundleID is present.
#define kBGMAppVolumesKey_ProcessID         "pid"
// The app's bundle ID as a CFString. May be omitted if kBGMAppVolumesKey_ProcessID is present.
#define kBGMAppVolumesKey_BundleID          "bid"

// Volume curve range for app volumes
#define kAppRelativeVolumeMaxRawValue   100
#define kAppRelativeVolumeMinRawValue   0
#define kAppRelativeVolumeMinDbValue    -96.0f
#define kAppRelativeVolumeMaxDbValue	0.0f

// Pan position values
#define kAppPanLeftRawValue   -100
#define kAppPanCenterRawValue 0
#define kAppPanRightRawValue  100

// kAudioDeviceCustomPropertyEnabledOutputControls indices
enum
{
    // True if BGMDevice's master output volume control is enabled.
    kBGMEnabledOutputControlsIndex_Volume = 0,
    // True if BGMDevice's master output mute control is enabled.
    kBGMEnabledOutputControlsIndex_Mute   = 1
};

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

static const AudioObjectPropertyAddress kBGMEnabledOutputControlsAddress = {
    kAudioDeviceCustomPropertyEnabledOutputControls,
    kAudioObjectPropertyScopeOutput,
    kAudioObjectPropertyElementMaster
};

#pragma mark XPC Return Codes

enum {
    kBGMXPC_Success,
    kBGMXPC_MessageFailure,
    kBGMXPC_Timeout,
    kBGMXPC_BGMAppStateError,
    kBGMXPC_HardwareError,
    kBGMXPC_ReturningEarlyError,
    kBGMXPC_InternalError
};

#pragma mark Exceptions

#if defined(__cplusplus)

class BGM_InvalidClientException : public std::runtime_error {
public:
    BGM_InvalidClientException() : std::runtime_error("InvalidClient") { }
};

class BGM_InvalidClientPIDException : public std::runtime_error {
public:
    BGM_InvalidClientPIDException() : std::runtime_error("InvalidClientPID") { }
};

class BGM_InvalidClientRelativeVolumeException : public std::runtime_error {
public:
    BGM_InvalidClientRelativeVolumeException() : std::runtime_error("InvalidClientRelativeVolume") { }
};

class BGM_InvalidClientPanPositionException : public std::runtime_error {
public:
    BGM_InvalidClientPanPositionException() : std::runtime_error("InvalidClientPanPosition") { }
};

class BGM_DeviceNotSetException : public std::runtime_error {
public:
    BGM_DeviceNotSetException() : std::runtime_error("DeviceNotSet") { }
};

#endif

// Assume we've failed to start the output device if it isn't running IO after this timeout expires.
//
// Currently set to 30s because some devices, e.g. AirPlay, can legitimately take that long to start.
//
// TODO: Should we have a timeout at all? Is there a notification we can subscribe to that will tell us whether the
//       device is still making progress? Should we regularly poll mOutputDevice.IsAlive() while we're waiting to
//       check it's still responsive?
static const UInt64 kStartIOTimeoutNsec = 30 * NSEC_PER_SEC;

#endif /* SharedSource__BGM_Types */


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
//  Mock_CAHALAudioDevice.cpp
//  BGMAppUnitTests
//
//  Copyright © 2020 Kyle Neideck
//

// Self Include
#include "CAHALAudioDevice.h"

// Local Includes
#include "MockAudioDevice.h"
#include "MockAudioObjects.h"

// BGM Includes
#include "BGM_Types.h"

// PublicUtility Includes
#include "CACFString.h"
#include "CAHALAudioSystemObject.h"
#include "CAPropertyAddress.h"


#pragma clang diagnostic ignored "-Wunused-parameter"

CAHALAudioDevice::CAHALAudioDevice(AudioObjectID inObjectID)
:
    CAHALAudioObject(inObjectID)
{
}

CAHALAudioDevice::CAHALAudioDevice(CFStringRef inUID)
:
        CAHALAudioObject(CAHALAudioSystemObject().GetAudioDeviceForUID(inUID))
{
}

CAHALAudioDevice::~CAHALAudioDevice()
{
}

void	CAHALAudioDevice::GetCurrentVirtualFormats(bool inIsInput, UInt32& ioNumberStreams, AudioStreamBasicDescription* outFormats) const
{
    ioNumberStreams = 1;
    CAPropertyAddress theAddress(kAudioStreamPropertyVirtualFormat);
    UInt32 theSize = sizeof(AudioStreamBasicDescription);
    GetPropertyData(theAddress, 0, NULL, theSize, outFormats);
}

UInt32	CAHALAudioDevice::GetIOBufferSize() const
{
    return MockAudioObjects::GetAudioDevice(GetObjectID())->mIOBufferSize;
}

void	CAHALAudioDevice::SetIOBufferSize(UInt32 inBufferSize)
{
    MockAudioObjects::GetAudioDevice(GetObjectID())->mIOBufferSize = inBufferSize;
}

bool	CAHALAudioDevice::IsAlive() const
{
    return true;
}

AudioDeviceIOProcID	CAHALAudioDevice::CreateIOProcID(AudioDeviceIOProc inIOProc, void* inClientData)
{
    return reinterpret_cast<AudioDeviceIOProcID>(0x99990000);
}

void	CAHALAudioDevice::DestroyIOProcID(AudioDeviceIOProcID inIOProcID)
{
}

Float64	CAHALAudioDevice::GetNominalSampleRate() const
{
    return MockAudioObjects::GetAudioDevice(GetObjectID())->mNominalSampleRate;
}

void	CAHALAudioDevice::SetNominalSampleRate(Float64 inSampleRate)
{
    MockAudioObjects::GetAudioDevice(GetObjectID())->mNominalSampleRate = inSampleRate;
}

CFStringRef    CAHALAudioDevice::CopyDeviceUID() const
{
    std::string uid = MockAudioObjects::GetAudioDevice(GetObjectID())->mUID;
    return CACFString(uid.c_str()).CopyCFString();
}

#pragma mark Unimplemented Methods

bool	CAHALAudioDevice::HasModelUID() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

CFStringRef	CAHALAudioDevice::CopyModelUID() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

CFStringRef	CAHALAudioDevice::CopyConfigurationApplicationBundleID() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

CFURLRef	CAHALAudioDevice::CopyIconLocation() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetTransportType() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::CanBeDefaultDevice(bool inIsInput, bool inIsSystem) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasDevicePlugInStatus() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

OSStatus	CAHALAudioDevice::GetDevicePlugInStatus() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::IsHidden() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

pid_t	CAHALAudioDevice::GetHogModeOwner() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::IsHogModeSettable() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::TakeHogMode()
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::ReleaseHogMode()
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasPreferredStereoChannels(bool inIsInput) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetPreferredStereoChannels(bool inIsInput, UInt32& outLeft, UInt32& outRight) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetPreferredStereoChannels(bool inIsInput, UInt32 inLeft, UInt32 inRight)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasPreferredChannelLayout(bool inIsInput) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetPreferredChannelLayout(bool inIsInput, AudioChannelLayout& outChannelLayout) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetPreferredStereoChannels(bool inIsInput, AudioChannelLayout& inChannelLayout)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetNumberRelatedAudioDevices() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetRelatedAudioDevices(UInt32& ioNumberRelatedDevices, AudioObjectID* outRelatedDevices) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

AudioObjectID	CAHALAudioDevice::GetRelatedAudioDeviceByIndex(UInt32 inIndex) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetNumberStreams(bool inIsInput) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetStreams(bool inIsInput, UInt32& ioNumberStreams, AudioObjectID* outStreamList) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

AudioObjectID	CAHALAudioDevice::GetStreamByIndex(bool inIsInput, UInt32 inIndex) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetTotalNumberChannels(bool inIsInput) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetCurrentPhysicalFormats(bool inIsInput, UInt32& ioNumberStreams, AudioStreamBasicDescription* outFormats) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::IsRunning() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::IsRunningSomewhere() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetLatency(bool inIsInput) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetSafetyOffset(bool inIsInput) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasClockDomain() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetClockDomain() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

Float64	CAHALAudioDevice::GetActualSampleRate() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetNumberAvailableNominalSampleRateRanges() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetAvailableNominalSampleRateRanges(UInt32& ioNumberRanges, AudioValueRange* outRanges) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetAvailableNominalSampleRateRangeByIndex(UInt32 inIndex, Float64& outMinimum, Float64& outMaximum) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::IsValidNominalSampleRate(Float64 inSampleRate) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::IsIOBufferSizeSettable() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::UsesVariableIOBufferSizes() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetMaximumVariableIOBufferSize() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasIOBufferSizeRange() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetIOBufferSizeRange(UInt32& outMinimum, UInt32& outMaximum) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::StartIOProc(AudioDeviceIOProcID inIOProcID)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::StartIOProcAtTime(AudioDeviceIOProcID inIOProcID, AudioTimeStamp& ioStartTime, bool inIsInput, bool inIgnoreHardware)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::StopIOProc(AudioDeviceIOProcID inIOProcID)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetIOProcStreamUsage(AudioDeviceIOProcID inIOProcID, bool inIsInput, bool* outStreamUsage) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetIOProcStreamUsage(AudioDeviceIOProcID inIOProcID, bool inIsInput, const bool* inStreamUsage)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

Float32	CAHALAudioDevice::GetIOCycleUsage() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetIOCycleUsage(Float32 inValue)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetCurrentTime(AudioTimeStamp& outTime)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::TranslateTime(const AudioTimeStamp& inTime, AudioTimeStamp& outTime)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetNearestStartTime(AudioTimeStamp& ioTime, bool inIsInput, bool inIgnoreHardware)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasVolumeControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::VolumeControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

Float32	CAHALAudioDevice::GetVolumeControlScalarValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

Float32	CAHALAudioDevice::GetVolumeControlDecibelValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetVolumeControlScalarValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetVolumeControlDecibelValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

Float32	CAHALAudioDevice::GetVolumeControlScalarForDecibelValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

Float32	CAHALAudioDevice::GetVolumeControlDecibelForScalarValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasSubVolumeControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::SubVolumeControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

Float32	CAHALAudioDevice::GetSubVolumeControlScalarValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

Float32	CAHALAudioDevice::GetSubVolumeControlDecibelValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetSubVolumeControlScalarValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetSubVolumeControlDecibelValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

Float32	CAHALAudioDevice::GetSubVolumeControlScalarForDecibelValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

Float32	CAHALAudioDevice::GetSubVolumeControlDecibelForScalarValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasMuteControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::MuteControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::GetMuteControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetMuteControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel, bool inValue)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasSoloControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::SoloControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::GetSoloControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetSoloControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel, bool inValue)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasStereoPanControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::StereoPanControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

Float32	CAHALAudioDevice::GetStereoPanControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetStereoPanControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetStereoPanControlChannels(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32& outLeftChannel, UInt32& outRightChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasJackControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::GetJackControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasSubMuteControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::SubMuteControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::GetSubMuteControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetSubMuteControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel, bool inValue)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasiSubOwnerControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::iSubOwnerControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::GetiSubOwnerControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetiSubOwnerControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel, bool inValue)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasDataSourceControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::DataSourceControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetCurrentDataSourceID(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetCurrentDataSourceByID(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32 inID)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetNumberAvailableDataSources(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetAvailableDataSources(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32& ioNumberSources, UInt32* outSources) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetAvailableDataSourceByIndex(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32 inIndex) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

CFStringRef	CAHALAudioDevice::CopyDataSourceNameForID(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32 inID) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasDataDestinationControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::DataDestinationControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetCurrentDataDestinationID(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetCurrentDataDestinationByID(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32 inID)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetNumberAvailableDataDestinations(AudioObjectPropertyScope inScope, UInt32 inChannel) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetAvailableDataDestinations(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32& ioNumberDestinations, UInt32* outDestinations) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetAvailableDataDestinationByIndex(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32 inIndex) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

CFStringRef	CAHALAudioDevice::CopyDataDestinationNameForID(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32 inID) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::HasClockSourceControl() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioDevice::ClockSourceControlIsSettable() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetCurrentClockSourceID() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::SetCurrentClockSourceByID(UInt32 inID)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetNumberAvailableClockSources() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioDevice::GetAvailableClockSources(UInt32& ioNumberSources, UInt32* outSources) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetAvailableClockSourceByIndex(UInt32 inIndex) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

CFStringRef	CAHALAudioDevice::CopyClockSourceNameForID(UInt32 inID) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioDevice::GetClockSourceKindForID(UInt32 inID) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}


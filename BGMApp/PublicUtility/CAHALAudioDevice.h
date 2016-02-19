/*
     File: CAHALAudioDevice.h
 Abstract: Part of CoreAudio Utility Classes
  Version: 1.1
 
 Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple
 Inc. ("Apple") in consideration of your agreement to the following
 terms, and your use, installation, modification or redistribution of
 this Apple software constitutes acceptance of these terms.  If you do
 not agree with these terms, please do not use, install, modify or
 redistribute this Apple software.
 
 In consideration of your agreement to abide by the following terms, and
 subject to these terms, Apple grants you a personal, non-exclusive
 license, under Apple's copyrights in this original Apple software (the
 "Apple Software"), to use, reproduce, modify and redistribute the Apple
 Software, with or without modifications, in source and/or binary forms;
 provided that if you redistribute the Apple Software in its entirety and
 without modifications, you must retain this notice and the following
 text and disclaimers in all such redistributions of the Apple Software.
 Neither the name, trademarks, service marks or logos of Apple Inc. may
 be used to endorse or promote products derived from the Apple Software
 without specific prior written permission from Apple.  Except as
 expressly stated in this notice, no other rights or licenses, express or
 implied, are granted by Apple herein, including but not limited to any
 patent rights that may be infringed by your derivative works or by other
 works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
 MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
 THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
 OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
 AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
 STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 
 Copyright (C) 2014 Apple Inc. All Rights Reserved.
 
*/
#if !defined(__CAHALAudioDevice_h__)
#define __CAHALAudioDevice_h__

//==================================================================================================
//	Includes
//==================================================================================================

//	Super Class Includes
#include "CAHALAudioObject.h"

//	PublicUtility Includes
#include "CADebugMacros.h"
#include "CAException.h"

//==================================================================================================
//	CAHALAudioDevice
//==================================================================================================

class CAHALAudioDevice
:
	public	CAHALAudioObject
{

//	Construction/Destruction
public:
						CAHALAudioDevice(AudioObjectID inAudioDevice);
						CAHALAudioDevice(CFStringRef inUID);
	virtual				~CAHALAudioDevice();

//	General Stuff
public:
	CFStringRef			CopyDeviceUID() const;
	bool				HasModelUID() const;
	CFStringRef			CopyModelUID() const;
	CFStringRef			CopyConfigurationApplicationBundleID() const;
	CFURLRef			CopyIconLocation() const;
	UInt32				GetTransportType() const;
	bool				CanBeDefaultDevice(bool inIsInput, bool inIsSystem) const;
	bool				HasDevicePlugInStatus() const;
	OSStatus			GetDevicePlugInStatus() const;
	bool				IsAlive() const;
	bool				IsHidden() const;
	pid_t				GetHogModeOwner() const;
	bool				IsHogModeSettable() const;
	bool				TakeHogMode();
	void				ReleaseHogMode();
	bool				HasPreferredStereoChannels(bool inIsInput) const;
	void				GetPreferredStereoChannels(bool inIsInput, UInt32& outLeft, UInt32& outRight) const;
	void				SetPreferredStereoChannels(bool inIsInput, UInt32 inLeft, UInt32 inRight);
	bool				HasPreferredChannelLayout(bool inIsInput) const;
	void				GetPreferredChannelLayout(bool inIsInput, AudioChannelLayout& outChannelLayout) const;
	void				SetPreferredStereoChannels(bool inIsInput, AudioChannelLayout& inChannelLayout);
	UInt32				GetNumberRelatedAudioDevices() const;
	void				GetRelatedAudioDevices(UInt32& ioNumberRelatedDevices, AudioObjectID* outRelatedDevices) const;
	AudioObjectID		GetRelatedAudioDeviceByIndex(UInt32 inIndex) const;

//	Stream Stuff
public:
	UInt32				GetNumberStreams(bool inIsInput) const;
	void				GetStreams(bool inIsInput, UInt32& ioNumberStreams, AudioObjectID* outStreamList) const;
	AudioObjectID		GetStreamByIndex(bool inIsInput, UInt32 inIndex) const;
	UInt32				GetTotalNumberChannels(bool inIsInput) const;
	void				GetCurrentVirtualFormats(bool inIsInput, UInt32& ioNumberStreams, AudioStreamBasicDescription* outFormats) const;
	void				GetCurrentPhysicalFormats(bool inIsInput, UInt32& ioNumberStreams, AudioStreamBasicDescription* outFormats) const;
	
//	IO Stuff
public:
	bool				IsRunning() const;
	bool				IsRunningSomewhere() const;
	UInt32				GetLatency(bool inIsInput) const;
	UInt32				GetSafetyOffset(bool inIsInput) const;
	bool				HasClockDomain() const;
	UInt32				GetClockDomain() const;
	Float64				GetActualSampleRate() const;
	Float64				GetNominalSampleRate() const;
	void				SetNominalSampleRate(Float64 inSampleRate);
	UInt32				GetNumberAvailableNominalSampleRateRanges() const;
	void				GetAvailableNominalSampleRateRanges(UInt32& ioNumberRanges, AudioValueRange* outRanges) const;
	void				GetAvailableNominalSampleRateRangeByIndex(UInt32 inIndex, Float64& outMinimum, Float64& outMaximum) const;
	bool				IsValidNominalSampleRate(Float64 inSampleRate) const;
	bool				IsIOBufferSizeSettable() const;
	UInt32				GetIOBufferSize() const;
	void				SetIOBufferSize(UInt32 inBufferSize);
	bool				UsesVariableIOBufferSizes() const;
	UInt32				GetMaximumVariableIOBufferSize() const;
	bool				HasIOBufferSizeRange() const;
	void				GetIOBufferSizeRange(UInt32& outMinimum, UInt32& outMaximum) const;
	AudioDeviceIOProcID	CreateIOProcID(AudioDeviceIOProc inIOProc, void* inClientData);
	AudioDeviceIOProcID	CreateIOProcIDWithBlock(dispatch_queue_t inDispatchQueue, AudioDeviceIOBlock inIOBlock);
	void				DestroyIOProcID(AudioDeviceIOProcID inIOProcID);
	void				StartIOProc(AudioDeviceIOProcID inIOProcID);
	void				StartIOProcAtTime(AudioDeviceIOProcID inIOProcID, AudioTimeStamp& ioStartTime, bool inIsInput, bool inIgnoreHardware);
	void				StopIOProc(AudioDeviceIOProcID inIOProcID);
	void				GetIOProcStreamUsage(AudioDeviceIOProcID inIOProcID, bool inIsInput, bool* outStreamUsage) const;
	void				SetIOProcStreamUsage(AudioDeviceIOProcID inIOProcID, bool inIsInput, const bool* inStreamUsage);
	Float32				GetIOCycleUsage() const;
	void				SetIOCycleUsage(Float32 inValue);
	
//	Time Operations
public:
	void				GetCurrentTime(AudioTimeStamp& outTime);
	void				TranslateTime(const AudioTimeStamp& inTime, AudioTimeStamp& outTime);
	void				GetNearestStartTime(AudioTimeStamp& ioTime, bool inIsInput, bool inIgnoreHardware);

//	Controls
public:
	bool				HasVolumeControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				VolumeControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	Float32				GetVolumeControlScalarValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	Float32				GetVolumeControlDecibelValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	void				SetVolumeControlScalarValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue);
	void				SetVolumeControlDecibelValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue);
	Float32				GetVolumeControlScalarForDecibelValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue) const;
	Float32				GetVolumeControlDecibelForScalarValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue) const;
	
	bool				HasSubVolumeControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				SubVolumeControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	Float32				GetSubVolumeControlScalarValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	Float32				GetSubVolumeControlDecibelValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	void				SetSubVolumeControlScalarValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue);
	void				SetSubVolumeControlDecibelValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue);
	Float32				GetSubVolumeControlScalarForDecibelValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue) const;
	Float32				GetSubVolumeControlDecibelForScalarValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue) const;
	
	bool				HasMuteControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				MuteControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				GetMuteControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	void				SetMuteControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel, bool inValue);
	
	bool				HasSoloControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				SoloControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				GetSoloControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	void				SetSoloControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel, bool inValue);
	
	bool				HasStereoPanControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				StereoPanControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	Float32				GetStereoPanControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	void				SetStereoPanControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel, Float32 inValue);
	void				GetStereoPanControlChannels(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32& outLeftChannel, UInt32& outRightChannel) const;
	
	bool				HasJackControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				GetJackControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	
	bool				HasSubMuteControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				SubMuteControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				GetSubMuteControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	void				SetSubMuteControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel, bool inValue);
	
	bool				HasiSubOwnerControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				iSubOwnerControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				GetiSubOwnerControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	void				SetiSubOwnerControlValue(AudioObjectPropertyScope inScope, UInt32 inChannel, bool inValue);

	bool				HasDataSourceControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				DataSourceControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	UInt32				GetCurrentDataSourceID(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	void				SetCurrentDataSourceByID(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32 inID);
	UInt32				GetNumberAvailableDataSources(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	void				GetAvailableDataSources(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32& ioNumberSources, UInt32* outSources) const;
	UInt32				GetAvailableDataSourceByIndex(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32 inIndex) const;
	CFStringRef			CopyDataSourceNameForID(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32 inID) const;

	bool				HasDataDestinationControl(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	bool				DataDestinationControlIsSettable(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	UInt32				GetCurrentDataDestinationID(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	void				SetCurrentDataDestinationByID(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32 inID);
	UInt32				GetNumberAvailableDataDestinations(AudioObjectPropertyScope inScope, UInt32 inChannel) const;
	void				GetAvailableDataDestinations(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32& ioNumberDestinations, UInt32* outDestinations) const;
	UInt32				GetAvailableDataDestinationByIndex(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32 inIndex) const;
	CFStringRef			CopyDataDestinationNameForID(AudioObjectPropertyScope inScope, UInt32 inChannel, UInt32 inID) const;

	bool				HasClockSourceControl() const;
	bool				ClockSourceControlIsSettable() const;
	UInt32				GetCurrentClockSourceID() const;
	void				SetCurrentClockSourceByID(UInt32 inID);
	UInt32				GetNumberAvailableClockSources() const;
	void				GetAvailableClockSources(UInt32& ioNumberSources, UInt32* outSources) const;
	UInt32				GetAvailableClockSourceByIndex(UInt32 inIndex) const;
	CFStringRef			CopyClockSourceNameForID(UInt32 inID) const;
	UInt32				GetClockSourceKindForID(UInt32 inID) const;
	
};

inline AudioDeviceIOProcID	CAHALAudioDevice::CreateIOProcIDWithBlock(dispatch_queue_t inDispatchQueue, AudioDeviceIOBlock inIOBlock)
{
	AudioDeviceIOProcID theAnswer = NULL;
	OSStatus theError = AudioDeviceCreateIOProcIDWithBlock(&theAnswer, mObjectID, inDispatchQueue, inIOBlock);
	ThrowIfError(theError, CAException(theError), "CAHALAudioDevice::CreateIOProcIDWithBlock: got an error creating the IOProc ID");
	return theAnswer;
}

#endif

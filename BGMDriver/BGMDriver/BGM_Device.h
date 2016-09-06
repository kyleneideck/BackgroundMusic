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
//  BGM_Device.h
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//  Portions copyright (C) 2013 Apple Inc. All Rights Reserved.
//
//  Based largely on SA_Device.h from Apple's SimpleAudioDriver Plug-In sample code.
//  https://developer.apple.com/library/mac/samplecode/AudioDriverExamples
//

#ifndef __BGMDriver__BGM_Device__
#define __BGMDriver__BGM_Device__

// SuperClass Includes
#include "BGM_Object.h"

// Local Includes
#include "BGM_Types.h"
#include "BGM_WrappedAudioEngine.h"
#include "BGM_Clients.h"
#include "BGM_TaskQueue.h"

// PublicUtility Includes
#include "CAMutex.h"
#include "CAVolumeCurve.h"

// System Includes
#include <CoreFoundation/CoreFoundation.h>


class BGM_Device
:
	public BGM_Object
{

#pragma mark Construction/Destruction
    
public:
    static BGM_Device&			GetInstance();
    
private:
    static void					StaticInitializer();

protected:
                                BGM_Device();
    virtual						~BGM_Device();
    
    virtual void				Activate();
    virtual void				Deactivate();
    
private:
    void                        InitLoopback();
	
#pragma mark Property Operations
    
public:
	virtual bool				HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	virtual bool				IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	virtual UInt32				GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* __nullable inQualifierData) const;
	virtual void				GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* __nullable inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* __nonnull outData) const;
	virtual void				SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* __nullable inQualifierData, UInt32 inDataSize, const void* __nonnull inData);

#pragma mark Device Property Operations
    
private:
	bool						Device_HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	bool						Device_IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	UInt32						Device_GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* __nullable inQualifierData) const;
	void						Device_GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* __nullable inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* __nonnull outData) const;
	void						Device_SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* __nullable inQualifierData, UInt32 inDataSize, const void* __nonnull inData);

#pragma mark Stream Property Operations
    
private:
	bool						Stream_HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	bool						Stream_IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	UInt32						Stream_GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* __nullable inQualifierData) const;
	void						Stream_GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* __nullable inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* __nonnull outData) const;
	void						Stream_SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* __nullable inQualifierData, UInt32 inDataSize, const void* __nonnull inData);

#pragma mark Control Property Operations
    
private:
	bool						Control_HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	bool						Control_IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	UInt32						Control_GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* __nullable inQualifierData) const;
	void						Control_GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* __nullable inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* __nonnull outData) const;
	void						Control_SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* __nullable inQualifierData, UInt32 inDataSize, const void* __nonnull inData);

#pragma mark IO Operations
    
public:
	void						StartIO(UInt32 inClientID);
	void						StopIO(UInt32 inClientID);
    
	void						GetZeroTimeStamp(Float64& outSampleTime, UInt64& outHostTime, UInt64& outSeed);
	
	void						WillDoIOOperation(UInt32 inOperationID, bool& outWillDo, bool& outWillDoInPlace) const;
	void						BeginIOOperation(UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo& inIOCycleInfo, UInt32 inClientID);
	void						DoIOOperation(AudioObjectID inStreamObjectID, UInt32 inClientID, UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo& inIOCycleInfo, void* __nonnull ioMainBuffer, void* __nullable ioSecondaryBuffer);
	void						EndIOOperation(UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo& inIOCycleInfo, UInt32 inClientID);

private:
	void						ReadInputData(UInt32 inIOBufferFrameSize, Float64 inSampleTime, void* __nonnull outBuffer);
    void						WriteOutputData(UInt32 inIOBufferFrameSize, Float64 inSampleTime, const void* __nonnull inBuffer);
    void                        ApplyClientRelativeVolume(UInt32 inClientID, UInt32 inIOBufferFrameSize, void* __nonnull inBuffer) const;
    bool                        BufferIsAudible(UInt32 inIOBufferFrameSize, const void* __nonnull inBuffer);
    void                        UpdateAudibleStateSampleTimes_PreMix(UInt32 inClientID, UInt32 inIOBufferFrameSize, Float64 inOutputSampleTime, const void* __nonnull inBuffer);
    void                        UpdateAudibleStateSampleTimes_PostMix(UInt32 inIOBufferFrameSize, Float64 inOutputSampleTime, const void* __nonnull inBuffer);
    void                        UpdateDeviceAudibleState(UInt32 inIOBufferFrameSize, Float64 inOutputSampleTime);

#pragma mark Hardware Accessors
    
private:
	void						_HW_Open();
	void						_HW_Close();
	kern_return_t				_HW_StartIO();
	void						_HW_StopIO();
	Float64						_HW_GetSampleRate() const;
	kern_return_t				_HW_SetSampleRate(Float64 inNewSampleRate);
	UInt32						_HW_GetRingBufferFrameSize() const;
	SInt32						_HW_GetVolumeControlValue(AudioObjectID inObjectID) const;
	kern_return_t				_HW_SetVolumeControlValue(AudioObjectID inObjectID, SInt32 inNewControlValue);
    UInt32                      _HW_GetMuteControlValue(AudioObjectID inObjectID) const;
    kern_return_t               _HW_SetMuteControlValue(AudioObjectID inObjectID, UInt32 inValue);

#pragma mark Implementation
    
public:
	CFStringRef __nonnull		CopyDeviceUID() const { return CFSTR(kBGMDeviceUID); }
    void                        AddClient(const AudioServerPlugInClientInfo* __nonnull inClientInfo);
    void                        RemoveClient(const AudioServerPlugInClientInfo* __nonnull inClientInfo);
	void						PerformConfigChange(UInt64 inChangeAction, void* __nullable inChangeInfo);
	void						AbortConfigChange(UInt64 inChangeAction, void* __nullable inChangeInfo);

private:
    static pthread_once_t		sStaticInitializer;
    static BGM_Device* __nonnull    sInstance;
    
    #define kDeviceName                 "Background Music Device"
    #define kDeviceManufacturerName     "Background Music contributors"
    
	enum
	{
								kNumberOfSubObjects					= 4,
								kNumberOfInputSubObjects			= 1,
								kNumberOfOutputSubObjects			= 3,
								
								kNumberOfStreams					= 2,
								kNumberOfInputStreams				= 1,
								kNumberOfOutputStreams				= 1,
								
								kNumberOfControls					= 2
	};
    
    CAMutex                     mStateMutex;
    CAMutex						mIOMutex;
    
    UInt64 __unused				mSampleRateShadow;  // Currently unused.
    const Float64               kSampleRateDefault = 44100.0;
    // Before we can change sample rate, the host has to stop the device. The new sample rate is stored here while it does.
    Float64                     mPendingSampleRate;
    
    BGM_WrappedAudioEngine* __nullable mWrappedAudioEngine;
    
    BGM_TaskQueue               mTaskQueue;
    
    BGM_Clients                 mClients;
    
    #define kLoopbackRingBufferFrameSize    16384
    Float64                     mLoopbackSampleRate;
	Float32						mLoopbackRingBuffer[kLoopbackRingBufferFrameSize * 2];
    // TODO: a comment explaining why we need a clock for loopback-only mode
    struct {
        Float64					hostTicksPerFrame = 0.0;
        UInt64					numberTimeStamps  = 0;
        UInt64					anchorHostTime    = 0;
    }                           mLoopbackTime;
	
	bool						mInputStreamIsActive;
    bool						mOutputStreamIsActive;
    
    SInt32                      mDeviceAudibleState;
    struct
    {
        Float64                 latestAudibleNonMusic;
        Float64                 latestSilent;
        Float64                 latestAudibleMusic;
        Float64                 latestSilentMusic;
    }                           mAudibleStateSampleTimes;
    
    // This volume range will be used when the BGMDevice isn't wrapping another device (or we fail to
    // get the range of the wrapped device for some reason).
    #define kDefaultMinRawVolumeValue   0
    #define kDefaultMaxRawVolumeValue	96
    #define kDefaultMinDbVolumeValue	-96.0f
    #define kDefaultMaxDbVolumeValue	0.0f
	
	SInt32						mOutputMasterVolumeControlRawValueShadow;
    SInt32                      mOutputMasterMinRawVolumeShadow;
    SInt32                      mOutputMasterMaxRawVolumeShadow;
    Float32                     mOutputMasterMinDbVolumeShadow;
    Float32                     mOutputMasterMaxDbVolumeShadow;
	CAVolumeCurve				mVolumeCurve;
    UInt32                      mOutputMuteValueShadow;

};

#endif /* __BGMDriver__BGM_Device__ */


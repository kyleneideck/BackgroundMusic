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
//  Copyright © 2016, 2017, 2019 Kyle Neideck
//  Copyright © 2019 Gordon Childs
//  Copyright (C) 2013 Apple Inc. All Rights Reserved.
//
//  Based largely on SA_Device.h from Apple's SimpleAudioDriver Plug-In sample code.
//  https://developer.apple.com/library/mac/samplecode/AudioDriverExamples
//

#ifndef BGMDriver__BGM_Device
#define BGMDriver__BGM_Device

// SuperClass Includes
#include "BGM_AbstractDevice.h"

// Local Includes
#include "BGM_Types.h"
#include "BGM_WrappedAudioEngine.h"
#include "BGM_Clients.h"
#include "BGM_TaskQueue.h"
#include "BGM_AudibleState.h"
#include "BGM_Stream.h"
#include "BGM_VolumeControl.h"
#include "BGM_MuteControl.h"

// PublicUtility Includes
#include "CAMutex.h"
#include "CAVolumeCurve.h"
#include "CARingBuffer.h"

// System Includes
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>


class BGM_Device
:
	public BGM_AbstractDevice
{

#pragma mark Construction/Destruction
    
public:
    static BGM_Device&			GetInstance();
    static BGM_Device&			GetUISoundsInstance();
    
private:
    static void					StaticInitializer();

protected:
                                BGM_Device(AudioObjectID inObjectID,
										   const CFStringRef __nonnull inDeviceName,
                                           const CFStringRef __nonnull inDeviceUID,
										   const CFStringRef __nonnull inDeviceModelUID,
                                           AudioObjectID inInputStreamID,
                                           AudioObjectID inOutputStreamID,
                                           AudioObjectID inOutputVolumeControlID,
										   AudioObjectID inOutputMuteControlID);
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

#pragma mark Accessors

public:
	/*!
	 Enable or disable the device's volume and/or mute controls. This function is async because it
	 has to ask the host to stop IO for the device before the controls can be enabled/disabled.

	 See BGM_Device::PerformConfigChange and RequestDeviceConfigurationChange in AudioServerPlugIn.h.
	 */
    void                        RequestEnabledControls(bool inVolumeEnabled, bool inMuteEnabled);

    Float64						GetSampleRate() const;
    void                        RequestSampleRate(Float64 inRequestedSampleRate);

private:
	/*!
     @return The Audio Object that has the ID inObjectID and belongs to this device.
     @throws CAException if there is no such Audio Object.
     */
	const BGM_Object&		    GetOwnedObjectByID(AudioObjectID inObjectID) const;
	BGM_Object&                 GetOwnedObjectByID(AudioObjectID inObjectID);

	/*! @return The number of Audio Objects belonging to this device, e.g. streams and controls. */
	UInt32 						GetNumberOfSubObjects() const;
	/*! @return The number of Audio Objects with output scope belonging to this device. */
    UInt32 						GetNumberOfOutputSubObjects() const;
	/*!
	 @return The number of control Audio Objects with output scope belonging to this device, e.g.
	         output volume and mute controls.
	 */
    UInt32 						GetNumberOfOutputControls() const;
    /*!
     Enable or disable the device's volume and/or mute controls.

     Private because (after initialisation) this can only be called after asking the host to stop IO
     for the device. See BGM_Device::RequestEnabledControls, BGM_Device::PerformConfigChange and
     RequestDeviceConfigurationChange in AudioServerPlugIn.h.
     */
    void                        SetEnabledControls(bool inVolumeEnabled, bool inMuteEnabled);
    /*!
     Set the device's sample rate.

     Private because (after initialisation) this can only be called after asking the host to stop IO
     for the device. See BGM_Device::RequestEnabledControls, BGM_Device::PerformConfigChange and
     RequestDeviceConfigurationChange in AudioServerPlugIn.h.

     @param inNewSampleRate The sample rate.
     @param force If true, set the sample rate on the device even if it's currently set to
                  inNewSampleRate.
     @throws CAException if inNewSampleRate < 1 or if applying the sample rate to one of the streams
             fails.
     */
    void                        SetSampleRate(Float64 inNewSampleRate, bool force = false);

    /*! @return True if inObjectID is the ID of one of this device's streams. */
    inline bool                 IsStreamID(AudioObjectID inObjectID) const noexcept;

#pragma mark Hardware Accessors
    
private:
	void						_HW_Open();
	void						_HW_Close();
	kern_return_t				_HW_StartIO();
	void						_HW_StopIO();
	Float64						_HW_GetSampleRate() const;
	kern_return_t				_HW_SetSampleRate(Float64 inNewSampleRate);
	UInt32						_HW_GetRingBufferFrameSize() const;

#pragma mark Implementation
    
public:
    CFStringRef __nonnull		CopyDeviceUID() const { return mDeviceUID; }
    void                        AddClient(const AudioServerPlugInClientInfo* __nonnull inClientInfo);
    void                        RemoveClient(const AudioServerPlugInClientInfo* __nonnull inClientInfo);
    /*!
     Apply a change requested with BGM_PlugIn::Host_RequestDeviceConfigurationChange. See
     PerformDeviceConfigurationChange in AudioServerPlugIn.h.
     */
	void						PerformConfigChange(UInt64 inChangeAction, void* __nullable inChangeInfo);
    /*! Cancel a change requested with BGM_PlugIn::Host_RequestDeviceConfigurationChange. */
	void						AbortConfigChange(UInt64 inChangeAction, void* __nullable inChangeInfo);

private:
    static pthread_once_t		sStaticInitializer;
    static BGM_Device* __nonnull    sInstance;
    static BGM_Device* __nonnull    sUISoundsInstance;
    
    #define kDeviceName                 "Background Music"
    #define kDeviceName_UISounds        "Background Music (UI Sounds)"
    #define kDeviceManufacturerName     "Background Music contributors"

	const CFStringRef __nonnull	mDeviceName;
	const CFStringRef __nonnull mDeviceUID;
	const CFStringRef __nonnull mDeviceModelUID;

	enum
	{
		// The number of global/output sub-objects varies because the controls can be disabled.
								kNumberOfInputSubObjects			= 1,

								kNumberOfStreams					= 2,
								kNumberOfInputStreams				= 1,
								kNumberOfOutputStreams				= 1
	};

    CAMutex                     mStateMutex;
    CAMutex						mIOMutex;
    
    const Float64               kSampleRateDefault = 44100.0;
    // Before we can change sample rate, the host has to stop the device. The new sample rate is
    // stored here while it does.
    Float64                     mPendingSampleRate = kSampleRateDefault;
    
    BGM_WrappedAudioEngine* __nullable mWrappedAudioEngine;
    
    BGM_TaskQueue               mTaskQueue;
    
    BGM_Clients                 mClients;
    
    #define kLoopbackRingBufferFrameSize    16384
    Float64                     mLoopbackSampleRate;
    CARingBuffer                mLoopbackRingBuffer;

    // TODO: a comment explaining why we need a clock for loopback-only mode
    struct {
        Float64					hostTicksPerFrame = 0.0;
        UInt64					numberTimeStamps  = 0;
        UInt64					anchorHostTime    = 0;
    }                           mLoopbackTime;
	
    BGM_Stream                  mInputStream;
    BGM_Stream                  mOutputStream;

    BGM_AudibleState            mAudibleState;

    enum class ChangeAction : UInt64
    {
        SetSampleRate,
        SetEnabledControls
    };

    BGM_VolumeControl			mVolumeControl;
	BGM_MuteControl				mMuteControl;
    bool                        mPendingOutputVolumeControlEnabled = true;
    bool                        mPendingOutputMuteControlEnabled   = true;

};

#endif /* BGMDriver__BGM_Device */


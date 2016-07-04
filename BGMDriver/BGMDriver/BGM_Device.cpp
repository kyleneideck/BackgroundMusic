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
//  BGM_Device.cpp
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//  Portions copyright (C) 2013 Apple Inc. All Rights Reserved.
//
//  Based largely on SA_Device.cpp from Apple's SimpleAudioDriver Plug-In sample code. Also uses a few sections from Apple's
//  NullAudio.c sample code (found in the same sample project).
//  https://developer.apple.com/library/mac/samplecode/AudioDriverExamples
//

// Self Include
#include "BGM_Device.h"

// Local Includes
#include "BGM_PlugIn.h"
#include "BGM_XPCHelper.h"

// PublicUtility Includes
#include "CADispatchQueue.h"
#include "CAException.h"
#include "CACFArray.h"
#include "CACFString.h"
#include "CADebugMacros.h"
#include "CAAtomic.h"

// System Includes
#include <mach/mach_time.h>

// STL Includes
#include <algorithm>


#pragma mark Construction/Destruction

pthread_once_t				BGM_Device::sStaticInitializer = PTHREAD_ONCE_INIT;
BGM_Device*					BGM_Device::sInstance = NULL;

BGM_Device&	BGM_Device::GetInstance()
{
    pthread_once(&sStaticInitializer, StaticInitializer);
    return *sInstance;
}

void	BGM_Device::StaticInitializer()
{
    try
    {
        sInstance = new BGM_Device;
        sInstance->Activate();
    }
    catch(...)
    {
        DebugMsg("BGM_Device::StaticInitializer: failed to create the device");
        delete sInstance;
        sInstance = NULL;
    }
}

BGM_Device::BGM_Device()
:
	BGM_Object(kObjectID_Device, kAudioDeviceClassID, kAudioObjectClassID, kAudioObjectPlugInObject),
	mStateMutex("Device State"),
	mIOMutex("Device IO"),
	mSampleRateShadow(0),
    mDeviceAudibleState(kBGMDeviceIsSilent),
    mAudibleStateSampleTimes({0, 0, 0, 0}),
    mClients(&mTaskQueue),
    mWrappedAudioEngine(NULL),
	mInputStreamIsActive(true),
	mOutputStreamIsActive(true),
	//mInputMasterVolumeControlRawValueShadow(kDefaultMinRawVolumeValue),
	mOutputMasterVolumeControlRawValueShadow(kDefaultMinRawVolumeValue),
    mOutputMasterMinRawVolumeShadow(kDefaultMinRawVolumeValue),
    mOutputMasterMaxRawVolumeShadow(kDefaultMaxRawVolumeValue),
    mOutputMasterMinDbVolumeShadow(kDefaultMinDbVolumeValue),
    mOutputMasterMaxDbVolumeShadow(kDefaultMaxDbVolumeValue),
    mOutputMuteValueShadow(0)
{
	// Setup the volume curve with the one range
    mVolumeCurve.AddRange(kDefaultMinRawVolumeValue, kDefaultMaxRawVolumeValue, kDefaultMinDbVolumeValue, kDefaultMaxDbVolumeValue);
    
    // Initialises the loopback clock with the default sample rate and, if there is one, sets the wrapped device to the same sample rate
    _HW_SetSampleRate(kSampleRateDefault);
}

BGM_Device::~BGM_Device()
{
}

void	BGM_Device::Activate()
{
	//	Open the connection to the driver and initialize things.
	//_HW_Open();
	
	//	Call the super-class, which just marks the object as active
	BGM_Object::Activate();
}

void	BGM_Device::Deactivate()
{
	//	When this method is called, the object is basically dead, but we still need to be thread
	//	safe. In this case, we also need to be safe vs. any IO threads, so we need to take both
	//	locks.
	CAMutex::Locker theStateLocker(mStateMutex);
	CAMutex::Locker theIOLocker(mIOMutex);
	
	//	mark the object inactive by calling the super-class
	BGM_Object::Deactivate();
	
	//	close the connection to the driver
	//_HW_Close();
}

void    BGM_Device::InitLoopback()
{
    //	Calculate the host ticks per frame for the loopback timer
    struct mach_timebase_info theTimeBaseInfo;
    mach_timebase_info(&theTimeBaseInfo);
    Float64 theHostClockFrequency = theTimeBaseInfo.denom / theTimeBaseInfo.numer;
    theHostClockFrequency *= 1000000000.0;
    mLoopbackTime.hostTicksPerFrame = theHostClockFrequency / mLoopbackSampleRate;
    
    //  Zero-out the loopback buffer
    //  2 channels * 32-bit float = bytes in each frame
    memset(mLoopbackRingBuffer, 0, sizeof(Float32) * 2 * kLoopbackRingBufferFrameSize);
}

#pragma mark Property Operations

bool	BGM_Device::HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	This object implements several API-level objects. So the first thing to do is to figure out
	//	which object this request is really for. Note that mObjectID is an invariant as this
	//	driver's structure does not change dynamically. It will always have the parts it has.
	bool theAnswer = false;
	if(inObjectID == mObjectID)
	{
		theAnswer = Device_HasProperty(inObjectID, inClientPID, inAddress);
	}
	else if((inObjectID == kObjectID_Stream_Input) || (inObjectID == kObjectID_Stream_Output))
	{
		theAnswer = Stream_HasProperty(inObjectID, inClientPID, inAddress);
	}
	else if(/*(inObjectID == mInputMasterVolumeControlObjectID) ||*/ (inObjectID == kObjectID_Volume_Output_Master) || (inObjectID == kObjectID_Mute_Output_Master))
	{
		theAnswer = Control_HasProperty(inObjectID, inClientPID, inAddress);
	}
	else
	{
		Throw(CAException(kAudioHardwareBadObjectError));
	}
	return theAnswer;
}

bool	BGM_Device::IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	bool theAnswer = false;
	if(inObjectID == mObjectID)
	{
		theAnswer = Device_IsPropertySettable(inObjectID, inClientPID, inAddress);
	}
	else if((inObjectID == kObjectID_Stream_Input) || (inObjectID == kObjectID_Stream_Output))
	{
		theAnswer = Stream_IsPropertySettable(inObjectID, inClientPID, inAddress);
	}
	else if(/*(inObjectID == mInputMasterVolumeControlObjectID) ||*/ (inObjectID == kObjectID_Volume_Output_Master) || (inObjectID == kObjectID_Mute_Output_Master))
	{
		theAnswer = Control_IsPropertySettable(inObjectID, inClientPID, inAddress);
	}
	else
	{
		Throw(CAException(kAudioHardwareBadObjectError));
	}
	return theAnswer;
}

UInt32	BGM_Device::GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const
{
	UInt32 theAnswer = 0;
	if(inObjectID == mObjectID)
	{
		theAnswer = Device_GetPropertyDataSize(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData);
	}
	else if((inObjectID == kObjectID_Stream_Input) || (inObjectID == kObjectID_Stream_Output))
	{
		theAnswer = Stream_GetPropertyDataSize(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData);
	}
	else if(/*(inObjectID == mInputMasterVolumeControlObjectID) ||*/ (inObjectID == kObjectID_Volume_Output_Master) || (inObjectID == kObjectID_Mute_Output_Master))
	{
		theAnswer = Control_GetPropertyDataSize(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData);
	}
	else
	{
		Throw(CAException(kAudioHardwareBadObjectError));
	}
	return theAnswer;
}

void	BGM_Device::GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* outData) const
{
    ThrowIfNULL(outData, BGM_RuntimeException(), "BGM_Device::GetPropertyData: !outData");
    
	if(inObjectID == mObjectID)
	{
		Device_GetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
	}
	else if((inObjectID == kObjectID_Stream_Input) || (inObjectID == kObjectID_Stream_Output))
	{
		Stream_GetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
	}
	else if(/*(inObjectID == mInputMasterVolumeControlObjectID) ||*/ (inObjectID == kObjectID_Volume_Output_Master) || (inObjectID == kObjectID_Mute_Output_Master))
	{
		Control_GetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
	}
	else
	{
		Throw(CAException(kAudioHardwareBadObjectError));
	}
}

void	BGM_Device::SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData)
{
    ThrowIfNULL(inData, BGM_RuntimeException(), "BGM_Device::SetPropertyData: no data");
    
	if(inObjectID == mObjectID)
	{
		Device_SetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
	}
	else if((inObjectID == kObjectID_Stream_Input) || (inObjectID == kObjectID_Stream_Output))
	{
		Stream_SetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
	}
	else if(/*(inObjectID == mInputMasterVolumeControlObjectID) ||*/ (inObjectID == kObjectID_Volume_Output_Master) || (inObjectID == kObjectID_Mute_Output_Master))
	{
		Control_SetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
	}
	else
	{
		Throw(CAException(kAudioHardwareBadObjectError));
	}
}

#pragma mark Device Property Operations

bool	BGM_Device::Device_HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Device_GetPropertyData() method.
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyName:
		case kAudioObjectPropertyManufacturer:
		case kAudioDevicePropertyDeviceUID:
		case kAudioDevicePropertyModelUID:
		case kAudioDevicePropertyTransportType:
		case kAudioDevicePropertyRelatedDevices:
		case kAudioDevicePropertyClockDomain:
		case kAudioDevicePropertyDeviceIsAlive:
		case kAudioDevicePropertyDeviceIsRunning:
		case kAudioObjectPropertyControlList:
		case kAudioDevicePropertyNominalSampleRate:
		case kAudioDevicePropertyAvailableNominalSampleRates:
		case kAudioDevicePropertyIsHidden:
		case kAudioDevicePropertyZeroTimeStampPeriod:
        case kAudioDevicePropertyStreams:
        case kAudioDevicePropertyIcon:
        case kAudioObjectPropertyCustomPropertyInfoList:
        case kAudioDeviceCustomPropertyDeviceAudibleState:
        case kAudioDeviceCustomPropertyMusicPlayerProcessID:
        case kAudioDeviceCustomPropertyMusicPlayerBundleID:
        case kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp:
        case kAudioDeviceCustomPropertyAppVolumes:
			theAnswer = true;
			break;
			
		case kAudioDevicePropertyLatency:
		case kAudioDevicePropertySafetyOffset:
		case kAudioDevicePropertyPreferredChannelsForStereo:
		case kAudioDevicePropertyPreferredChannelLayout:
		case kAudioDevicePropertyDeviceCanBeDefaultDevice:
		case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
			theAnswer = (inAddress.mScope == kAudioObjectPropertyScopeInput) || (inAddress.mScope == kAudioObjectPropertyScopeOutput);
			break;
			
		default:
			theAnswer = BGM_Object::HasProperty(inObjectID, inClientPID, inAddress);
			break;
	};
	return theAnswer;
}

bool	BGM_Device::Device_IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Device_GetPropertyData() method.
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyName:
		case kAudioObjectPropertyManufacturer:
		case kAudioDevicePropertyDeviceUID:
		case kAudioDevicePropertyModelUID:
		case kAudioDevicePropertyTransportType:
		case kAudioDevicePropertyRelatedDevices:
		case kAudioDevicePropertyClockDomain:
		case kAudioDevicePropertyDeviceIsAlive:
		case kAudioDevicePropertyDeviceIsRunning:
		case kAudioDevicePropertyDeviceCanBeDefaultDevice:
		case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
		case kAudioDevicePropertyLatency:
		case kAudioDevicePropertyStreams:
		case kAudioObjectPropertyControlList:
		case kAudioDevicePropertySafetyOffset:
		case kAudioDevicePropertyAvailableNominalSampleRates:
		case kAudioDevicePropertyIsHidden:
		case kAudioDevicePropertyPreferredChannelsForStereo:
		case kAudioDevicePropertyPreferredChannelLayout:
		case kAudioDevicePropertyZeroTimeStampPeriod:
        case kAudioDevicePropertyIcon:
        case kAudioObjectPropertyCustomPropertyInfoList:
        case kAudioDeviceCustomPropertyDeviceAudibleState:
        case kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp:
			theAnswer = false;
			break;
            
        case kAudioDevicePropertyNominalSampleRate:
        case kAudioDeviceCustomPropertyMusicPlayerProcessID:
        case kAudioDeviceCustomPropertyMusicPlayerBundleID:
        case kAudioDeviceCustomPropertyAppVolumes:
			theAnswer = true;
			break;
		
		default:
			theAnswer = BGM_Object::IsPropertySettable(inObjectID, inClientPID, inAddress);
			break;
	};
	return theAnswer;
}

UInt32	BGM_Device::Device_GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Device_GetPropertyData() method.
	
	UInt32 theAnswer = 0;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyName:
			theAnswer = sizeof(CFStringRef);
			break;
			
		case kAudioObjectPropertyManufacturer:
			theAnswer = sizeof(CFStringRef);
			break;
			
		case kAudioObjectPropertyOwnedObjects:
			switch(inAddress.mScope)
			{
				case kAudioObjectPropertyScopeGlobal:
					theAnswer = kNumberOfSubObjects * sizeof(AudioObjectID);
					break;
					
				case kAudioObjectPropertyScopeInput:
					theAnswer = kNumberOfInputSubObjects * sizeof(AudioObjectID);
					break;
					
				case kAudioObjectPropertyScopeOutput:
					theAnswer = kNumberOfOutputSubObjects * sizeof(AudioObjectID);
					break;
			};
			break;

		case kAudioDevicePropertyDeviceUID:
			theAnswer = sizeof(CFStringRef);
			break;

		case kAudioDevicePropertyModelUID:
			theAnswer = sizeof(CFStringRef);
			break;

		case kAudioDevicePropertyTransportType:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyRelatedDevices:
			theAnswer = sizeof(AudioObjectID);
			break;

		case kAudioDevicePropertyClockDomain:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyDeviceIsAlive:
			theAnswer = sizeof(AudioClassID);
			break;

		case kAudioDevicePropertyDeviceIsRunning:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyDeviceCanBeDefaultDevice:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyLatency:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyStreams:
			switch(inAddress.mScope)
			{
				case kAudioObjectPropertyScopeGlobal:
					theAnswer = kNumberOfStreams * sizeof(AudioObjectID);
					break;
					
				case kAudioObjectPropertyScopeInput:
					theAnswer = kNumberOfInputStreams * sizeof(AudioObjectID);
					break;
					
				case kAudioObjectPropertyScopeOutput:
					theAnswer = kNumberOfOutputStreams * sizeof(AudioObjectID);
					break;
			};
			break;

		case kAudioObjectPropertyControlList:
			theAnswer = kNumberOfControls * sizeof(AudioObjectID);
			break;

		case kAudioDevicePropertySafetyOffset:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyNominalSampleRate:
			theAnswer = sizeof(Float64);
			break;

		case kAudioDevicePropertyAvailableNominalSampleRates:
			theAnswer = 1 * sizeof(AudioValueRange);
			break;
		
		case kAudioDevicePropertyIsHidden:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioDevicePropertyPreferredChannelsForStereo:
			theAnswer = 2 * sizeof(UInt32);
			break;

		case kAudioDevicePropertyPreferredChannelLayout:
			theAnswer = offsetof(AudioChannelLayout, mChannelDescriptions) + (2 * sizeof(AudioChannelDescription));
			break;

		case kAudioDevicePropertyZeroTimeStampPeriod:
			theAnswer = sizeof(UInt32);
            break;
            
        case kAudioDevicePropertyIcon:
            theAnswer = sizeof(CFURLRef);
            break;
            
        case kAudioObjectPropertyCustomPropertyInfoList:
            theAnswer = sizeof(AudioServerPlugInCustomPropertyInfo) * 5;
            break;
            
        case kAudioDeviceCustomPropertyDeviceAudibleState:
            theAnswer = sizeof(CFNumberRef);
            break;

        case kAudioDeviceCustomPropertyMusicPlayerProcessID:
            theAnswer = sizeof(CFPropertyListRef);
			break;
            
        case kAudioDeviceCustomPropertyMusicPlayerBundleID:
            theAnswer = sizeof(CFStringRef);
            break;
            
        case kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp:
            theAnswer = sizeof(CFBooleanRef);
            break;
            
        case kAudioDeviceCustomPropertyAppVolumes:
            theAnswer = sizeof(CFPropertyListRef);
            break;
		
		default:
			theAnswer = BGM_Object::GetPropertyDataSize(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData);
			break;
	};
	return theAnswer;
}

void	BGM_Device::Device_GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* outData) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required.
	//	Also, since most of the data that will get returned is static, there are few instances where
	//	it is necessary to lock the state mutex.

	UInt32 theNumberItemsToFetch;
	UInt32 theItemIndex;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyName:
			//	This is the human readable name of the device. Note that in this case we return a
			//	value that is a key into the localizable strings in this bundle. This allows us to
			//	return a localized name for the device.
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioObjectPropertyManufacturer for the device");
			*reinterpret_cast<CFStringRef*>(outData) = CFSTR(kDeviceName);
			outDataSize = sizeof(CFStringRef);
			break;
			
		case kAudioObjectPropertyManufacturer:
			//	This is the human readable name of the maker of the plug-in. Note that in this case
			//	we return a value that is a key into the localizable strings in this bundle. This
			//	allows us to return a localized name for the manufacturer.
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioObjectPropertyManufacturer for the device");
			*reinterpret_cast<CFStringRef*>(outData) = CFSTR(kDeviceManufacturerName);
			outDataSize = sizeof(CFStringRef);
			break;
			
		case kAudioObjectPropertyOwnedObjects:
			//	Calculate the number of items that have been requested. Note that this
			//	number is allowed to be smaller than the actual size of the list. In such
			//	case, only that number of items will be returned
			theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);
			
			//	The device owns its streams and controls. Note that what is returned here
			//	depends on the scope requested.
			switch(inAddress.mScope)
			{
				case kAudioObjectPropertyScopeGlobal:
					//	global scope means return all objects
					if(theNumberItemsToFetch > kNumberOfSubObjects)
					{
						theNumberItemsToFetch = kNumberOfSubObjects;
					}
					
					//	fill out the list with as many objects as requested, which is everything
					if(theNumberItemsToFetch > 0)
					{
						reinterpret_cast<AudioObjectID*>(outData)[0] = kObjectID_Stream_Input;
					}
					if(theNumberItemsToFetch > 1)
					{
						reinterpret_cast<AudioObjectID*>(outData)[1] = kObjectID_Stream_Output;
					}
                    /*
					if(theNumberItemsToFetch > 2)
					{
						reinterpret_cast<AudioObjectID*>(outData)[2] = mInputMasterVolumeControlObjectID;
					}
                     */
					if(theNumberItemsToFetch > 2)
					{
						reinterpret_cast<AudioObjectID*>(outData)[2] = kObjectID_Volume_Output_Master;
                    }
                    if(theNumberItemsToFetch > 3)
                    {
                        reinterpret_cast<AudioObjectID*>(outData)[3] = kObjectID_Mute_Output_Master;
                    }
					break;
					
				case kAudioObjectPropertyScopeInput:
					//	input scope means just the objects on the input side
					if(theNumberItemsToFetch > kNumberOfInputSubObjects)
					{
						theNumberItemsToFetch = kNumberOfInputSubObjects;
					}
					
					//	fill out the list with the right objects
					if(theNumberItemsToFetch > 0)
					{
						reinterpret_cast<AudioObjectID*>(outData)[0] = kObjectID_Stream_Input;
					}
                    /*
					if(theNumberItemsToFetch > 1)
					{
						reinterpret_cast<AudioObjectID*>(outData)[1] = mInputMasterVolumeControlObjectID;
					}
                     */
					break;
					
				case kAudioObjectPropertyScopeOutput:
					//	output scope means just the objects on the output side
					if(theNumberItemsToFetch > kNumberOfOutputSubObjects)
					{
						theNumberItemsToFetch = kNumberOfOutputSubObjects;
					}
					
					//	fill out the list with the right objects
					if(theNumberItemsToFetch > 0)
					{
						reinterpret_cast<AudioObjectID*>(outData)[0] = kObjectID_Stream_Output;
					}
					if(theNumberItemsToFetch > 1)
					{
						reinterpret_cast<AudioObjectID*>(outData)[1] = kObjectID_Volume_Output_Master;
                    }
                    if(theNumberItemsToFetch > 2)
                    {
                        reinterpret_cast<AudioObjectID*>(outData)[2] = kObjectID_Mute_Output_Master;
                    }
					break;
			};
			
			//	report how much we wrote
			outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
			break;

		case kAudioDevicePropertyDeviceUID:
			//	This is a CFString that is a persistent token that can identify the same
			//	audio device across boot sessions. Note that two instances of the same
			//	device must have different values for this property.
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyDeviceUID for the device");
			*reinterpret_cast<CFStringRef*>(outData) = CFSTR(kBGMDeviceUID);
			outDataSize = sizeof(CFStringRef);
			break;

		case kAudioDevicePropertyModelUID:
			//	This is a CFString that is a persistent token that can identify audio
			//	devices that are the same kind of device. Note that two instances of the
			//	save device must have the same value for this property.
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyModelUID for the device");
			*reinterpret_cast<CFStringRef*>(outData) = CFSTR(kBGMDeviceModelUID);
			outDataSize = sizeof(CFStringRef);
			break;

		case kAudioDevicePropertyTransportType:
			//	This value represents how the device is attached to the system. This can be
			//	any 32 bit integer, but common values for this property are defined in
			//	<CoreAudio/AudioHardwareBase.h>
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyTransportType for the device");
			*reinterpret_cast<UInt32*>(outData) = kAudioDeviceTransportTypeVirtual;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyRelatedDevices:
			//	The related devices property identifies device objects that are very closely
			//	related. Generally, this is for relating devices that are packaged together
			//	in the hardware such as when the input side and the output side of a piece
			//	of hardware can be clocked separately and therefore need to be represented
			//	as separate AudioDevice objects. In such case, both devices would report
			//	that they are related to each other. Note that at minimum, a device is
			//	related to itself, so this list will always be at least one item long.

			//	Calculate the number of items that have been requested. Note that this
			//	number is allowed to be smaller than the actual size of the list. In such
			//	case, only that number of items will be returned
			theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);
			
			//	we only have the one device...
			if(theNumberItemsToFetch > 1)
			{
				theNumberItemsToFetch = 1;
			}
			
			//	Write the devices' object IDs into the return value
			if(theNumberItemsToFetch > 0)
			{
				reinterpret_cast<AudioObjectID*>(outData)[0] = GetObjectID();
			}
			
			//	report how much we wrote
			outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
			break;

		case kAudioDevicePropertyClockDomain:
			//	This property allows the device to declare what other devices it is
			//	synchronized with in hardware. The way it works is that if two devices have
			//	the same value for this property and the value is not zero, then the two
			//	devices are synchronized in hardware. Note that a device that either can't
			//	be synchronized with others or doesn't know should return 0 for this
			//	property.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyClockDomain for the device");
            *reinterpret_cast<UInt32*>(outData) = 0;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyDeviceIsAlive:
			//	This property returns whether or not the device is alive. Note that it is
			//	note uncommon for a device to be dead but still momentarily availble in the
			//	device list. In the case of this device, it will always be alive.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyDeviceIsAlive for the device");
			*reinterpret_cast<UInt32*>(outData) = 1;
			outDataSize = sizeof(UInt32);
			break;
            
		case kAudioDevicePropertyDeviceIsRunning:
			//	This property returns whether or not IO is running for the device.
            ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyDeviceIsRunning for the device");
            *reinterpret_cast<UInt32*>(outData) = mClients.ClientsRunningIO();
            outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyDeviceCanBeDefaultDevice:
			//	This property returns whether or not the device wants to be able to be the
			//	default device for content. This is the device that iTunes and QuickTime
			//	will use to play their content on and FaceTime will use as it's microphone.
			//	Nearly all devices should allow for this.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyDeviceCanBeDefaultDevice for the device");
			*reinterpret_cast<UInt32*>(outData) = 1;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
			//	This property returns whether or not the device wants to be the system
			//	default device. This is the device that is used to play interface sounds and
			//	other incidental or UI-related sounds on. Most devices should allow this
			//	although devices with lots of latency may not want to.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyDeviceCanBeDefaultSystemDevice for the device");
			*reinterpret_cast<UInt32*>(outData) = 1;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyLatency:
			//	This property returns the presentation latency of the device.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyLatency for the device");
			*reinterpret_cast<UInt32*>(outData) = 0;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyStreams:
			//	Calculate the number of items that have been requested. Note that this
			//	number is allowed to be smaller than the actual size of the list. In such
			//	case, only that number of items will be returned
			theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);
			
			//	Note that what is returned here depends on the scope requested.
			switch(inAddress.mScope)
			{
				case kAudioObjectPropertyScopeGlobal:
					//	global scope means return all streams
					if(theNumberItemsToFetch > kNumberOfStreams)
					{
						theNumberItemsToFetch = kNumberOfStreams;
					}
					
					//	fill out the list with as many objects as requested
					if(theNumberItemsToFetch > 0)
					{
						reinterpret_cast<AudioObjectID*>(outData)[0] = kObjectID_Stream_Input;
					}
					if(theNumberItemsToFetch > 1)
					{
						reinterpret_cast<AudioObjectID*>(outData)[1] = kObjectID_Stream_Output;
					}
					break;
					
				case kAudioObjectPropertyScopeInput:
					//	input scope means just the objects on the input side
					if(theNumberItemsToFetch > kNumberOfInputStreams)
					{
						theNumberItemsToFetch = kNumberOfInputStreams;
					}
					
					//	fill out the list with as many objects as requested
					if(theNumberItemsToFetch > 0)
					{
						reinterpret_cast<AudioObjectID*>(outData)[0] = kObjectID_Stream_Input;
					}
					break;
					
				case kAudioObjectPropertyScopeOutput:
					//	output scope means just the objects on the output side
					if(theNumberItemsToFetch > kNumberOfOutputStreams)
					{
						theNumberItemsToFetch = kNumberOfOutputStreams;
					}
					
					//	fill out the list with as many objects as requested
					if(theNumberItemsToFetch > 0)
					{
						reinterpret_cast<AudioObjectID*>(outData)[0] = kObjectID_Stream_Output;
					}
					break;
			};
			
			//	report how much we wrote
			outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
			break;

		case kAudioObjectPropertyControlList:
			//	Calculate the number of items that have been requested. Note that this
			//	number is allowed to be smaller than the actual size of the list. In such
			//	case, only that number of items will be returned
			theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);
			if(theNumberItemsToFetch > kNumberOfControls)
			{
				theNumberItemsToFetch = kNumberOfControls;
			}
			
			//	fill out the list with as many objects as requested, which is everything
            /*
			if(theNumberItemsToFetch > 0)
			{
				reinterpret_cast<AudioObjectID*>(outData)[0] = mInputMasterVolumeControlObjectID;
			}
             */
            if(theNumberItemsToFetch > 0)
            {
                reinterpret_cast<AudioObjectID*>(outData)[0] = kObjectID_Volume_Output_Master;
            }
            if(theNumberItemsToFetch > 1)
            {
                reinterpret_cast<AudioObjectID*>(outData)[1] = kObjectID_Mute_Output_Master;
            }
			
			//	report how much we wrote
			outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
			break;

		case kAudioDevicePropertySafetyOffset:
			//	This property returns the how close to now the HAL can read and write.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertySafetyOffset for the device");
			*reinterpret_cast<UInt32*>(outData) = 0;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyNominalSampleRate:
			//	This property returns the nominal sample rate of the device. Note that we
			//	only need to take the state lock to get this value.
			{
				ThrowIf(inDataSize < sizeof(Float64), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyNominalSampleRate for the device");
			
				//	The sample rate is protected by the state lock
				CAMutex::Locker theStateLocker(mStateMutex);

				*reinterpret_cast<Float64*>(outData) = _HW_GetSampleRate();
				outDataSize = sizeof(Float64);
			}
			break;

		case kAudioDevicePropertyAvailableNominalSampleRates:
			//	This returns all nominal sample rates the device supports as an array of
			//	AudioValueRangeStructs. Note that for discrete sampler rates, the range
			//	will have the minimum value equal to the maximum value.
            //
            //  BGMDevice supports any sample rate so it can be set to match the output
            //  device when in loopback mode.
			
			//	Calculate the number of items that have been requested. Note that this
			//	number is allowed to be smaller than the actual size of the list. In such
			//	case, only that number of items will be returned
			theNumberItemsToFetch = inDataSize / sizeof(AudioValueRange);
			
			//	clamp it to the number of items we have
			if(theNumberItemsToFetch > 1)
			{
				theNumberItemsToFetch = 1;
			}
			
			//	fill out the return array
			if(theNumberItemsToFetch > 0)
			{
                // 0 would cause divide-by-zero errors in other BGM_Device functions (and
                // wouldn't make sense anyway).
                ((AudioValueRange*)outData)[0].mMinimum = 1.0;
                // Just in case DBL_MAX would cause problems in a client for some reason,
                // use an arbitrary very large number instead. (It wouldn't make sense to
                // actually set the sample rate this high, but I don't know what a
                // reasonable maximum would be.)
                ((AudioValueRange*)outData)[0].mMaximum = 1000000000.0;
			}
			
			//	report how much we wrote
			outDataSize = theNumberItemsToFetch * sizeof(AudioValueRange);
			break;
		
		case kAudioDevicePropertyIsHidden:
			//	This returns whether or not the device is visible to clients.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyIsHidden for the device");
			*reinterpret_cast<UInt32*>(outData) = 0;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioDevicePropertyPreferredChannelsForStereo:
			//	This property returns which two channesl to use as left/right for stereo
			//	data by default. Note that the channel numbers are 1-based.
			ThrowIf(inDataSize < (2 * sizeof(UInt32)), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyPreferredChannelsForStereo for the device");
			((UInt32*)outData)[0] = 1;
			((UInt32*)outData)[1] = 2;
			outDataSize = 2 * sizeof(UInt32);
			break;

		case kAudioDevicePropertyPreferredChannelLayout:
			//	This property returns the default AudioChannelLayout to use for the device
			//	by default. For this device, we return a stereo ACL.
			{
				UInt32 theACLSize = offsetof(AudioChannelLayout, mChannelDescriptions) + (2 * sizeof(AudioChannelDescription));
				ThrowIf(inDataSize < theACLSize, CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyPreferredChannelLayout for the device");
				((AudioChannelLayout*)outData)->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
				((AudioChannelLayout*)outData)->mChannelBitmap = 0;
				((AudioChannelLayout*)outData)->mNumberChannelDescriptions = 2;
				for(theItemIndex = 0; theItemIndex < 2; ++theItemIndex)
				{
					((AudioChannelLayout*)outData)->mChannelDescriptions[theItemIndex].mChannelLabel = kAudioChannelLabel_Left + theItemIndex;
					((AudioChannelLayout*)outData)->mChannelDescriptions[theItemIndex].mChannelFlags = 0;
					((AudioChannelLayout*)outData)->mChannelDescriptions[theItemIndex].mCoordinates[0] = 0;
					((AudioChannelLayout*)outData)->mChannelDescriptions[theItemIndex].mCoordinates[1] = 0;
					((AudioChannelLayout*)outData)->mChannelDescriptions[theItemIndex].mCoordinates[2] = 0;
				}
				outDataSize = theACLSize;
			}
			break;

		case kAudioDevicePropertyZeroTimeStampPeriod:
			//	This property returns how many frames the HAL should expect to see between
			//	successive sample times in the zero time stamps this device provides.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyZeroTimeStampPeriod for the device");
			*reinterpret_cast<UInt32*>(outData) = kLoopbackRingBufferFrameSize;
			outDataSize = sizeof(UInt32);
            break;
            
        case kAudioDevicePropertyIcon:
            {
                // This property is a CFURL that points to the device's icon in the plugin's resource bundle
                ThrowIf(inDataSize < sizeof(CFURLRef), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDevicePropertyIcon for the device");
                
                CFBundleRef theBundle = CFBundleGetBundleWithIdentifier(BGM_PlugIn::GetInstance().GetBundleID());
                ThrowIf(theBundle == NULL, CAException(kAudioHardwareUnspecifiedError), "BGM_Device::Device_GetPropertyData: could not get the plugin bundle for kAudioDevicePropertyIcon");
                
                CFURLRef theURL = CFBundleCopyResourceURL(theBundle, CFSTR("DeviceIcon.icns"), NULL, NULL);
                ThrowIf(theURL == NULL, CAException(kAudioHardwareUnspecifiedError), "BGM_Device::Device_GetPropertyData: could not get the URL for kAudioDevicePropertyIcon");
                
                *reinterpret_cast<CFURLRef*>(outData) = theURL;
                outDataSize = sizeof(CFURLRef);
            }
            break;
            
        case kAudioObjectPropertyCustomPropertyInfoList:
            theNumberItemsToFetch = inDataSize / sizeof(AudioServerPlugInCustomPropertyInfo);
            
            //	clamp it to the number of items we have
            if(theNumberItemsToFetch > 5)
            {
                theNumberItemsToFetch = 5;
            }
            
            if(theNumberItemsToFetch > 0)
            {
                ((AudioServerPlugInCustomPropertyInfo*)outData)[0].mSelector = kAudioDeviceCustomPropertyAppVolumes;
                ((AudioServerPlugInCustomPropertyInfo*)outData)[0].mPropertyDataType = kAudioServerPlugInCustomPropertyDataTypeCFPropertyList;
                ((AudioServerPlugInCustomPropertyInfo*)outData)[0].mQualifierDataType = kAudioServerPlugInCustomPropertyDataTypeNone;
            }
            if(theNumberItemsToFetch > 1)
            {
                ((AudioServerPlugInCustomPropertyInfo*)outData)[1].mSelector = kAudioDeviceCustomPropertyMusicPlayerProcessID;
                ((AudioServerPlugInCustomPropertyInfo*)outData)[1].mPropertyDataType = kAudioServerPlugInCustomPropertyDataTypeCFPropertyList;
                ((AudioServerPlugInCustomPropertyInfo*)outData)[1].mQualifierDataType = kAudioServerPlugInCustomPropertyDataTypeNone;
            }
            if(theNumberItemsToFetch > 2)
            {
                ((AudioServerPlugInCustomPropertyInfo*)outData)[2].mSelector = kAudioDeviceCustomPropertyMusicPlayerBundleID;
                ((AudioServerPlugInCustomPropertyInfo*)outData)[2].mPropertyDataType = kAudioServerPlugInCustomPropertyDataTypeCFString;
                ((AudioServerPlugInCustomPropertyInfo*)outData)[2].mQualifierDataType = kAudioServerPlugInCustomPropertyDataTypeNone;
            }
            if(theNumberItemsToFetch > 3)
            {
                ((AudioServerPlugInCustomPropertyInfo*)outData)[3].mSelector = kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp;
                ((AudioServerPlugInCustomPropertyInfo*)outData)[3].mPropertyDataType = kAudioServerPlugInCustomPropertyDataTypeCFPropertyList;
                ((AudioServerPlugInCustomPropertyInfo*)outData)[3].mQualifierDataType = kAudioServerPlugInCustomPropertyDataTypeNone;
            }
            if(theNumberItemsToFetch > 4)
            {
                ((AudioServerPlugInCustomPropertyInfo*)outData)[4].mSelector = kAudioDeviceCustomPropertyDeviceAudibleState;
                ((AudioServerPlugInCustomPropertyInfo*)outData)[4].mPropertyDataType = kAudioServerPlugInCustomPropertyDataTypeCFPropertyList;
                ((AudioServerPlugInCustomPropertyInfo*)outData)[4].mQualifierDataType = kAudioServerPlugInCustomPropertyDataTypeNone;
            }
            outDataSize = theNumberItemsToFetch * sizeof(AudioServerPlugInCustomPropertyInfo);
            break;
            
        case kAudioDeviceCustomPropertyDeviceAudibleState:
            {
                ThrowIf(inDataSize < sizeof(CFNumberRef), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDeviceCustomPropertyDeviceAudibleState for the device");
                
                // mDeviceAudibleState is accessed without locking to avoid priority inversions on the IO threads. (The memory barrier
                // is probably unnecessary.)
                CAMemoryBarrier();
                *reinterpret_cast<CFNumberRef*>(outData) = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &mDeviceAudibleState);
                outDataSize = sizeof(CFNumberRef);
            }
            break;
            
        case kAudioDeviceCustomPropertyMusicPlayerProcessID:
            {
                ThrowIf(inDataSize < sizeof(CFNumberRef), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDeviceCustomPropertyMusicPlayerProcessID for the device");
                CAMutex::Locker theStateLocker(mStateMutex);
                pid_t pid = mClients.GetMusicPlayerProcessIDProperty();
                *reinterpret_cast<CFNumberRef*>(outData) = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pid);
                outDataSize = sizeof(CFNumberRef);
            }
            break;
            
        case kAudioDeviceCustomPropertyMusicPlayerBundleID:
            {
                ThrowIf(inDataSize < sizeof(CFStringRef), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDeviceCustomPropertyMusicPlayerBundleID for the device");
                CAMutex::Locker theStateLocker(mStateMutex);
                *reinterpret_cast<CFStringRef*>(outData) = mClients.CopyMusicPlayerBundleIDProperty();
                outDataSize = sizeof(CFStringRef);
            }
            break;
            
        case kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp:
            ThrowIf(inDataSize < sizeof(CFBooleanRef), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp for the device");
            *reinterpret_cast<CFBooleanRef*>(outData) = mClients.ClientsOtherThanBGMAppRunningIO() ? kCFBooleanTrue : kCFBooleanFalse;
            outDataSize = sizeof(CFBooleanRef);
            break;
            
        case kAudioDeviceCustomPropertyAppVolumes:
            {
                ThrowIf(inDataSize < sizeof(CFArrayRef), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_GetPropertyData: not enough space for the return value of kAudioDeviceCustomPropertyAppVolumes for the device");
                CAMutex::Locker theStateLocker(mStateMutex);
                *reinterpret_cast<CFArrayRef*>(outData) = mClients.CopyClientRelativeVolumesAsAppVolumes().GetCFArray();
                outDataSize = sizeof(CFArrayRef);
            }
            break;

		default:
			BGM_Object::GetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
			break;
	};
}

void	BGM_Device::Device_SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData)
{
	switch(inAddress.mSelector)
	{
        case kAudioDevicePropertyNominalSampleRate:
            {
                ThrowIf(inDataSize < sizeof(Float64), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_SetPropertyData: wrong size for the data for kAudioDevicePropertyNominalSampleRate");
                
                // BGMDevice's input and output sample rates are the same, so we just pass the request to Stream_SetPropertyData for one of them
                // TODO: Set the property for both streams just in case that assumption changes? (As unlikely as that is.)
                
                Float64 theNewSampleRate = *reinterpret_cast<const Float64*>(inData);
                
                // Get the stream description so we can just update the sample rate
                AudioObjectPropertyAddress theStreamPropertyAddress = { kAudioStreamPropertyVirtualFormat, inAddress.mScope, inAddress.mElement };
                UInt32 theStreamFormatDataSize;
                AudioStreamBasicDescription theStreamFormat;
                Stream_GetPropertyData(kObjectID_Stream_Input, inClientPID, theStreamPropertyAddress, 0, NULL, sizeof(AudioStreamBasicDescription), theStreamFormatDataSize, &theStreamFormat);
                
                ThrowIf(theStreamFormatDataSize < sizeof(AudioStreamBasicDescription), CAException(kAudioHardwareUnspecifiedError), "BGM_Device::Device_SetPropertyData: too little data returned by Stream_GetPropertyData for kAudioStreamPropertyVirtualFormat");
                
                theStreamFormat.mSampleRate = theNewSampleRate;
                
                Stream_SetPropertyData(kObjectID_Stream_Input, inClientPID, theStreamPropertyAddress, 0, NULL, theStreamFormatDataSize, &theStreamFormat);
            }
            break;
            
        case kAudioDeviceCustomPropertyMusicPlayerProcessID:
            {
                ThrowIf(inDataSize < sizeof(CFNumberRef), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_SetPropertyData: wrong size for the data for kAudioDeviceCustomPropertyMusicPlayerProcessID");
                
                CFNumberRef pidRef = *reinterpret_cast<const CFNumberRef*>(inData);
                
                ThrowIf(pidRef == NULL, CAException(kAudioHardwareIllegalOperationError), "BGM_Device::Device_SetPropertyData: null reference given for kAudioDeviceCustomPropertyMusicPlayerProcessID");
                ThrowIf(CFGetTypeID(pidRef) != CFNumberGetTypeID(), CAException(kAudioHardwareIllegalOperationError), "BGM_Device::Device_SetPropertyData: CFType given for kAudioDeviceCustomPropertyMusicPlayerProcessID was not a CFNumber");
                
                // Get the pid out of the CFNumber we received
                // (Not using CACFNumber::GetSInt32 here because it would return 0 if CFNumberGetValue didn't write to our
                // pid variable, and we want that to be an error.)
                pid_t pid = INT_MIN;
                // CFNumberGetValue docs: "If the conversion is lossy, or the value is out of range, false is returned."
                Boolean success = CFNumberGetValue(pidRef, kCFNumberSInt32Type, &pid);
                
                ThrowIf(!success, CAException(kAudioHardwareIllegalOperationError), "BGM_Device::Device_SetPropertyData: probable error from CFNumberGetValue when reading pid for kAudioDeviceCustomPropertyMusicPlayerProcessID");
                
                CAMutex::Locker theStateLocker(mStateMutex);
                
                bool propertyWasChanged = false;
                
                try
                {
                    propertyWasChanged = mClients.SetMusicPlayer(pid);
                }
                catch(BGM_InvalidClientPIDException)
                {
                    Throw(CAException(kAudioHardwareIllegalOperationError));
                }
                
                if(propertyWasChanged)
                {
                    // Send notification
                    CADispatchQueue::GetGlobalSerialQueue().Dispatch(false,	^{
                        AudioObjectPropertyAddress theChangedProperties[] = { kBGMMusicPlayerProcessIDAddress, kBGMMusicPlayerBundleIDAddress };
                        BGM_PlugIn::Host_PropertiesChanged(inObjectID, 2, theChangedProperties);
                    });
                }
            }
            break;
            
        case kAudioDeviceCustomPropertyMusicPlayerBundleID:
            {
                ThrowIf(inDataSize < sizeof(CFStringRef), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_SetPropertyData: wrong size for the data for kAudioDeviceCustomPropertyMusicPlayerBundleID");
            
                CFStringRef theBundleIDRef = *reinterpret_cast<const CFStringRef*>(inData);
                
                ThrowIfNULL(theBundleIDRef, CAException(kAudioHardwareIllegalOperationError), "BGM_Device::Device_SetPropertyData: kAudioDeviceCustomPropertyMusicPlayerBundleID cannot be set to NULL");
                ThrowIf(CFGetTypeID(theBundleIDRef) != CFStringGetTypeID(), CAException(kAudioHardwareIllegalOperationError), "BGM_Device::Device_SetPropertyData: CFType given for kAudioDeviceCustomPropertyMusicPlayerBundleID was not a CFString");
                
                CAMutex::Locker theStateLocker(mStateMutex);
                
                CFRetain(theBundleIDRef);
                CACFString bundleID(theBundleIDRef);
                
                bool propertyWasChanged = mClients.SetMusicPlayer(bundleID);
                
                if(propertyWasChanged)
                {
                    // Send notification
                    CADispatchQueue::GetGlobalSerialQueue().Dispatch(false,	^{
                        AudioObjectPropertyAddress theChangedProperties[] = { kBGMMusicPlayerBundleIDAddress, kBGMMusicPlayerProcessIDAddress };
                        BGM_PlugIn::Host_PropertiesChanged(inObjectID, 2, theChangedProperties);
                    });
                }
            }
            break;
            
        case kAudioDeviceCustomPropertyAppVolumes:
            {
                CAMutex::Locker theStateLocker(mStateMutex);
                
                ThrowIf(inDataSize < sizeof(CFArrayRef), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Device_SetPropertyData: wrong size for the data for kAudioDeviceCustomPropertyAppVolumes");
                
                CFArrayRef arrayRef = *reinterpret_cast<const CFArrayRef*>(inData);
                
                ThrowIf(CFGetTypeID(arrayRef) != CFArrayGetTypeID(), CAException(kAudioHardwareIllegalOperationError), "BGM_Device::Device_SetPropertyData: CFType given for kAudioDeviceCustomPropertyAppVolumes was not a CFArray");
                
                CACFArray array(arrayRef, false);

                bool propertyWasChanged = false;
                
                try
                {
                    propertyWasChanged = mClients.SetClientsRelativeVolumes(array);
                }
                catch(BGM_InvalidClientRelativeVolumeException)
                {
                    Throw(CAException(kAudioHardwareIllegalOperationError));
                }
                
                if(propertyWasChanged)
                {
                    // Send notification
                    CADispatchQueue::GetGlobalSerialQueue().Dispatch(false,	^{
                        AudioObjectPropertyAddress theChangedProperties[] = { kBGMAppVolumesAddress };
                        BGM_PlugIn::Host_PropertiesChanged(inObjectID, 1, theChangedProperties);
                    });
                }
            }
            break;
            
		default:
			BGM_Object::SetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
			break;
    };
}

#pragma mark Stream Property Operations

bool	BGM_Device::Stream_HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Stream_GetPropertyData() method.
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioStreamPropertyIsActive:
		case kAudioStreamPropertyDirection:
		case kAudioStreamPropertyTerminalType:
		case kAudioStreamPropertyStartingChannel:
		case kAudioStreamPropertyLatency:
		case kAudioStreamPropertyVirtualFormat:
		case kAudioStreamPropertyPhysicalFormat:
		case kAudioStreamPropertyAvailableVirtualFormats:
		case kAudioStreamPropertyAvailablePhysicalFormats:
			theAnswer = true;
			break;
			
		default:
			theAnswer = BGM_Object::HasProperty(inObjectID, inClientPID, inAddress);
			break;
	};
	return theAnswer;
}

bool	BGM_Device::Stream_IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Stream_GetPropertyData() method.
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioStreamPropertyDirection:
		case kAudioStreamPropertyTerminalType:
		case kAudioStreamPropertyStartingChannel:
		case kAudioStreamPropertyLatency:
		case kAudioStreamPropertyAvailableVirtualFormats:
		case kAudioStreamPropertyAvailablePhysicalFormats:
			theAnswer = false;
			break;
		
		case kAudioStreamPropertyIsActive:
		case kAudioStreamPropertyVirtualFormat:
		case kAudioStreamPropertyPhysicalFormat:
			theAnswer = true;
			break;
		
		default:
			theAnswer = BGM_Object::IsPropertySettable(inObjectID, inClientPID, inAddress);
			break;
	};
	return theAnswer;
}

UInt32	BGM_Device::Stream_GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Stream_GetPropertyData() method.
	
	UInt32 theAnswer = 0;
	switch(inAddress.mSelector)
	{
		case kAudioStreamPropertyIsActive:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioStreamPropertyDirection:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioStreamPropertyTerminalType:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioStreamPropertyStartingChannel:
			theAnswer = sizeof(UInt32);
			break;
		
		case kAudioStreamPropertyLatency:
			theAnswer = sizeof(UInt32);
			break;

		case kAudioStreamPropertyVirtualFormat:
		case kAudioStreamPropertyPhysicalFormat:
			theAnswer = sizeof(AudioStreamBasicDescription);
			break;

		case kAudioStreamPropertyAvailableVirtualFormats:
		case kAudioStreamPropertyAvailablePhysicalFormats:
			theAnswer = 1 * sizeof(AudioStreamRangedDescription);
			break;

		default:
			theAnswer = BGM_Object::GetPropertyDataSize(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData);
			break;
	};
	return theAnswer;
}

void	BGM_Device::Stream_GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* outData) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required.
	//	Also, since most of the data that will get returned is static, there are few instances where
	//	it is necessary to lock the state mutex.
	
	UInt32 theNumberItemsToFetch;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyBaseClass:
			//	The base class for kAudioStreamClassID is kAudioObjectClassID
			ThrowIf(inDataSize < sizeof(AudioClassID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Stream_GetPropertyData: not enough space for the return value of kAudioObjectPropertyBaseClass for the stream");
			*reinterpret_cast<AudioClassID*>(outData) = kAudioObjectClassID;
			outDataSize = sizeof(AudioClassID);
			break;
			
		case kAudioObjectPropertyClass:
			//	Streams are of the class, kAudioStreamClassID
			ThrowIf(inDataSize < sizeof(AudioClassID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Stream_GetPropertyData: not enough space for the return value of kAudioObjectPropertyClass for the stream");
			*reinterpret_cast<AudioClassID*>(outData) = kAudioStreamClassID;
			outDataSize = sizeof(AudioClassID);
			break;
			
		case kAudioObjectPropertyOwner:
			//	The stream's owner is the device object
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Stream_GetPropertyData: not enough space for the return value of kAudioObjectPropertyOwner for the stream");
			*reinterpret_cast<AudioObjectID*>(outData) = GetObjectID();
			outDataSize = sizeof(AudioObjectID);
			break;
			
		case kAudioStreamPropertyIsActive:
			//	This property tells the device whether or not the given stream is going to
			//	be used for IO. Note that we need to take the state lock to examine this
			//	value.
			{
				ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Stream_GetPropertyData: not enough space for the return value of kAudioStreamPropertyIsActive for the stream");
				
				//	lock the state mutex
				CAMutex::Locker theStateLocker(mStateMutex);
				
				//	return the requested value
				*reinterpret_cast<UInt32*>(outData) = (inAddress.mScope == kAudioObjectPropertyScopeInput) ? mInputStreamIsActive : mOutputStreamIsActive;
				outDataSize = sizeof(UInt32);
			}
			break;

		case kAudioStreamPropertyDirection:
			//	This returns whether the stream is an input stream or an output stream.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Stream_GetPropertyData: not enough space for the return value of kAudioStreamPropertyDirection for the stream");
			*reinterpret_cast<UInt32*>(outData) = (inObjectID == kObjectID_Stream_Input) ? 1 : 0;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioStreamPropertyTerminalType:
			//	This returns a value that indicates what is at the other end of the stream
			//	such as a speaker or headphones, or a microphone. Values for this property
			//	are defined in <CoreAudio/AudioHardwareBase.h>
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Stream_GetPropertyData: not enough space for the return value of kAudioStreamPropertyTerminalType for the stream");
			*reinterpret_cast<UInt32*>(outData) = (inObjectID == kObjectID_Stream_Input) ? kAudioStreamTerminalTypeMicrophone : kAudioStreamTerminalTypeSpeaker;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioStreamPropertyStartingChannel:
			//	This property returns the absolute channel number for the first channel in
			//	the stream. For example, if a device has two output streams with two
			//	channels each, then the starting channel number for the first stream is 1
			//	and the starting channel number for the second stream is 3.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Stream_GetPropertyData: not enough space for the return value of kAudioStreamPropertyStartingChannel for the stream");
			*reinterpret_cast<UInt32*>(outData) = 1;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioStreamPropertyLatency:
			//	This property returns any additonal presentation latency the stream has.
			ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Stream_GetPropertyData: not enough space for the return value of kAudioStreamPropertyLatency for the stream");
			*reinterpret_cast<UInt32*>(outData) = 0;
			outDataSize = sizeof(UInt32);
			break;

		case kAudioStreamPropertyVirtualFormat:
		case kAudioStreamPropertyPhysicalFormat:
			//	This returns the current format of the stream in an AudioStreamBasicDescription.
			//	For devices that don't override the mix operation, the virtual format has to be the
			//	same as the physical format.
			{
				ThrowIf(inDataSize < sizeof(AudioStreamBasicDescription), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Stream_GetPropertyData: not enough space for the return value of kAudioStreamPropertyVirtualFormat for the stream");
				
				CAMutex::Locker theStateLocker(mStateMutex);
				
				//	This particular device always vends 32-bit native endian floats
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mSampleRate = _HW_GetSampleRate();
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mFormatID = kAudioFormatLinearPCM;
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mBytesPerPacket = 8;
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mFramesPerPacket = 1;
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mBytesPerFrame = 8;
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mChannelsPerFrame = 2;
				reinterpret_cast<AudioStreamBasicDescription*>(outData)->mBitsPerChannel = 32;
				outDataSize = sizeof(AudioStreamBasicDescription);
			}
			break;

		case kAudioStreamPropertyAvailableVirtualFormats:
		case kAudioStreamPropertyAvailablePhysicalFormats:
			//	This returns an array of AudioStreamRangedDescriptions that describe what
			//	formats are supported.

			//	Calculate the number of items that have been requested. Note that this
			//	number is allowed to be smaller than the actual size of the list. In such
			//	case, only that number of items will be returned
			theNumberItemsToFetch = inDataSize / sizeof(AudioStreamRangedDescription);
			
			//	clamp it to the number of items we have
			if(theNumberItemsToFetch > 1)
			{
				theNumberItemsToFetch = 1;
			}
			
			//	fill out the return array
			if(theNumberItemsToFetch > 0)
			{
				((AudioStreamRangedDescription*)outData)[0].mFormat.mSampleRate = kSampleRateDefault;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mFormatID = kAudioFormatLinearPCM;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mBytesPerPacket = 8;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mFramesPerPacket = 1;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mBytesPerFrame = 8;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mChannelsPerFrame = 2;
				((AudioStreamRangedDescription*)outData)[0].mFormat.mBitsPerChannel = 32;
                // These match kAudioDevicePropertyAvailableNominalSampleRates.
				((AudioStreamRangedDescription*)outData)[0].mSampleRateRange.mMinimum = 1.0;
				((AudioStreamRangedDescription*)outData)[0].mSampleRateRange.mMaximum = 1000000000.0;
			}
			
			//	report how much we wrote
			outDataSize = theNumberItemsToFetch * sizeof(AudioStreamRangedDescription);
			break;

		default:
			BGM_Object::GetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
			break;
	};
}

void	BGM_Device::Stream_SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData)
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Stream_GetPropertyData() method.
	
	switch(inAddress.mSelector)
	{
		case kAudioStreamPropertyIsActive:
			{
				//	Changing the active state of a stream doesn't affect IO or change the structure
				//	so we can just save the state and send the notification.
				ThrowIf(inDataSize != sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Stream_SetPropertyData: wrong size for the data for kAudioStreamPropertyIsActive");
				bool theNewIsActive = *reinterpret_cast<const UInt32*>(inData) != 0;
				
				CAMutex::Locker theStateLocker(mStateMutex);
				if(inObjectID == kObjectID_Stream_Input)
				{
					if(mInputStreamIsActive != theNewIsActive)
					{
						mInputStreamIsActive = theNewIsActive;
					}
				}
				else
				{
					if(mOutputStreamIsActive != theNewIsActive)
					{
						mOutputStreamIsActive = theNewIsActive;
					}
				}
			}
			break;
			
		case kAudioStreamPropertyVirtualFormat:
		case kAudioStreamPropertyPhysicalFormat:
			{
				//	Changing the stream format needs to be handled via the
				//	RequestConfigChange/PerformConfigChange machinery. Note that because this
				//	device only supports 2 channel 32 bit float data, the only thing that can
				//	change is the sample rate.
				ThrowIf(inDataSize != sizeof(AudioStreamBasicDescription), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Stream_SetPropertyData: wrong size for the data for kAudioStreamPropertyPhysicalFormat");
				
				const AudioStreamBasicDescription* theNewFormat = reinterpret_cast<const AudioStreamBasicDescription*>(inData);
				ThrowIf(theNewFormat->mFormatID != kAudioFormatLinearPCM, CAException(kAudioDeviceUnsupportedFormatError), "BGM_Device::Stream_SetPropertyData: unsupported format ID for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mFormatFlags != (kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked), CAException(kAudioDeviceUnsupportedFormatError), "BGM_Device::Stream_SetPropertyData: unsupported format flags for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mBytesPerPacket != 8, CAException(kAudioDeviceUnsupportedFormatError), "BGM_Device::Stream_SetPropertyData: unsupported bytes per packet for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mFramesPerPacket != 1, CAException(kAudioDeviceUnsupportedFormatError), "BGM_Device::Stream_SetPropertyData: unsupported frames per packet for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mBytesPerFrame != 8, CAException(kAudioDeviceUnsupportedFormatError), "BGM_Device::Stream_SetPropertyData: unsupported bytes per frame for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mChannelsPerFrame != 2, CAException(kAudioDeviceUnsupportedFormatError), "BGM_Device::Stream_SetPropertyData: unsupported channels per frame for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mBitsPerChannel != 32, CAException(kAudioDeviceUnsupportedFormatError), "BGM_Device::Stream_SetPropertyData: unsupported bits per channel for kAudioStreamPropertyPhysicalFormat");
				ThrowIf(theNewFormat->mSampleRate < 1.0, //(theNewFormat->mSampleRate != 44100.0) && (theNewFormat->mSampleRate != 48000.0),
                        CAException(kAudioDeviceUnsupportedFormatError),
                        "BGM_Device::Stream_SetPropertyData: unsupported sample rate for kAudioStreamPropertyPhysicalFormat");
                
                Float64 theNewSampleRate = *reinterpret_cast<const Float64*>(inData);

                CAMutex::Locker theStateLocker(mStateMutex);
                Float64 theOldSampleRate = _HW_GetSampleRate();
                
				if(theNewSampleRate != theOldSampleRate)
                {
                    mPendingSampleRate = theNewSampleRate;
                    
					//	we dispatch this so that the change can happen asynchronously
                    AudioObjectID theDeviceObjectID = GetObjectID();
					CADispatchQueue::GetGlobalSerialQueue().Dispatch(false,
                                                                     ^{
                                                                         BGM_PlugIn::Host_RequestDeviceConfigurationChange(theDeviceObjectID, 0,  NULL);
                                                                     });
				}
			}
			break;
		
		default:
			BGM_Object::SetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
			break;
	};
}

#pragma mark Control Property Operations

bool	BGM_Device::Control_HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Control_GetPropertyData() method.
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioControlPropertyScope:
		case kAudioControlPropertyElement:
		case kAudioLevelControlPropertyScalarValue:
		case kAudioLevelControlPropertyDecibelValue:
		case kAudioLevelControlPropertyDecibelRange:
		case kAudioLevelControlPropertyConvertScalarToDecibels:
        case kAudioLevelControlPropertyConvertDecibelsToScalar:
        case kAudioBooleanControlPropertyValue:
			theAnswer = true;
			break;
		
		default:
			theAnswer = BGM_Object::HasProperty(inObjectID, inClientPID, inAddress);
			break;
	};
	return theAnswer;
}

bool	BGM_Device::Control_IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Control_GetPropertyData() method.
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioControlPropertyScope:
		case kAudioControlPropertyElement:
		case kAudioLevelControlPropertyDecibelRange:
		case kAudioLevelControlPropertyConvertScalarToDecibels:
		case kAudioLevelControlPropertyConvertDecibelsToScalar:
			theAnswer = false;
			break;
		
		case kAudioLevelControlPropertyScalarValue:
        case kAudioLevelControlPropertyDecibelValue:
        case kAudioBooleanControlPropertyValue:
			theAnswer = true;
			break;
		
		default:
			theAnswer = BGM_Object::IsPropertySettable(inObjectID, inClientPID, inAddress);
			break;
	};
	return theAnswer;
}

UInt32	BGM_Device::Control_GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Control_GetPropertyData() method.
	
	UInt32 theAnswer = 0;
	switch(inAddress.mSelector)
	{
		case kAudioControlPropertyScope:
			theAnswer = sizeof(AudioObjectPropertyScope);
			break;

		case kAudioControlPropertyElement:
			theAnswer = sizeof(AudioObjectPropertyElement);
			break;

		case kAudioLevelControlPropertyScalarValue:
			theAnswer = sizeof(Float32);
			break;

		case kAudioLevelControlPropertyDecibelValue:
			theAnswer = sizeof(Float32);
			break;

		case kAudioLevelControlPropertyDecibelRange:
			theAnswer = sizeof(AudioValueRange);
			break;

		case kAudioLevelControlPropertyConvertScalarToDecibels:
			theAnswer = sizeof(Float32);
			break;

		case kAudioLevelControlPropertyConvertDecibelsToScalar:
			theAnswer = sizeof(Float32);
            break;
            
        case kAudioBooleanControlPropertyValue:
            theAnswer = sizeof(UInt32);
            break;

		default:
			theAnswer = BGM_Object::GetPropertyDataSize(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData);
			break;
	};
	return theAnswer;
}

void	BGM_Device::Control_GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* outData) const
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required.
	//	Also, since most of the data that will get returned is static, there are few instances where
	//	it is necessary to lock the state mutex.
	
	SInt32 theControlRawValue;
	Float32 theVolumeValue;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyBaseClass:
			//	The base classes of kAudioVolumeControlClassID and kAudioMuteControlClassID are kAudioLevelControlClassID and kAudioBooleanControlClassID, respectively
			ThrowIf(inDataSize < sizeof(AudioClassID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_GetPropertyData: not enough space for the return value of kAudioObjectPropertyBaseClass for the volume/mute control");
            *reinterpret_cast<AudioClassID*>(outData) =
                (inObjectID == kObjectID_Mute_Output_Master) ? kAudioBooleanControlClassID : kAudioLevelControlClassID;
			outDataSize = sizeof(AudioClassID);
			break;
			
		case kAudioObjectPropertyClass:
			//	Volume controls are of the class kAudioVolumeControlClassID. Mute controls are of the class kAudioMuteControlClassID.
			ThrowIf(inDataSize < sizeof(AudioClassID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_GetPropertyData: not enough space for the return value of kAudioObjectPropertyClass for the volume/mute control");
            *reinterpret_cast<AudioClassID*>(outData) =
                (inObjectID == kObjectID_Mute_Output_Master) ? kAudioMuteControlClassID : kAudioVolumeControlClassID;
			outDataSize = sizeof(AudioClassID);
			break;
			
		case kAudioObjectPropertyOwner:
			//	The control's owner is the device object
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_GetPropertyData: not enough space for the return value of kAudioObjectPropertyOwner for the volume/mute control");
            *reinterpret_cast<AudioObjectID*>(outData) = GetObjectID();
			outDataSize = sizeof(AudioObjectID);
			break;
			
		case kAudioControlPropertyScope:
			//	This property returns the scope that the control is attached to.
			ThrowIf(inDataSize < sizeof(AudioObjectPropertyScope), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_GetPropertyData: not enough space for the return value of kAudioControlPropertyScope for the volume/mute control");
			*reinterpret_cast<AudioObjectPropertyScope*>(outData) =
                /*(inObjectID == mInputMasterVolumeControlObjectID) ? kAudioObjectPropertyScopeInput :*/ kAudioObjectPropertyScopeOutput;
			outDataSize = sizeof(AudioObjectPropertyScope);
			break;

		case kAudioControlPropertyElement:
			//	This property returns the element that the control is attached to.
			ThrowIf(inDataSize < sizeof(AudioObjectPropertyElement), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_GetPropertyData: not enough space for the return value of kAudioControlPropertyElement for the volume/mute control");
			*reinterpret_cast<AudioObjectPropertyElement*>(outData) = kAudioObjectPropertyElementMaster;
			outDataSize = sizeof(AudioObjectPropertyElement);
			break;

		case kAudioLevelControlPropertyScalarValue:
			//	This returns the value of the control in the normalized range of 0 to 1.
			{
				ThrowIf(inDataSize < sizeof(Float32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_GetPropertyData: not enough space for the return value of kAudioLevelControlPropertyScalarValue for the volume control");
				CAMutex::Locker theStateLocker(mStateMutex);
				theControlRawValue = _HW_GetVolumeControlValue(inObjectID);
				*reinterpret_cast<Float32*>(outData) = mVolumeCurve.ConvertRawToScalar(theControlRawValue);
				outDataSize = sizeof(Float32);
			}
			break;

		case kAudioLevelControlPropertyDecibelValue:
			//	This returns the dB value of the control.
			{
				ThrowIf(inDataSize < sizeof(Float32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_GetPropertyData: not enough space for the return value of kAudioLevelControlPropertyDecibelValue for the volume control");
				CAMutex::Locker theStateLocker(mStateMutex);
				theControlRawValue = _HW_GetVolumeControlValue(inObjectID);
				*reinterpret_cast<Float32*>(outData) = mVolumeCurve.ConvertRawToDB(theControlRawValue);
				outDataSize = sizeof(Float32);
			}
			break;

		case kAudioLevelControlPropertyDecibelRange:
			//	This returns the dB range of the control.
			ThrowIf(inDataSize < sizeof(AudioValueRange), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_GetPropertyData: not enough space for the return value of kAudioLevelControlPropertyDecibelRange for the volume control");
			reinterpret_cast<AudioValueRange*>(outData)->mMinimum = mVolumeCurve.GetMinimumDB();
			reinterpret_cast<AudioValueRange*>(outData)->mMaximum = mVolumeCurve.GetMaximumDB();
			outDataSize = sizeof(AudioValueRange);
			break;

		case kAudioLevelControlPropertyConvertScalarToDecibels:
			//	This takes the scalar value in outData and converts it to dB.
			ThrowIf(inDataSize < sizeof(Float32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_GetPropertyData: not enough space for the return value of kAudioLevelControlPropertyDecibelValue for the volume control");
			
			//	clamp the value to be between 0 and 1
			theVolumeValue = *reinterpret_cast<Float32*>(outData);
			theVolumeValue = std::min(1.0f, std::max(0.0f, theVolumeValue));
			
			//	do the conversion
			*reinterpret_cast<Float32*>(outData) = mVolumeCurve.ConvertScalarToDB(theVolumeValue);
			
			//	report how much we wrote
			outDataSize = sizeof(Float32);
			break;

		case kAudioLevelControlPropertyConvertDecibelsToScalar:
			//	This takes the dB value in outData and converts it to scalar.
			ThrowIf(inDataSize < sizeof(Float32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_GetPropertyData: not enough space for the return value of kAudioLevelControlPropertyDecibelValue for the volume control");
			
			//	clamp the value to be between mOutputMasterMinDbVolumeShadow and mOutputMasterMaxDbVolumeShadow
			theVolumeValue = *reinterpret_cast<Float32*>(outData);
			theVolumeValue = std::min(mOutputMasterMaxDbVolumeShadow, std::max(mOutputMasterMinDbVolumeShadow, theVolumeValue));
			
			//	do the conversion
			*reinterpret_cast<Float32*>(outData) = mVolumeCurve.ConvertDBToScalar(theVolumeValue);
			
			//	report how much we wrote
			outDataSize = sizeof(Float32);
            break;
            
        case kAudioBooleanControlPropertyValue:
            //	This returns the mute value of the control.
            {
                ThrowIf(inDataSize < sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_GetPropertyData: not enough space for the return value of kAudioBooleanControlPropertyValue for the mute control");
                CAMutex::Locker theStateLocker(mStateMutex);
                *reinterpret_cast<UInt32*>(outData) = _HW_GetMuteControlValue(inObjectID);
                outDataSize = sizeof(UInt32);
            }
            break;

		default:
			BGM_Object::GetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
			break;
	};
}

void	BGM_Device::Control_SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData)
{
	//	For each object, this driver implements all the required properties plus a few extras that
	//	are useful but not required. There is more detailed commentary about each property in the
	//	Control_GetPropertyData() method.
	
    bool sendMuteNotification = false;
    bool sendVolumeNotification = false;
	kern_return_t theError = 0;
	Float32 theNewVolumeValue;
	SInt32 theNewRawVolumeValue;
	switch(inAddress.mSelector)
	{
		case kAudioLevelControlPropertyScalarValue:
			//	For the scalar volume, we clamp the new value to [0, 1]. Note that if this
			//	value changes, it implies that the dB value changed too.
			{
				ThrowIf(inDataSize != sizeof(Float32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_SetPropertyData: wrong size for the data for kAudioLevelControlPropertyScalarValue");
				theNewVolumeValue = *((const Float32*)inData);
				theNewVolumeValue = std::min(1.0f, std::max(0.0f, theNewVolumeValue));
				theNewRawVolumeValue = mVolumeCurve.ConvertScalarToRaw(theNewVolumeValue);
				CAMutex::Locker theStateLocker(mStateMutex);
				theError = _HW_SetVolumeControlValue(inObjectID, theNewRawVolumeValue);
                sendVolumeNotification = theError == 0;
			}
			break;
		
		case kAudioLevelControlPropertyDecibelValue:
			//	For the dB value, we first convert it to a scalar value since that is how
			//	the value is tracked. Note that if this value changes, it implies that the
			//	scalar value changes as well.
			{
				ThrowIf(inDataSize != sizeof(Float32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_SetPropertyData: wrong size for the data for kAudioLevelControlPropertyScalarValue");
				theNewVolumeValue = *((const Float32*)inData);
				theNewVolumeValue = std::min(mOutputMasterMaxDbVolumeShadow, std::max(mOutputMasterMinDbVolumeShadow, theNewVolumeValue));
				theNewRawVolumeValue = mVolumeCurve.ConvertDBToRaw(theNewVolumeValue);
				CAMutex::Locker theStateLocker(mStateMutex);
				theError = _HW_SetVolumeControlValue(inObjectID, theNewRawVolumeValue);
                sendVolumeNotification = theError == 0;
            }
            break;
            
        case kAudioBooleanControlPropertyValue:
            {
                ThrowIf(inDataSize != sizeof(UInt32), CAException(kAudioHardwareBadPropertySizeError), "BGM_Device::Control_SetPropertyData: wrong size for the data for kAudioBooleanControlPropertyValue");
                ThrowIf(inObjectID != kObjectID_Mute_Output_Master, CAException(kAudioHardwareBadObjectError), "BGM_Device::Control_SetPropertyData: unexpected control object id for kAudioBooleanControlPropertyValue");
                CAMutex::Locker theStateLocker(mStateMutex);
                theError = _HW_SetMuteControlValue(inObjectID, *((const UInt32*)inData));
                sendMuteNotification = theError == 0;
            }
            break;
		
		default:
			BGM_Object::SetPropertyData(inObjectID, inClientPID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
			break;
    };
    
	if(sendMuteNotification || sendVolumeNotification)
	{
		CADispatchQueue::GetGlobalSerialQueue().Dispatch(false,	^{
            AudioObjectPropertyAddress theChangedProperties[2];
            if(sendMuteNotification)
            {
                theChangedProperties[0] = { kAudioBooleanControlPropertyValue, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
            }
            if(sendVolumeNotification)
            {
                theChangedProperties[0] = { kAudioLevelControlPropertyScalarValue, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
                theChangedProperties[1] = { kAudioLevelControlPropertyDecibelValue, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
            }
            BGM_PlugIn::Host_PropertiesChanged(inObjectID, sendVolumeNotification ? 2 : 1, theChangedProperties);
        });
	}
}

#pragma mark IO Operations

void	BGM_Device::StartIO(UInt32 inClientID)
{
    bool clientIsBGMApp, bgmAppHasClientRegistered;
    
    {
        CAMutex::Locker theStateLocker(mStateMutex);
        
        // An overview of the process this function is part of:
        //   - A client starts IO.
        //   - The plugin host (the HAL) calls the StartIO function in BGM_PlugInInterface, which calls this function.
        //   - BGMDriver sends a message to BGMApp telling it to start the (real) audio hardware.
        //   - BGMApp starts the hardware and, after the hardware is ready, replies to BGMDriver's message.
        //   - BGMDriver lets the host know that it's ready to do IO by returning from StartIO.
        
        // Update our client data.
        //
        // We add the work to the task queue, rather than doing it here, because BeginIOOperation and EndIOOperation
        // also add this task to the queue and the updates should be done in order.
        bool didStartIO = mTaskQueue.QueueSync_StartClientIO(&mClients, inClientID);
        
        // We only tell the hardware to start if this is the first time IO has been started.
        if(didStartIO)
        {
            kern_return_t theError = _HW_StartIO();
            ThrowIfKernelError(theError,
                               CAException(theError),
                               "BGM_Device::StartIO: Failed to start because of an error calling down to the driver.");
        }
        
        clientIsBGMApp = mClients.IsBGMApp(inClientID);
        bgmAppHasClientRegistered = mClients.BGMAppHasClientRegistered();
    }
    
    // We only return from StartIO after BGMApp is ready to pass the audio through to the output device. That way
    // the HAL doesn't start sending us data before BGMApp can play it, which would mean we'd have to either drop
    // frames or increase latency.
    if(!clientIsBGMApp && bgmAppHasClientRegistered)
    {
        UInt64 theXPCError = WaitForBGMAppToStartOutputDevice();
        
        if(theXPCError == kBGMXPC_Success)
        {
            DebugMsg("BGM_Device::StartIO: Ready for IO.");
        }
        else if(theXPCError == kBGMXPC_MessageFailure)
        {
            // This most likely means BGMXPCHelper isn't installed or has crashed. IO will probably still work,
            // but we may drop frames while the audio hardware starts up.
            DebugMsg("BGM_Device::StartIO: Couldn't reach BGMApp via XPC. Attempting to start IO anyway.");
        }
        else
        {
            DebugMsg("BGM_Device::StartIO: BGMApp failed to start the output device. theXPCError=%llu", theXPCError);
            Throw(CAException(kAudioHardwareUnspecifiedError));
        }
    }
}

void	BGM_Device::StopIO(UInt32 inClientID)
{
    CAMutex::Locker theStateLocker(mStateMutex);
    
    // Update our client data.
    //
    // We add the work to the task queue, rather than doing it here, because BeginIOOperation and EndIOOperation also
    // add this task to the queue and the updates should be done in order.
    bool didStopIO = mTaskQueue.QueueSync_StopClientIO(&mClients, inClientID);
	
	//	we tell the hardware to stop if this is the last stop call
	if(didStopIO)
	{
		_HW_StopIO();
	}
}

void	BGM_Device::GetZeroTimeStamp(Float64& outSampleTime, UInt64& outHostTime, UInt64& outSeed)
{
	// accessing the buffers requires holding the IO mutex
	CAMutex::Locker theIOLocker(mIOMutex);
    
    if(mWrappedAudioEngine != NULL)
    {
    }
    else
    {
        // Without a wrapped device, we base our timing on the host. This is mostly from Apple's NullAudio.c sample code
    	UInt64 theCurrentHostTime;
    	Float64 theHostTicksPerRingBuffer;
    	Float64 theHostTickOffset;
    	UInt64 theNextHostTime;
    	
    	//	get the current host time
    	theCurrentHostTime = mach_absolute_time();
    	
    	//	calculate the next host time
    	theHostTicksPerRingBuffer = mLoopbackTime.hostTicksPerFrame * kLoopbackRingBufferFrameSize;
    	theHostTickOffset = static_cast<Float64>(mLoopbackTime.numberTimeStamps + 1) * theHostTicksPerRingBuffer;
    	theNextHostTime = mLoopbackTime.anchorHostTime + static_cast<UInt64>(theHostTickOffset);
    	
    	//	go to the next time if the next host time is less than the current time
    	if(theNextHostTime <= theCurrentHostTime)
    	{
            mLoopbackTime.numberTimeStamps++;
    	}
    	
    	//	set the return values
    	outSampleTime = mLoopbackTime.numberTimeStamps * kLoopbackRingBufferFrameSize;
    	outHostTime = static_cast<UInt64>(mLoopbackTime.anchorHostTime + (static_cast<Float64>(mLoopbackTime.numberTimeStamps) * theHostTicksPerRingBuffer));
        // TODO: I think we should increment outSeed whenever this device switches to/from having a wrapped engine
    	outSeed = 1;
    }
}

void	BGM_Device::WillDoIOOperation(UInt32 inOperationID, bool& outWillDo, bool& outWillDoInPlace) const
{
	switch(inOperationID)
	{
        case kAudioServerPlugInIOOperationThread:
        case kAudioServerPlugInIOOperationReadInput:
        case kAudioServerPlugInIOOperationProcessOutput:
		case kAudioServerPlugInIOOperationWriteMix:
			outWillDo = true;
			outWillDoInPlace = true;
			break;
			
		case kAudioServerPlugInIOOperationCycle:
        case kAudioServerPlugInIOOperationConvertInput:
        case kAudioServerPlugInIOOperationProcessInput:
		case kAudioServerPlugInIOOperationMixOutput:
		case kAudioServerPlugInIOOperationProcessMix:
		case kAudioServerPlugInIOOperationConvertMix:
		default:
			outWillDo = false;
			outWillDoInPlace = true;
			break;
			
	};
}

void	BGM_Device::BeginIOOperation(UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo& inIOCycleInfo, UInt32 inClientID)
{
	#pragma unused(inIOBufferFrameSize, inIOCycleInfo)
    
    if(inOperationID == kAudioServerPlugInIOOperationThread)
    {
        // Update this client's IO state and send notifications if that changes the value of
        // kAudioDeviceCustomPropertyDeviceIsRunning or
        // kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp. We have to do this here
        // as well as in StartIO because the HAL only calls StartIO/StopIO with the first/last clients.
        //
        // We perform the update async because it isn't real-time safe, but we can't just dispatch it with
        // dispatch_async because that isn't real-time safe either. (Apparently even constructing a block
        // isn't.)
        //
        // We don't have to hold the IO mutex here because mTaskQueue and mClients don't change and
        // adding a task to mTaskQueue is thread safe.
        mTaskQueue.QueueAsync_StartClientIO(&mClients, inClientID);
    }
}

void	BGM_Device::DoIOOperation(AudioObjectID inStreamObjectID, UInt32 inClientID, UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo& inIOCycleInfo, void* ioMainBuffer, void* ioSecondaryBuffer)
{
    #pragma unused(inStreamObjectID, ioSecondaryBuffer)
    
	switch(inOperationID)
	{
		case kAudioServerPlugInIOOperationReadInput:
            {
                CAMutex::Locker theIOLocker(mIOMutex);
                ReadInputData(inIOBufferFrameSize, inIOCycleInfo.mInputTime.mSampleTime, ioMainBuffer);
            }
			break;
            
        case kAudioServerPlugInIOOperationProcessOutput:
            {
                CAMutex::Locker theIOLocker(mIOMutex);
                // Called in this IO operation so we can get the music player client's data separately
                UpdateAudibleStateSampleTimes_PreMix(inClientID, inIOBufferFrameSize, inIOCycleInfo.mOutputTime.mSampleTime, ioMainBuffer);
            }
            ApplyClientRelativeVolume(inClientID, inIOBufferFrameSize, ioMainBuffer);
            break;

        case kAudioServerPlugInIOOperationWriteMix:
            {
                CAMutex::Locker theIOLocker(mIOMutex);
                UpdateAudibleStateSampleTimes_PostMix(inIOBufferFrameSize, inIOCycleInfo.mOutputTime.mSampleTime, ioMainBuffer);
                UpdateDeviceAudibleState(inIOBufferFrameSize, inIOCycleInfo.mOutputTime.mSampleTime);
                WriteOutputData(inIOBufferFrameSize, inIOCycleInfo.mOutputTime.mSampleTime, ioMainBuffer);
            }
			break;
	};
}

void	BGM_Device::EndIOOperation(UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo& inIOCycleInfo, UInt32 inClientID)
{
    #pragma unused(inIOBufferFrameSize, inIOCycleInfo)
    if(inOperationID == kAudioServerPlugInIOOperationThread)
    {
        // Tell BGM_Clients that this client has stopped IO. Queued async because we have to be real-time safe here.
        //
        // We don't have to hold the IO mutex here because mTaskQueue and mClients don't change and adding a task to
        // mTaskQueue is thread safe.
        mTaskQueue.QueueAsync_StopClientIO(&mClients, inClientID);
    }
}

void	BGM_Device::ReadInputData(UInt32 inIOBufferFrameSize, Float64 inSampleTime, void* outBuffer)
{
	//	figure out where we are starting
	UInt64 theSampleTime = static_cast<UInt64>(inSampleTime);
	UInt32 theStartFrameOffset = theSampleTime % kLoopbackRingBufferFrameSize;
	
	//	figure out how many frames we need to copy
	UInt32 theNumberFramesToCopy1 = inIOBufferFrameSize;
	UInt32 theNumberFramesToCopy2 = 0;
	if((theStartFrameOffset + theNumberFramesToCopy1) > kLoopbackRingBufferFrameSize)
	{
		theNumberFramesToCopy1 = kLoopbackRingBufferFrameSize - theStartFrameOffset;
		theNumberFramesToCopy2 = inIOBufferFrameSize - theNumberFramesToCopy1;
	}
	
	//	do the copying (the byte sizes here assume a 32 bit stereo sample format)
	Float32* theDestination = reinterpret_cast<Float32*>(outBuffer);
	memcpy(theDestination, mLoopbackRingBuffer + (theStartFrameOffset * 2), theNumberFramesToCopy1 * 8);
	if(theNumberFramesToCopy2 > 0)
	{
		memcpy(theDestination + (theNumberFramesToCopy1 * 2), mLoopbackRingBuffer, theNumberFramesToCopy2 * 8);
    }
    
    //DebugMsg("BGM_Device::ReadInputData: Reading. theSampleTime=%llu theStartFrameOffset=%u theNumberFramesToCopy1=%u theNumberFramesToCopy2=%u", theSampleTime, theStartFrameOffset, theNumberFramesToCopy1, theNumberFramesToCopy2);
}

void	BGM_Device::WriteOutputData(UInt32 inIOBufferFrameSize, Float64 inSampleTime, const void* inBuffer)
{
	//	figure out where we are starting
	UInt64 theSampleTime = static_cast<UInt64>(inSampleTime);
	UInt32 theStartFrameOffset = theSampleTime % kLoopbackRingBufferFrameSize;
	
	//	figure out how many frames we need to copy
	UInt32 theNumberFramesToCopy1 = inIOBufferFrameSize;
	UInt32 theNumberFramesToCopy2 = 0;
	if((theStartFrameOffset + theNumberFramesToCopy1) > kLoopbackRingBufferFrameSize)
	{
		theNumberFramesToCopy1 = kLoopbackRingBufferFrameSize - theStartFrameOffset;
		theNumberFramesToCopy2 = inIOBufferFrameSize - theNumberFramesToCopy1;
	}
	
	//	do the copying (the byte sizes here assume a 32 bit stereo sample format)
	const Float32* theSource = reinterpret_cast<const Float32*>(inBuffer);
	memcpy(mLoopbackRingBuffer + (theStartFrameOffset * 2), theSource, theNumberFramesToCopy1 * 8);
	if(theNumberFramesToCopy2 > 0)
	{
		memcpy(mLoopbackRingBuffer, theSource + (theNumberFramesToCopy1 * 2), theNumberFramesToCopy2 * 8);
    }
    
    //DebugMsg("BGM_Device::WriteOutputData: Writing. theSampleTime=%llu theStartFrameOffset=%u theNumberFramesToCopy1=%u theNumberFramesToCopy2=%u", theSampleTime, theStartFrameOffset, theNumberFramesToCopy1, theNumberFramesToCopy2);
}

void	BGM_Device::ApplyClientRelativeVolume(UInt32 inClientID, UInt32 inIOBufferFrameSize, void* ioBuffer) const
{
    Float32* theBuffer = reinterpret_cast<Float32*>(ioBuffer);
    Float32 theRelativeVolume = mClients.GetClientRelativeVolumeRT(inClientID);
    
    if(theRelativeVolume != 1.)
    {
        for(UInt32 i = 0; i < inIOBufferFrameSize * 2; i++)
        {
            Float32 theAdjustedSample = theBuffer[i] * theRelativeVolume;
            
            // Clamp to [-1.0, 1.0]
            // (This way is roughly 6 times faster than using std::min and std::max because the compiler can vectorize the loop.)
            const Float32 theAdjustedSampleClippedBelow = theAdjustedSample < -1. ? -1. : theAdjustedSample;
            theBuffer[i] = theAdjustedSampleClippedBelow > 1. ? 1. : theAdjustedSampleClippedBelow;

        }
    }
}

bool	BGM_Device::BufferIsAudible(UInt32 inIOBufferFrameSize, const void* inBuffer)
{
    bool audible = false;
    
    // Check each frame to see if any are audible
    for(UInt32 i = 0; i < inIOBufferFrameSize * 2; i++)
    {
        audible = audible || (0. != reinterpret_cast<const Float32*>(inBuffer)[i]);
    }
    
    return audible;
}

void	BGM_Device::UpdateAudibleStateSampleTimes_PreMix(UInt32 inClientID, UInt32 inIOBufferFrameSize, Float64 inOutputSampleTime, const void* inBuffer)
{
    // Updates the sample times of the most recent audible music, silent music and audible non-music samples we've received
    
    Float64 endFrameSampleTime = inOutputSampleTime + inIOBufferFrameSize - 1;
    
    if(mClients.IsMusicPlayerRT(inClientID))
    {
        if(BufferIsAudible(inIOBufferFrameSize, inBuffer))
        {
            mAudibleStateSampleTimes.latestAudibleMusic = std::max(mAudibleStateSampleTimes.latestAudibleMusic, endFrameSampleTime);
        }
        else
        {
            mAudibleStateSampleTimes.latestSilentMusic = std::max(mAudibleStateSampleTimes.latestSilentMusic, endFrameSampleTime);
        }
    }
    else if(endFrameSampleTime > mAudibleStateSampleTimes.latestAudibleNonMusic &&  // Don't bother checking the buffer if it won't change anything
            BufferIsAudible(inIOBufferFrameSize, inBuffer))
    {
        mAudibleStateSampleTimes.latestAudibleNonMusic = std::max(mAudibleStateSampleTimes.latestAudibleNonMusic, endFrameSampleTime);
    }
}

void	BGM_Device::UpdateAudibleStateSampleTimes_PostMix(UInt32 inIOBufferFrameSize, Float64 inOutputSampleTime, const void* inBuffer)
{
    // Updates the sample time of the most recent silent sample we've received. (The music player client is not considered separate
    // for the latest silent sample.)
    
    bool audible = BufferIsAudible(inIOBufferFrameSize, inBuffer);
    Float64 endFrameSampleTime = inOutputSampleTime + inIOBufferFrameSize - 1;
    
    if(!audible)
    {
        mAudibleStateSampleTimes.latestSilent = std::max(mAudibleStateSampleTimes.latestSilent, endFrameSampleTime);
    }
}

void	BGM_Device::UpdateDeviceAudibleState(UInt32 inIOBufferFrameSize, Float64 inOutputSampleTime)
{
    // The sample time of the last frame we're looking at
    Float64 endFrameSampleTime = inOutputSampleTime + inIOBufferFrameSize - 1;
    Float64 sinceLatestSilent = endFrameSampleTime - mAudibleStateSampleTimes.latestSilent;
    Float64 sinceLatestMusicSilent = endFrameSampleTime - mAudibleStateSampleTimes.latestSilentMusic;
    Float64 sinceLatestAudible = endFrameSampleTime - mAudibleStateSampleTimes.latestAudibleNonMusic;
    Float64 sinceLatestMusicAudible = endFrameSampleTime - mAudibleStateSampleTimes.latestAudibleMusic;
    bool sendNotifications = false;
    
    // Update mDeviceAudibleState
    
    // Change from silent/silentExceptMusic to audible
    if(mDeviceAudibleState != kBGMDeviceIsAudible &&
       sinceLatestSilent >= kDeviceAudibleStateMinChangedFramesForUpdate &&
       // Check that non-music audio is currently playing
       sinceLatestAudible <= 0 && mAudibleStateSampleTimes.latestAudibleNonMusic != 0)
    {
        DebugMsg("BGM_Device::UpdateDeviceAudibleState: Changing kAudioDeviceCustomPropertyDeviceAudibleState to audible");
        mDeviceAudibleState = kBGMDeviceIsAudible;
        CAMemoryBarrier();
        sendNotifications = true;
    }
    // Change from silent to silentExceptMusic
    else if(((mDeviceAudibleState == kBGMDeviceIsSilent &&
              sinceLatestMusicSilent >= kDeviceAudibleStateMinChangedFramesForUpdate) ||
                 // ...or from audible to silentExceptMusic
                 (mDeviceAudibleState == kBGMDeviceIsAudible &&
                  sinceLatestAudible >= kDeviceAudibleStateMinChangedFramesForUpdate &&
                  sinceLatestMusicSilent >= kDeviceAudibleStateMinChangedFramesForUpdate)) &&
            // In case we haven't seen any music samples yet (either audible or silent), check that music is currently playing
            sinceLatestMusicAudible <= 0 && mAudibleStateSampleTimes.latestAudibleMusic != 0)
    {
        DebugMsg("BGM_Device::UpdateDeviceAudibleState: Changing kAudioDeviceCustomPropertyDeviceAudibleState to silent except music");
        mDeviceAudibleState = kBGMDeviceIsSilentExceptMusic;
        CAMemoryBarrier();
        sendNotifications = true;
    }
    // Change from audible/silentExceptMusic to silent
    else if(mDeviceAudibleState != kBGMDeviceIsSilent &&
            sinceLatestAudible >= kDeviceAudibleStateMinChangedFramesForUpdate &&
            sinceLatestMusicAudible >= kDeviceAudibleStateMinChangedFramesForUpdate)
    {
        DebugMsg("BGM_Device::UpdateDeviceAudibleState: Changing kAudioDeviceCustomPropertyDeviceAudibleState to silent");
        mDeviceAudibleState = kBGMDeviceIsSilent;
        CAMemoryBarrier();
        sendNotifications = true;
    }

    if(sendNotifications)
    {
        // I'm pretty sure we don't have to use RequestDeviceConfigurationChange for this, but the docs seemed a little unclear to me
        mTaskQueue.QueueAsync_SendPropertyNotification(kAudioDeviceCustomPropertyDeviceAudibleState);
    }
}

#pragma mark Hardware Accessors

// TODO: Out of laziness, some of these hardware functions do more than their names suggest

void	BGM_Device::_HW_Open()
{
}

void	BGM_Device::_HW_Close()
{
}

kern_return_t	BGM_Device::_HW_StartIO()
{
    if(mWrappedAudioEngine != NULL)
    {
    }
    
    // Reset the loopback timing values
    mLoopbackTime.numberTimeStamps = 0;
    mLoopbackTime.anchorHostTime = mach_absolute_time();
    // ...and the most-recent audible/silent sample times
    mAudibleStateSampleTimes.latestSilent = 0;
    mAudibleStateSampleTimes.latestAudibleNonMusic = 0;
    mAudibleStateSampleTimes.latestSilentMusic = 0;
    mAudibleStateSampleTimes.latestAudibleMusic = 0;
    
    return 0;
}

void	BGM_Device::_HW_StopIO()
{
    if(mWrappedAudioEngine != NULL)
    {
    }
}

Float64	BGM_Device::_HW_GetSampleRate() const
{
    return (mWrappedAudioEngine != NULL) ? mWrappedAudioEngine->GetSampleRate() : mLoopbackSampleRate;
}

kern_return_t	BGM_Device::_HW_SetSampleRate(Float64 inNewSampleRate)
{
    kern_return_t theError = 0;
    
    if(mWrappedAudioEngine != NULL)
    {
        theError = mWrappedAudioEngine->SetSampleRate(inNewSampleRate);
    }
    
    mLoopbackSampleRate = inNewSampleRate;
    InitLoopback();
    
    return theError;
}

UInt32	BGM_Device::_HW_GetRingBufferFrameSize() const
{
    return (mWrappedAudioEngine != NULL) ? mWrappedAudioEngine->GetSampleBufferFrameSize() : 0;
}

SInt32	BGM_Device::_HW_GetVolumeControlValue(int inObjectID) const
{
    if(mWrappedAudioEngine != NULL)
    {
    }

	switch(inObjectID)
	{
        //case kObjectID_Volume_Input_Master:
            //return mInputMasterVolumeControlRawValueShadow;
			
        case kObjectID_Volume_Output_Master:
            return mOutputMasterVolumeControlRawValueShadow;
	};
    
    Throw(CAException(kAudioHardwareBadObjectError));
}

kern_return_t	BGM_Device::_HW_SetVolumeControlValue(int inObjectID, SInt32 inNewControlValue)
{
    kern_return_t theError = 0;
    
    if(mWrappedAudioEngine != NULL)
    {
    }
	
	//	if there wasn't an error, the new value was applied, so we need to update the shadow
	if(theError == 0)
	{
		switch(inObjectID)
		{
			//case kSimpleAudioDriver_Control_MasterInputVolume:
            	//	make sure the new value is in the proper range
            	//inNewControlValue = std::min(std::max(kSimpleAudioDriver_Control_MinRawVolumeValue, inNewControlValue), kSimpleAudioDriver_Control_MaxRawVolumeValue);
				//mInputMasterVolumeControlRawValueShadow = inNewControlValue;
				//break;
				
            case kObjectID_Volume_Output_Master:
            	//	make sure the new value is in the proper range
            	inNewControlValue = std::min(std::max(mOutputMasterMinRawVolumeShadow, inNewControlValue), mOutputMasterMaxRawVolumeShadow);
                
				mOutputMasterVolumeControlRawValueShadow = inNewControlValue;
				break;
		};
	}
	
	return theError;
}

UInt32 BGM_Device::_HW_GetMuteControlValue(AudioObjectID inObjectID) const
{
    if(inObjectID == kObjectID_Mute_Output_Master)
    {
        if(mWrappedAudioEngine != NULL)
        {
        }

        return mOutputMuteValueShadow;
    }
    
    Throw(CAException(kAudioHardwareBadObjectError));
}

kern_return_t BGM_Device::_HW_SetMuteControlValue(AudioObjectID inObjectID, UInt32 inValue)
{
    #pragma unused(inObjectID)
    
    kern_return_t theError = 0;
    
    if(mWrappedAudioEngine != NULL)
    {
    }
    
    if(theError == 0)
    {
        mOutputMuteValueShadow = inValue;
    }
    
    return theError;
}

#pragma mark Implementation

void	BGM_Device::AddClient(const AudioServerPlugInClientInfo* inClientInfo)
{
    DebugMsg("BGM_Device::AddClient: Adding client %u (%s)",
             inClientInfo->mClientID,
             (inClientInfo->mBundleID == NULL ?
                 "no bundle ID" :
                 CFStringGetCStringPtr(inClientInfo->mBundleID, kCFStringEncodingUTF8)));
    
    CAMutex::Locker theStateLocker(mStateMutex);

    mClients.AddClient(inClientInfo);
}

void	BGM_Device::RemoveClient(const AudioServerPlugInClientInfo* inClientInfo)
{
    DebugMsg("BGM_Device::RemoveClient: Removing client %u (%s)",
             inClientInfo->mClientID,
             CFStringGetCStringPtr(inClientInfo->mBundleID, kCFStringEncodingUTF8));
    
    CAMutex::Locker theStateLocker(mStateMutex);

    mClients.RemoveClient(inClientInfo->mClientID);
}

void	BGM_Device::PerformConfigChange(UInt64 inChangeAction, void* inChangeInfo)
{
	#pragma unused(inChangeAction, inChangeInfo)
	
	// This device only supports changing the sample rate
	
    //	we need to lock the state lock around telling the hardware about the new sample rate
    CAMutex::Locker theStateLocker(mStateMutex);
    _HW_SetSampleRate(mPendingSampleRate);
}

void	BGM_Device::AbortConfigChange(UInt64 inChangeAction, void* inChangeInfo)
{
	#pragma unused(inChangeAction, inChangeInfo)
	
	//	this device doesn't need to do anything special if a change request gets aborted
}


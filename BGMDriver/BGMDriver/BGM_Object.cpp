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
//  BGM_Object.cpp
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//  Portions copyright (C) 2013 Apple Inc. All Rights Reserved.
//
//  Based largely on SA_Object.cpp from Apple's SimpleAudioDriver Plug-In sample code.
//  https://developer.apple.com/library/mac/samplecode/AudioDriverExamples
//
//  Similarly to BGM_Object.h, this file hasn't been changed much from SA_Object.cpp, except to
//  remove the SA_ObjectMap class.
//

//	Self Include
#include "BGM_Object.h"

//	PublicUtility Includes
#include "CADebugMacros.h"
#include "CAException.h"


//==================================================================================================
#pragma mark -
#pragma mark BGM_Object
//==================================================================================================

#pragma mark Construction/Destruction

BGM_Object::BGM_Object(AudioObjectID inObjectID, AudioClassID inClassID, AudioClassID inBaseClassID, AudioObjectID inOwnerObjectID)
:
	mObjectID(inObjectID),
	mClassID(inClassID),
	mBaseClassID(inBaseClassID),
	mOwnerObjectID(inOwnerObjectID),
	mIsActive(false)
{
}

void	BGM_Object::Activate()
{
	mIsActive = true;
}

void	BGM_Object::Deactivate()
{
	mIsActive = false;
}

BGM_Object::~BGM_Object()
{
}

#pragma mark Property Operations

bool	BGM_Object::HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	#pragma unused(inObjectID, inClientPID)
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyBaseClass:
		case kAudioObjectPropertyClass:
		case kAudioObjectPropertyOwner:
		case kAudioObjectPropertyOwnedObjects:
			theAnswer = true;
			break;
	};
	return theAnswer;
}

bool	BGM_Object::IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const
{
	#pragma unused(inObjectID, inClientPID)
	
	bool theAnswer = false;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyBaseClass:
		case kAudioObjectPropertyClass:
		case kAudioObjectPropertyOwner:
		case kAudioObjectPropertyOwnedObjects:
			theAnswer = false;
			break;
		
		default:
			Throw(CAException(kAudioHardwareUnknownPropertyError));
	};
	return theAnswer;
}

UInt32	BGM_Object::GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const
{
	#pragma unused(inObjectID, inClientPID, inQualifierDataSize, inQualifierData)
	
	UInt32 theAnswer = 0;
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyBaseClass:
		case kAudioObjectPropertyClass:
			theAnswer = sizeof(AudioClassID);
			break;
			
		case kAudioObjectPropertyOwner:
			theAnswer = sizeof(AudioObjectID);
			break;
			
		case kAudioObjectPropertyOwnedObjects:
			theAnswer = 0;
			break;
		
		default:
			Throw(CAException(kAudioHardwareUnknownPropertyError));
	};
	return theAnswer;
}

void	BGM_Object::GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* outData) const
{
	#pragma unused(inObjectID, inClientPID, inQualifierDataSize, inQualifierData)
	
	switch(inAddress.mSelector)
	{
		case kAudioObjectPropertyBaseClass:
			//	This is the AudioClassID of the base class of this object. This is an invariant.
			ThrowIf(inDataSize < sizeof(AudioClassID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Object::GetPropertyData: not enough space for the return value of kAudioObjectPropertyBaseClass");
			*reinterpret_cast<AudioClassID*>(outData) = mBaseClassID;
			outDataSize = sizeof(AudioClassID);
			break;
			
		case kAudioObjectPropertyClass:
			//	This is the AudioClassID of the class of this object. This is an invariant.
			ThrowIf(inDataSize < sizeof(AudioClassID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Object::GetPropertyData: not enough space for the return value of kAudioObjectPropertyClass");
			*reinterpret_cast<AudioClassID*>(outData) = mClassID;
			outDataSize = sizeof(AudioClassID);
			break;
			
		case kAudioObjectPropertyOwner:
			//	The AudioObjectID of the object that owns this object. This is an invariant.
			ThrowIf(inDataSize < sizeof(AudioObjectID), CAException(kAudioHardwareBadPropertySizeError), "BGM_Object::GetPropertyData: not enough space for the return value of kAudioObjectPropertyOwner");
			*reinterpret_cast<AudioClassID*>(outData) = mOwnerObjectID;
			outDataSize = sizeof(AudioObjectID);
			break;
			
		case kAudioObjectPropertyOwnedObjects:
			//	This is an array of AudioObjectIDs for the objects owned by this object. By default,
			//	objects don't own any other objects. This is an invariant by default, but an object
			//	that can contain other objects will likely need to do some synchronization to access
			//	this property.
			outDataSize = 0;
			break;
		
		default:
			Throw(CAException(kAudioHardwareUnknownPropertyError));
	};
}

void	BGM_Object::SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData)
{
	#pragma unused(inObjectID, inClientPID, inQualifierDataSize, inQualifierData, inDataSize, inData)
	
	switch(inAddress.mSelector)
	{
		default:
			Throw(CAException(kAudioHardwareUnknownPropertyError));
	};
}


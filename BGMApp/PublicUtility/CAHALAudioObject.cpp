/*
     File: CAHALAudioObject.cpp
 Abstract: CAHALAudioObject.h
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
//==================================================================================================
//	Includes
//==================================================================================================

//	Self Include
#include "CAHALAudioObject.h"

//	PublicUtility Includes
#include "CAAutoDisposer.h"
#include "CADebugMacros.h"
#include "CAException.h"
#include "CAPropertyAddress.h"

//==================================================================================================
//	CAHALAudioObject
//==================================================================================================

CAHALAudioObject::CAHALAudioObject(AudioObjectID inObjectID)
:
	mObjectID(inObjectID)
{
}

CAHALAudioObject::~CAHALAudioObject()
{
}

AudioObjectID	CAHALAudioObject::GetObjectID() const
{
	return mObjectID;
}

void	CAHALAudioObject::SetObjectID(AudioObjectID inObjectID)
{
	mObjectID = inObjectID;
}

AudioClassID	CAHALAudioObject::GetClassID() const
{
	//	set up the return value
	AudioClassID theAnswer = 0;
	
	//	set up the property address
	CAPropertyAddress theAddress(kAudioObjectPropertyClass);
	
	//	make sure the property exists
	if(HasProperty(theAddress))
	{
		UInt32 theSize = sizeof(AudioClassID);
		GetPropertyData(theAddress, 0, NULL, theSize, &theAnswer);
	}
	
	return theAnswer;
}

AudioObjectID	CAHALAudioObject::GetOwnerObjectID() const
{
	//	set up the return value
	AudioObjectID theAnswer = 0;
	
	//	set up the property address
	CAPropertyAddress theAddress(kAudioObjectPropertyOwner);
	
	//	make sure the property exists
	if(HasProperty(theAddress))
	{
		//	get the property data
		UInt32 theSize = sizeof(AudioObjectID);
		GetPropertyData(theAddress, 0, NULL, theSize, &theAnswer);
	}
	
	return theAnswer;
}

CFStringRef	CAHALAudioObject::CopyOwningPlugInBundleID() const
{
	//	set up the return value
	CFStringRef theAnswer = NULL;
	
	//	set up the property address
	CAPropertyAddress theAddress(kAudioObjectPropertyCreator);
	
	//	make sure the property exists
	if(HasProperty(theAddress))
	{
		//	get the property data
		UInt32 theSize = sizeof(CFStringRef);
		GetPropertyData(theAddress, 0, NULL, theSize, &theAnswer);
	}
	
	return theAnswer;
}

CFStringRef	CAHALAudioObject::CopyName() const
{
	//	set up the return value
	CFStringRef theAnswer = NULL;
	
	//	set up the property address
	CAPropertyAddress theAddress(kAudioObjectPropertyName);
	
	//	make sure the property exists
	if(HasProperty(theAddress))
	{
		//	get the property data
		UInt32 theSize = sizeof(CFStringRef);
		GetPropertyData(theAddress, 0, NULL, theSize, &theAnswer);
	}
	
	return theAnswer;
}

CFStringRef	CAHALAudioObject::CopyManufacturer() const
{
	//	set up the return value
	CFStringRef theAnswer = NULL;
	
	//	set up the property address
	CAPropertyAddress theAddress(kAudioObjectPropertyManufacturer);
	
	//	make sure the property exists
	if(HasProperty(theAddress))
	{
		//	get the property data
		UInt32 theSize = sizeof(CFStringRef);
		GetPropertyData(theAddress, 0, NULL, theSize, &theAnswer);
	}
	
	return theAnswer;
}

CFStringRef	CAHALAudioObject::CopyNameForElement(AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement) const
{
	//	set up the return value
	CFStringRef theAnswer = NULL;
	
	//	set up the property address
	CAPropertyAddress theAddress(kAudioObjectPropertyElementName, inScope, inElement);
	
	//	make sure the property exists
	if(HasProperty(theAddress))
	{
		//	get the property data
		UInt32 theSize = sizeof(CFStringRef);
		GetPropertyData(theAddress, 0, NULL, theSize, &theAnswer);
	}
	
	return theAnswer;
}

CFStringRef	CAHALAudioObject::CopyCategoryNameForElement(AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement) const
{
	//	set up the return value
	CFStringRef theAnswer = NULL;
	
	//	set up the property address
	CAPropertyAddress theAddress(kAudioObjectPropertyElementCategoryName, inScope, inElement);
	
	//	make sure the property exists
	if(HasProperty(theAddress))
	{
		//	get the property data
		UInt32 theSize = sizeof(CFStringRef);
		GetPropertyData(theAddress, 0, NULL, theSize, &theAnswer);
	}
	
	return theAnswer;
}

CFStringRef	CAHALAudioObject::CopyNumberNameForElement(AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement) const
{
	//	set up the return value
	CFStringRef theAnswer = NULL;
	
	//	set up the property address
	CAPropertyAddress theAddress(kAudioObjectPropertyElementNumberName, inScope, inElement);
	
	//	make sure the property exists
	if(HasProperty(theAddress))
	{
		//	get the property data
		UInt32 theSize = sizeof(CFStringRef);
		GetPropertyData(theAddress, 0, NULL, theSize, &theAnswer);
	}
	
	return theAnswer;
}

bool	CAHALAudioObject::ObjectExists(AudioObjectID inObjectID)
{
	Boolean isSettable;
	CAPropertyAddress theAddress(kAudioObjectPropertyClass);
    // BGM edit: Negated the expression returned. Seems to have been a bug.
    //return (inObjectID == 0) || (AudioObjectIsPropertySettable(inObjectID, &theAddress, &isSettable) != 0);
    return (inObjectID != kAudioObjectUnknown) && (AudioObjectIsPropertySettable(inObjectID, &theAddress, &isSettable) == kAudioHardwareNoError);
    // BGM edit end
}

UInt32	CAHALAudioObject::GetNumberOwnedObjects(AudioClassID inClass) const
{
	//	set up the return value
	UInt32 theAnswer = 0;
	
	//	set up the property address
	CAPropertyAddress theAddress(kAudioObjectPropertyOwnedObjects);
	
	//	figure out the qualifier
	UInt32 theQualifierSize = 0;
	void* theQualifierData = NULL;
	if(inClass != 0)
	{
		theQualifierSize = sizeof(AudioObjectID);
		theQualifierData = &inClass;
	}
	
	//	get the property data size
	theAnswer = GetPropertyDataSize(theAddress, theQualifierSize, theQualifierData);
	
	//	calculate the number of object IDs
	theAnswer /= SizeOf32(AudioObjectID);
	
	return theAnswer;
}

void	CAHALAudioObject::GetAllOwnedObjects(AudioClassID inClass, UInt32& ioNumberObjects, AudioObjectID* ioObjectIDs) const
{
	//	set up the property address
	CAPropertyAddress theAddress(kAudioObjectPropertyOwnedObjects);
	
	//	figure out the qualifier
	UInt32 theQualifierSize = 0;
	void* theQualifierData = NULL;
	if(inClass != 0)
	{
		theQualifierSize = sizeof(AudioObjectID);
		theQualifierData = &inClass;
	}
	
	//	get the property data
	UInt32 theDataSize = ioNumberObjects * SizeOf32(AudioClassID);
	GetPropertyData(theAddress, theQualifierSize, theQualifierData, theDataSize, ioObjectIDs);
	
	//	set the number of object IDs being returned
	ioNumberObjects = theDataSize / SizeOf32(AudioObjectID);
}

AudioObjectID	CAHALAudioObject::GetOwnedObjectByIndex(AudioClassID inClass, UInt32 inIndex)
{
	//	set up the property address
	CAPropertyAddress theAddress(kAudioObjectPropertyOwnedObjects);
	
	//	figure out the qualifier
	UInt32 theQualifierSize = 0;
	void* theQualifierData = NULL;
	if(inClass != 0)
	{
		theQualifierSize = sizeof(AudioObjectID);
		theQualifierData = &inClass;
	}
	
	//	figure out how much space to allocate
	UInt32 theDataSize = GetPropertyDataSize(theAddress, theQualifierSize, theQualifierData);
	UInt32 theNumberObjectIDs = theDataSize / SizeOf32(AudioObjectID);
	
	//	set up the return value
	AudioObjectID theAnswer = 0;
	
	//	maker sure the index is in range
	if(inIndex < theNumberObjectIDs)
	{
		//	allocate it
		CAAutoArrayDelete<AudioObjectID> theObjectList(theDataSize / sizeof(AudioObjectID));
		
		//	get the property data
		GetPropertyData(theAddress, theQualifierSize, theQualifierData, theDataSize, theObjectList);
		
		//	get the return value
		theAnswer = theObjectList[inIndex];
	}
	
	return theAnswer;
}

bool	CAHALAudioObject::HasProperty(const AudioObjectPropertyAddress& inAddress) const
{
	return AudioObjectHasProperty(mObjectID, &inAddress);
}

bool	CAHALAudioObject::IsPropertySettable(const AudioObjectPropertyAddress& inAddress) const
{
	Boolean isSettable = false;
	OSStatus theError = AudioObjectIsPropertySettable(mObjectID, &inAddress, &isSettable);
	ThrowIfError(theError, CAException(theError), "CAHALAudioObject::IsPropertySettable: got an error getting info about a property");
	return isSettable != 0;
}

UInt32	CAHALAudioObject::GetPropertyDataSize(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const
{
	UInt32 theDataSize = 0;
	OSStatus theError = AudioObjectGetPropertyDataSize(mObjectID, &inAddress, inQualifierDataSize, inQualifierData, &theDataSize);
	ThrowIfError(theError, CAException(theError), "CAHALAudioObject::GetPropertyDataSize: got an error getting the property data size");
	return theDataSize;
}

void	CAHALAudioObject::GetPropertyData(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32& ioDataSize, void* outData) const
{
	OSStatus theError = AudioObjectGetPropertyData(mObjectID, &inAddress, inQualifierDataSize, inQualifierData, &ioDataSize, outData);
	ThrowIfError(theError, CAException(theError), "CAHALAudioObject::GetPropertyData: got an error getting the property data");
}

void	CAHALAudioObject::SetPropertyData(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData)
{
	OSStatus theError = AudioObjectSetPropertyData(mObjectID, &inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
	ThrowIfError(theError, CAException(theError), "CAHALAudioObject::SetPropertyData: got an error setting the property data");
}

void	CAHALAudioObject::AddPropertyListener(const AudioObjectPropertyAddress& inAddress, AudioObjectPropertyListenerProc inListenerProc, void* inClientData)
{
	OSStatus theError = AudioObjectAddPropertyListener(mObjectID, &inAddress, inListenerProc, inClientData);
	ThrowIfError(theError, CAException(theError), "CAHALAudioObject::AddPropertyListener: got an error adding a property listener");
}

void	CAHALAudioObject::RemovePropertyListener(const AudioObjectPropertyAddress& inAddress, AudioObjectPropertyListenerProc inListenerProc, void* inClientData)
{
	OSStatus theError = AudioObjectRemovePropertyListener(mObjectID, &inAddress, inListenerProc, inClientData);
	ThrowIfError(theError, CAException(theError), "CAHALAudioObject::RemovePropertyListener: got an error removing a property listener");
}

/*
     File: CAHALAudioObject.h
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
#if !defined(__CAHALAudioObject_h__)
#define __CAHALAudioObject_h__

//==================================================================================================
//	Includes
//==================================================================================================

//	PublicUtility Includes
#include "CADebugMacros.h"
#include "CAException.h"

//	System Includes
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreAudio/CoreAudio.h>
	#include <CoreFoundation/CoreFoundation.h>
#else
	#include <CoreAudio.h>
	#include <CoreFoundation.h>
#endif

//==================================================================================================
//	CAHALAudioObject
//==================================================================================================

class CAHALAudioObject
{

//	Construction/Destruction
public:
								CAHALAudioObject(AudioObjectID inObjectID);
	virtual						~CAHALAudioObject();

//	Attributes
public:
	AudioObjectID				GetObjectID() const;
	void						SetObjectID(AudioObjectID inObjectID);
	AudioClassID				GetClassID() const;
	AudioObjectID				GetOwnerObjectID() const;
	CFStringRef					CopyOwningPlugInBundleID() const;
	CFStringRef					CopyName() const;
	CFStringRef					CopyManufacturer() const;
	CFStringRef					CopyNameForElement(AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement) const;
	CFStringRef					CopyCategoryNameForElement(AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement) const;
	CFStringRef					CopyNumberNameForElement(AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement) const;
	
	static bool					ObjectExists(AudioObjectID inObjectID);

//	Owned Objects
public:
	UInt32						GetNumberOwnedObjects(AudioClassID inClass) const;
	void						GetAllOwnedObjects(AudioClassID inClass, UInt32& ioNumberObjects, AudioObjectID* ioObjectIDs) const;
	AudioObjectID				GetOwnedObjectByIndex(AudioClassID inClass, UInt32 inIndex);
	
//	Property Operations
public:
	bool						HasProperty(const AudioObjectPropertyAddress& inAddress) const;
	bool						IsPropertySettable(const AudioObjectPropertyAddress& inAddress) const;
	UInt32						GetPropertyDataSize(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const;
	
	void						GetPropertyData(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32& ioDataSize, void* outData) const;
	void						SetPropertyData(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData);
	
	UInt32						GetPropertyData_UInt32(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL) const										{ UInt32 theAnswer = 0; UInt32 theDataSize = SizeOf32(UInt32); GetPropertyData(inAddress, inQualifierDataSize, inQualifierData, theDataSize, &theAnswer); return theAnswer; }
	void						SetPropertyData_UInt32(const AudioObjectPropertyAddress& inAddress, UInt32 inValue, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL)								{ SetPropertyData(inAddress, inQualifierDataSize, inQualifierData, SizeOf32(UInt32), &inValue); }

	Float32						GetPropertyData_Float32(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL) const										{ Float32 theAnswer = 0; UInt32 theDataSize = SizeOf32(Float32); GetPropertyData(inAddress, inQualifierDataSize, inQualifierData, theDataSize, &theAnswer); return theAnswer; }
	void						SetPropertyData_Float32(const AudioObjectPropertyAddress& inAddress, Float32 inValue, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL)							{ SetPropertyData(inAddress, inQualifierDataSize, inQualifierData, SizeOf32(Float32), &inValue); }

	Float64						GetPropertyData_Float64(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL) const										{ Float64 theAnswer = 0; UInt32 theDataSize = SizeOf32(Float64); GetPropertyData(inAddress, inQualifierDataSize, inQualifierData, theDataSize, &theAnswer); return theAnswer; }
	void						SetPropertyData_Float64(const AudioObjectPropertyAddress& inAddress, Float64 inValue, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL)							{ SetPropertyData(inAddress, inQualifierDataSize, inQualifierData, SizeOf32(Float64), &inValue); }

	CFTypeRef					GetPropertyData_CFType(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL) const										{ CFTypeRef theAnswer = NULL; UInt32 theDataSize = SizeOf32(CFTypeRef); GetPropertyData(inAddress, inQualifierDataSize, inQualifierData, theDataSize, &theAnswer); return theAnswer; }
	void						SetPropertyData_CFType(const AudioObjectPropertyAddress& inAddress, CFTypeRef inValue, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL)								{ SetPropertyData(inAddress, inQualifierDataSize, inQualifierData, SizeOf32(CFTypeRef), &inValue); }

	CFStringRef					GetPropertyData_CFString(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL) const										{ CFStringRef theAnswer = NULL; UInt32 theDataSize = SizeOf32(CFStringRef); GetPropertyData(inAddress, inQualifierDataSize, inQualifierData, theDataSize, &theAnswer); return theAnswer; }
	void						SetPropertyData_CFString(const AudioObjectPropertyAddress& inAddress, CFStringRef inValue, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL)							{ SetPropertyData(inAddress, inQualifierDataSize, inQualifierData, SizeOf32(CFStringRef), &inValue); }

	template <class T> void		GetPropertyData_Struct(const AudioObjectPropertyAddress& inAddress, T& outStruct, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL) const							{ UInt32 theDataSize = SizeOf32(T); GetPropertyData(inAddress, inQualifierDataSize, inQualifierData, theDataSize, &outStruct); }
	template <class T> void		SetPropertyData_Struct(const AudioObjectPropertyAddress& inAddress, T& inStruct, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL)								{ SetPropertyData(inAddress, inQualifierDataSize, inQualifierData, SizeOf32(T), &inStruct); }

	template <class T> UInt32	GetPropertyData_ArraySize(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL) const									{ return GetPropertyDataSize(inAddress, inQualifierDataSize, inQualifierData) / SizeOf32(T); }
	template <class T> void		GetPropertyData_Array(const AudioObjectPropertyAddress& inAddress, UInt32& ioNumberItems, T* outArray, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL) const	{ UInt32 theDataSize = ioNumberItems * SizeOf32(T); GetPropertyData(inAddress, inQualifierDataSize, inQualifierData, theDataSize, outArray); ioNumberItems = theDataSize / SizeOf32(T); }
	template <class T> void		SetPropertyData_Array(const AudioObjectPropertyAddress& inAddress, UInt32 inNumberItems, T* inArray, UInt32 inQualifierDataSize = 0, const void* inQualifierData = NULL)			{ SetPropertyData(inAddress, inQualifierDataSize, inQualifierData, inNumberItems * SizeOf32(T), inArray); }
	
	void						AddPropertyListener(const AudioObjectPropertyAddress& inAddress, AudioObjectPropertyListenerProc inListenerProc, void* inClientData);
	void						RemovePropertyListener(const AudioObjectPropertyAddress& inAddress, AudioObjectPropertyListenerProc inListenerProc, void* inClientData);

	void						AddPropertyListenerBlock(const AudioObjectPropertyAddress& inAddress, dispatch_queue_t inDispatchQueue, AudioObjectPropertyListenerBlock inListenerBlock);
	void						RemovePropertyListenerBlock(const AudioObjectPropertyAddress& inAddress, dispatch_queue_t inDispatchQueue, AudioObjectPropertyListenerBlock inListenerBlock);

//	Implementation
protected:
	AudioObjectID				mObjectID;

};

inline void	CAHALAudioObject::AddPropertyListenerBlock(const AudioObjectPropertyAddress& inAddress, dispatch_queue_t inDispatchQueue, AudioObjectPropertyListenerBlock inListenerBlock)
{
	OSStatus theError = AudioObjectAddPropertyListenerBlock(mObjectID, &inAddress, inDispatchQueue, inListenerBlock);
	ThrowIfError(theError, CAException(theError), "CAHALAudioObject::AddPropertyListenerBlock: got an error adding a property listener");
}

inline void	CAHALAudioObject::RemovePropertyListenerBlock(const AudioObjectPropertyAddress& inAddress, dispatch_queue_t inDispatchQueue, AudioObjectPropertyListenerBlock inListenerBlock)
{
	OSStatus theError = AudioObjectRemovePropertyListenerBlock(mObjectID, &inAddress, inDispatchQueue, inListenerBlock);
	ThrowIfError(theError, CAException(theError), "CAHALAudioObject::RemovePropertyListener: got an error removing a property listener");
}

#endif

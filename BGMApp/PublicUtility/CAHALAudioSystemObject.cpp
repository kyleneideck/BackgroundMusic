/*
     File: CAHALAudioSystemObject.cpp
 Abstract: CAHALAudioSystemObject.h
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
#include "CAHALAudioSystemObject.h"

//	PublicUtility Includes
#include "CAAutoDisposer.h"
#include "CACFString.h"
#include "CAHALAudioDevice.h"
#include "CAPropertyAddress.h"

//==================================================================================================
//	CAHALAudioSystemObject
//==================================================================================================

CAHALAudioSystemObject::CAHALAudioSystemObject()
:
	CAHALAudioObject(kAudioObjectSystemObject)
{
}

CAHALAudioSystemObject::~CAHALAudioSystemObject()
{
}

UInt32	CAHALAudioSystemObject::GetNumberAudioDevices() const
{
	CAPropertyAddress theAddress(kAudioHardwarePropertyDevices);
	UInt32 theAnswer = GetPropertyDataSize(theAddress, 0, NULL);
	theAnswer /= SizeOf32(AudioObjectID);
	return theAnswer;
}

void	CAHALAudioSystemObject::GetAudioDevices(UInt32& ioNumberAudioDevices, AudioObjectID* outAudioDevices) const
{
	CAPropertyAddress theAddress(kAudioHardwarePropertyDevices);
	UInt32 theSize = ioNumberAudioDevices * SizeOf32(AudioObjectID);
	GetPropertyData(theAddress, 0, NULL, theSize, outAudioDevices);
	ioNumberAudioDevices = theSize / SizeOf32(AudioObjectID);
}

AudioObjectID	CAHALAudioSystemObject::GetAudioDeviceAtIndex(UInt32 inIndex) const
{
	AudioObjectID theAnswer = kAudioObjectUnknown;
	UInt32 theNumberDevices = GetNumberAudioDevices();
	if((theNumberDevices > 0) && (inIndex < theNumberDevices))
	{
		CAAutoArrayDelete<AudioObjectID> theDeviceList(theNumberDevices);
		GetAudioDevices(theNumberDevices, theDeviceList);
		if((theNumberDevices > 0) && (inIndex < theNumberDevices))
		{
			theAnswer = theDeviceList[inIndex];
		}
	}
	return theAnswer;
}

AudioObjectID	CAHALAudioSystemObject::GetAudioDeviceForUID(CFStringRef inUID) const
{
	AudioObjectID theAnswer = kAudioObjectUnknown;
	AudioValueTranslation theValue = { &inUID, sizeof(CFStringRef), &theAnswer, sizeof(AudioObjectID) };
	CAPropertyAddress theAddress(kAudioHardwarePropertyDeviceForUID);
	UInt32 theSize = sizeof(AudioValueTranslation);
	GetPropertyData(theAddress, 0, NULL, theSize, &theValue);
	return theAnswer;
}

void	CAHALAudioSystemObject::LogBasicDeviceInfo()
{
	UInt32 theNumberDevices = GetNumberAudioDevices();
	CAAutoArrayDelete<AudioObjectID> theDeviceList(theNumberDevices);
	GetAudioDevices(theNumberDevices, theDeviceList);
	DebugMessageN1("CAHALAudioSystemObject::LogBasicDeviceInfo: %d devices", (int)theNumberDevices);
	for(UInt32 theDeviceIndex = 0; theDeviceIndex < theNumberDevices; ++theDeviceIndex)
	{
		char theCString[256];
		UInt32 theCStringSize = sizeof(theCString);
		DebugMessageN1("CAHALAudioSystemObject::LogBasicDeviceInfo: Device %d", (int)theDeviceIndex);
		
		CAHALAudioDevice theDevice(theDeviceList[theDeviceIndex]);
		DebugMessageN1("CAHALAudioSystemObject::LogBasicDeviceInfo:   Object ID: %d", (int)theDeviceList[theDeviceIndex]);
		
		CACFString theDeviceName(theDevice.CopyName());
		theCStringSize = sizeof(theCString);
		theDeviceName.GetCString(theCString, theCStringSize);
		DebugMessageN1("CAHALAudioSystemObject::LogBasicDeviceInfo:   Name:      %s", theCString);
		
		CACFString theDeviceUID(theDevice.CopyDeviceUID());
		theCStringSize = sizeof(theCString);
		theDeviceUID.GetCString(theCString, theCStringSize);
		DebugMessageN1("CAHALAudioSystemObject::LogBasicDeviceInfo:   UID:       %s", theCString);
	}
}

static inline AudioObjectPropertySelector	CAHALAudioSystemObject_CalculateDefaultDeviceProperySelector(bool inIsInput, bool inIsSystem)
{
	AudioObjectPropertySelector theAnswer = kAudioHardwarePropertyDefaultOutputDevice;
	if(inIsInput)
	{
		theAnswer = kAudioHardwarePropertyDefaultInputDevice;
	}
	else if(inIsSystem)
	{
		theAnswer = kAudioHardwarePropertyDefaultSystemOutputDevice;
	}
	return theAnswer;
}

AudioObjectID	CAHALAudioSystemObject::GetDefaultAudioDevice(bool inIsInput, bool inIsSystem) const
{
	AudioObjectID theAnswer = kAudioObjectUnknown;
	CAPropertyAddress theAddress(CAHALAudioSystemObject_CalculateDefaultDeviceProperySelector(inIsInput, inIsSystem));
	UInt32 theSize = sizeof(AudioObjectID);
	GetPropertyData(theAddress, 0, NULL, theSize, &theAnswer);
	return theAnswer;
}

void	CAHALAudioSystemObject::SetDefaultAudioDevice(bool inIsInput, bool inIsSystem, AudioObjectID inNewDefaultDevice)
{
	CAPropertyAddress theAddress(CAHALAudioSystemObject_CalculateDefaultDeviceProperySelector(inIsInput, inIsSystem));
	UInt32 theSize = sizeof(AudioObjectID);
	SetPropertyData(theAddress, 0, NULL, theSize, &inNewDefaultDevice);
}

AudioObjectID	CAHALAudioSystemObject::GetAudioPlugInForBundleID(CFStringRef inUID) const
{
	AudioObjectID theAnswer = kAudioObjectUnknown;
	AudioValueTranslation theValue = { &inUID, sizeof(CFStringRef), &theAnswer, sizeof(AudioObjectID) };
	CAPropertyAddress theAddress(kAudioHardwarePropertyPlugInForBundleID);
	UInt32 theSize = sizeof(AudioValueTranslation);
	GetPropertyData(theAddress, 0, NULL, theSize, &theValue);
	return theAnswer;
}

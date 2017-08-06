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
//  Mock_CAHALAudioObject.cpp
//  BGMAppUnitTests
//
//  Copyright © 2016 Kyle Neideck
//

// Self include
#include "CAHALAudioObject.h"

// BGM Includes
#include "BGM_Types.h"

// System includes
#include <CoreAudio/AudioHardware.h>


#pragma clang diagnostic ignored "-Wunused-parameter"

// The value of the music player bundle ID property. Tests should set this back to "" when they finish. (Has
// to be static because we can't add to the real class's interface.)
static CFStringRef playerBundleID = CFSTR("");

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

void	CAHALAudioObject::GetPropertyData(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32& ioDataSize, void* outData) const
{
    if(inAddress.mSelector == kAudioDeviceCustomPropertyMusicPlayerBundleID)
    {
        *reinterpret_cast<CFStringRef*>(outData) = playerBundleID;
    }
}

void	CAHALAudioObject::SetPropertyData(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData)
{
    if(inAddress.mSelector == kAudioDeviceCustomPropertyMusicPlayerBundleID)
    {
        playerBundleID = *reinterpret_cast<const CFStringRef*>(inData);
    }
}

#pragma mark Unimplemented methods

void	CAHALAudioObject::SetObjectID(AudioObjectID inObjectID)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

AudioClassID	CAHALAudioObject::GetClassID() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

AudioObjectID	CAHALAudioObject::GetOwnerObjectID() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

CFStringRef	CAHALAudioObject::CopyOwningPlugInBundleID() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

CFStringRef	CAHALAudioObject::CopyName() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

CFStringRef	CAHALAudioObject::CopyManufacturer() const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

CFStringRef	CAHALAudioObject::CopyNameForElement(AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

CFStringRef	CAHALAudioObject::CopyCategoryNameForElement(AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

CFStringRef	CAHALAudioObject::CopyNumberNameForElement(AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioObject::ObjectExists(AudioObjectID inObjectID)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioObject::GetNumberOwnedObjects(AudioClassID inClass) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioObject::GetAllOwnedObjects(AudioClassID inClass, UInt32& ioNumberObjects, AudioObjectID* ioObjectIDs) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

AudioObjectID	CAHALAudioObject::GetOwnedObjectByIndex(AudioClassID inClass, UInt32 inIndex)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioObject::HasProperty(const AudioObjectPropertyAddress& inAddress) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

bool	CAHALAudioObject::IsPropertySettable(const AudioObjectPropertyAddress& inAddress) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

UInt32	CAHALAudioObject::GetPropertyDataSize(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioObject::AddPropertyListener(const AudioObjectPropertyAddress& inAddress, AudioObjectPropertyListenerProc inListenerProc, void* inClientData)
{
    Throw(new CAException(kAudio_UnimplementedError));
}

void	CAHALAudioObject::RemovePropertyListener(const AudioObjectPropertyAddress& inAddress, AudioObjectPropertyListenerProc inListenerProc, void* inClientData)
{
    Throw(new CAException(kAudio_UnimplementedError));
}


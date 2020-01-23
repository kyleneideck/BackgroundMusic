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
//  Copyright © 2016, 2020 Kyle Neideck
//

// Self Include
#include "CAHALAudioObject.h"

// Local Includes
#include "MockAudioObjects.h"

// BGM Includes
#include "BGM_Types.h"

// PublicUtility Includes
#include "CACFString.h"


#pragma clang diagnostic ignored "-Wunused-parameter"

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
    switch(inAddress.mSelector)
    {
        case kAudioDeviceCustomPropertyMusicPlayerBundleID:
            *reinterpret_cast<CFStringRef*>(outData) =
                    MockAudioObjects::GetAudioDevice(GetObjectID())->
                            GetPlayerBundleID().CopyCFString();
            break;

        case kAudioDevicePropertyStreams:
            reinterpret_cast<AudioObjectID*>(outData)[0] = 1;
            if(inAddress.mScope == kAudioObjectPropertyScopeGlobal)
            {
                reinterpret_cast<AudioObjectID*>(outData)[1] = 2;
            }
            break;

        case kAudioDevicePropertyBufferFrameSize:
            *reinterpret_cast<UInt32*>(outData) = 512;
            break;

        case kAudioDevicePropertyDeviceIsAlive:
            *reinterpret_cast<UInt32*>(outData) = 1;
            break;

        case kAudioStreamPropertyVirtualFormat:
        {
            AudioStreamBasicDescription* outASBD =
                    reinterpret_cast<AudioStreamBasicDescription*>(outData);
            outASBD->mSampleRate = 44100.0;
            outASBD->mFormatID = kAudioFormatLinearPCM;
            outASBD->mFormatFlags =
                    kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
            outASBD->mBytesPerPacket = 8;
            outASBD->mFramesPerPacket = 1;
            outASBD->mBytesPerFrame = 8;
            outASBD->mChannelsPerFrame = 2;
            outASBD->mBitsPerChannel = 32;
            break;
        }

        default:
            Throw(new CAException(kAudio_UnimplementedError));
    }
}

void	CAHALAudioObject::SetPropertyData(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData)
{
    switch(inAddress.mSelector)
    {
        case kAudioDeviceCustomPropertyMusicPlayerBundleID:
            MockAudioObjects::GetAudioDevice(GetObjectID())->SetPlayerBundleID(
                    CACFString(*reinterpret_cast<const CFStringRef*>(inData), false));
            break;
        default:
            break;
    }
}

UInt32	CAHALAudioObject::GetPropertyDataSize(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const
{
    switch(inAddress.mSelector)
    {
        case kAudioDevicePropertyStreams:
            return (inAddress.mScope == kAudioObjectPropertyScopeGlobal ? 2 : 1) *
                    sizeof(AudioObjectID);
        default:
            Throw(new CAException(kAudio_UnimplementedError));
    }
}

void	CAHALAudioObject::AddPropertyListener(const AudioObjectPropertyAddress& inAddress, AudioObjectPropertyListenerProc inListenerProc, void* inClientData)
{
    MockAudioObjects::GetAudioObject(GetObjectID())->
            mPropertiesWithListeners.insert(inAddress.mSelector);
}

void	CAHALAudioObject::RemovePropertyListener(const AudioObjectPropertyAddress& inAddress, AudioObjectPropertyListenerProc inListenerProc, void* inClientData)
{
    MockAudioObjects::GetAudioObject(GetObjectID())->
            mPropertiesWithListeners.erase(inAddress.mSelector);
}

#pragma mark Unimplemented Methods

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


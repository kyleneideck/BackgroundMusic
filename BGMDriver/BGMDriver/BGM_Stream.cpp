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

//  BGM_Stream.cpp
//  BGMDriver
//
//  Copyright © 2017 Kyle Neideck
//  Portions copyright (C) 2013 Apple Inc. All Rights Reserved.
//

// Self Include
#include "BGM_Stream.h"

// Local Includes
#include "BGM_Types.h"
#include "BGM_Utils.h"
#include "BGM_Device.h"
#include "BGM_PlugIn.h"

// PublicUtility Includes
#include "CADebugMacros.h"
#include "CAException.h"
#include "CAPropertyAddress.h"
#include "CADispatchQueue.h"


#pragma clang assume_nonnull begin

BGM_Stream::BGM_Stream(AudioObjectID inObjectID,
                       AudioDeviceID inOwnerDeviceID,
                       bool inIsInput,
                       Float64 inSampleRate,
                       UInt32 inStartingChannel)
:
    BGM_Object(inObjectID, kAudioStreamClassID, kAudioObjectClassID, inOwnerDeviceID),
    mStateMutex(inIsInput ? "Input Stream State" : "Output Stream State"),
    mIsInput(inIsInput),
    mIsStreamActive(false),
    mSampleRate(inSampleRate),
    mStartingChannel(inStartingChannel)
{
}

BGM_Stream::~BGM_Stream()
{
}

bool    BGM_Stream::HasProperty(AudioObjectID inObjectID,
                                pid_t inClientPID,
                                const AudioObjectPropertyAddress& inAddress) const
{
    //    For each object, this driver implements all the required properties plus a few extras that
    //    are useful but not required. There is more detailed commentary about each property in the
    //    GetPropertyData() method.

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

bool    BGM_Stream::IsPropertySettable(AudioObjectID inObjectID,
                                       pid_t inClientPID,
                                       const AudioObjectPropertyAddress& inAddress) const
{
    // For each object, this driver implements all the required properties plus a few extras that
    // are useful but not required. There is more detailed commentary about each property in the
    // GetPropertyData() method.

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

UInt32    BGM_Stream::GetPropertyDataSize(AudioObjectID inObjectID,
                                          pid_t inClientPID,
                                          const AudioObjectPropertyAddress& inAddress,
                                          UInt32 inQualifierDataSize,
                                          const void* __nullable inQualifierData) const
{
    // For each object, this driver implements all the required properties plus a few extras that
    // are useful but not required. There is more detailed commentary about each property in the
    // GetPropertyData() method.

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
            theAnswer = BGM_Object::GetPropertyDataSize(inObjectID,
                                                        inClientPID,
                                                        inAddress,
                                                        inQualifierDataSize,
                                                        inQualifierData);
            break;
    };

    return theAnswer;
}

void    BGM_Stream::GetPropertyData(AudioObjectID inObjectID,
                                    pid_t inClientPID,
                                    const AudioObjectPropertyAddress& inAddress,
                                    UInt32 inQualifierDataSize,
                                    const void* __nullable inQualifierData,
                                    UInt32 inDataSize,
                                    UInt32& outDataSize,
                                    void* outData) const
{
    // Since most of the data that will get returned is static, there are few instances where it is
    // necessary to lock the state mutex.

    switch(inAddress.mSelector)
    {
        case kAudioObjectPropertyBaseClass:
            // The base class for kAudioStreamClassID is kAudioObjectClassID
            ThrowIf(inDataSize < sizeof(AudioClassID),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_Stream::GetPropertyData: not enough space for the return "
                    "value of kAudioObjectPropertyBaseClass for the stream");
            *reinterpret_cast<AudioClassID*>(outData) = kAudioObjectClassID;
            outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyClass:
            // Streams are of the class kAudioStreamClassID
            ThrowIf(inDataSize < sizeof(AudioClassID),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_Stream::GetPropertyData: not enough space for the return "
                    "value of kAudioObjectPropertyClass for the stream");
            *reinterpret_cast<AudioClassID*>(outData) = kAudioStreamClassID;
            outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyOwner:
            // A stream's owner is a device object.
            {
                ThrowIf(inDataSize < sizeof(AudioObjectID),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_Stream::GetPropertyData: not enough space for the return "
                        "value of kAudioObjectPropertyOwner for the stream");

                // Lock the state mutex to create a memory barrier, just in case a subclass ever
                // allows mOwnerObjectID to be modified.
                CAMutex::Locker theStateLocker(mStateMutex);

                // Return the requested value.
                *reinterpret_cast<AudioObjectID*>(outData) = mOwnerObjectID;
                outDataSize = sizeof(AudioObjectID);
            }
            break;

        case kAudioStreamPropertyIsActive:
            // This property tells the device whether or not the given stream is going to
            // be used for IO. Note that we need to take the state lock to examine this
            // value.
            {
                ThrowIf(inDataSize < sizeof(UInt32),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_Stream::GetPropertyData: not enough space for the return "
                        "value of kAudioStreamPropertyIsActive for the stream");

                // Take the state lock.
                CAMutex::Locker theStateLocker(mStateMutex);

                // Return the requested value.
                *reinterpret_cast<UInt32*>(outData) = mIsStreamActive;
                outDataSize = sizeof(UInt32);
            }
            break;

        case kAudioStreamPropertyDirection:
            // This returns whether the stream is an input or output stream.
            ThrowIf(inDataSize < sizeof(UInt32),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_Stream::GetPropertyData: not enough space for the return value of "
                    "kAudioStreamPropertyDirection for the stream");
            *reinterpret_cast<UInt32*>(outData) = mIsInput ? 1 : 0;
            outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyTerminalType:
            // This returns a value that indicates what is at the other end of the stream
            // such as a speaker or headphones or a microphone.
            ThrowIf(inDataSize < sizeof(UInt32),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_Stream::GetPropertyData: not enough space for the return value of "
                    "kAudioStreamPropertyTerminalType for the stream");
            *reinterpret_cast<UInt32*>(outData) =
                mIsInput ? kAudioStreamTerminalTypeMicrophone : kAudioStreamTerminalTypeSpeaker;
            outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyStartingChannel:
            // This property returns the absolute channel number for the first channel in
            // the stream. For example, if a device has two output streams with two
            // channels each, then the starting channel number for the first stream is 1
            // and the starting channel number for the second stream is 3.
            ThrowIf(inDataSize < sizeof(UInt32),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_Stream::GetPropertyData: not enough space for the return "
                    "value of kAudioStreamPropertyStartingChannel for the stream");
            *reinterpret_cast<UInt32*>(outData) = mStartingChannel;
            outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyLatency:
            // This property returns any additonal presentation latency the stream has.
            ThrowIf(inDataSize < sizeof(UInt32),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_Stream::GetPropertyData: not enough space for the return "
                    "value of kAudioStreamPropertyLatency for the stream");
            *reinterpret_cast<UInt32*>(outData) = 0;
            outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyVirtualFormat:
        case kAudioStreamPropertyPhysicalFormat:
            // This returns the current format of the stream in an AudioStreamBasicDescription.
            // For devices that don't override the mix operation, the virtual format has to be the
            // same as the physical format.
            {
                ThrowIf(inDataSize < sizeof(AudioStreamBasicDescription),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_Stream::GetPropertyData: not enough space for the return "
                        "value of kAudioStreamPropertyVirtualFormat for the stream");

                // This particular device always vends 32-bit native endian floats
                AudioStreamBasicDescription* outASBD =
                    reinterpret_cast<AudioStreamBasicDescription*>(outData);

                // Our streams have the same sample rate as the device they belong to.
                outASBD->mSampleRate = mSampleRate;
                outASBD->mFormatID = kAudioFormatLinearPCM;
                outASBD->mFormatFlags =
                    kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
                outASBD->mBytesPerPacket = 8;
                outASBD->mFramesPerPacket = 1;
                outASBD->mBytesPerFrame = 8;
                outASBD->mChannelsPerFrame = 2;
                outASBD->mBitsPerChannel = 32;

                outDataSize = sizeof(AudioStreamBasicDescription);
            }
            break;

        case kAudioStreamPropertyAvailableVirtualFormats:
        case kAudioStreamPropertyAvailablePhysicalFormats:
            // This returns an array of AudioStreamRangedDescriptions that describe what
            // formats are supported.
            if((inDataSize / sizeof(AudioStreamRangedDescription)) >= 1)
            {
                AudioStreamRangedDescription* outASRD =
                    reinterpret_cast<AudioStreamRangedDescription*>(outData);

                outASRD[0].mFormat.mSampleRate = mSampleRate;
                outASRD[0].mFormat.mFormatID = kAudioFormatLinearPCM;
                outASRD[0].mFormat.mFormatFlags =
                    kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
                outASRD[0].mFormat.mBytesPerPacket = 8;
                outASRD[0].mFormat.mFramesPerPacket = 1;
                outASRD[0].mFormat.mBytesPerFrame = 8;
                outASRD[0].mFormat.mChannelsPerFrame = 2;
                outASRD[0].mFormat.mBitsPerChannel = 32;
                // These match kAudioDevicePropertyAvailableNominalSampleRates.
                outASRD[0].mSampleRateRange.mMinimum = 1.0;
                outASRD[0].mSampleRateRange.mMaximum = 1000000000.0;

                // Report how much we wrote.
                outDataSize = sizeof(AudioStreamRangedDescription);
            }
            else
            {
                outDataSize = 0;
            }
            break;

        default:
            BGM_Object::GetPropertyData(inObjectID,
                                        inClientPID,
                                        inAddress,
                                        inQualifierDataSize,
                                        inQualifierData,
                                        inDataSize,
                                        outDataSize,
                                        outData);
            break;
    };
}

void    BGM_Stream::SetPropertyData(AudioObjectID inObjectID,
                                    pid_t inClientPID,
                                    const AudioObjectPropertyAddress& inAddress,
                                    UInt32 inQualifierDataSize,
                                    const void* __nullable inQualifierData,
                                    UInt32 inDataSize,
                                    const void* inData)
{
    // There is more detailed commentary about each property in the GetPropertyData() method.

    switch(inAddress.mSelector)
    {
        case kAudioStreamPropertyIsActive:
            {
                // Changing the active state of a stream doesn't affect IO or change the structure
                // so we can just save the state and send the notification.
                ThrowIf(inDataSize != sizeof(UInt32),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_Stream::SetPropertyData: wrong size for the data for "
                        "kAudioStreamPropertyIsActive");
                bool theNewIsActive = *reinterpret_cast<const UInt32*>(inData) != 0;

                CAMutex::Locker theStateLocker(mStateMutex);

                if(mIsStreamActive != theNewIsActive)
                {
                    mIsStreamActive = theNewIsActive;

                    // Send the notification.
                    CADispatchQueue::GetGlobalSerialQueue().Dispatch(false,	^{
                        AudioObjectPropertyAddress theProperty[] = {
                            CAPropertyAddress(kAudioStreamPropertyIsActive)
                        };
                        BGM_PlugIn::Host_PropertiesChanged(inObjectID, 1, theProperty);
                    });
                }
            }
            break;

        case kAudioStreamPropertyVirtualFormat:
        case kAudioStreamPropertyPhysicalFormat:
            {
                // The device that owns the stream handles changing the stream format, as it needs
                // to be handled via the RequestConfigChange/PerformConfigChange machinery. The
                // stream only needs to validate the format at this point.
                //
                // Note that because our devices only supports 2 channel 32 bit float data, the only
                // thing that can change is the sample rate.
                ThrowIf(inDataSize != sizeof(AudioStreamBasicDescription),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_Stream::SetPropertyData: wrong size for the data for "
                        "kAudioStreamPropertyPhysicalFormat");

                const AudioStreamBasicDescription* theNewFormat =
                    reinterpret_cast<const AudioStreamBasicDescription*>(inData);

                ThrowIf(theNewFormat->mFormatID != kAudioFormatLinearPCM,
                        CAException(kAudioDeviceUnsupportedFormatError),
                        "BGM_Stream::SetPropertyData: unsupported format ID for "
                        "kAudioStreamPropertyPhysicalFormat");
                ThrowIf(theNewFormat->mFormatFlags !=
                            (kAudioFormatFlagIsFloat |
                             kAudioFormatFlagsNativeEndian |
                             kAudioFormatFlagIsPacked),
                        CAException(kAudioDeviceUnsupportedFormatError),
                        "BGM_Stream::SetPropertyData: unsupported format flags for "
                        "kAudioStreamPropertyPhysicalFormat");
                ThrowIf(theNewFormat->mBytesPerPacket != 8,
                        CAException(kAudioDeviceUnsupportedFormatError),
                        "BGM_Stream::SetPropertyData: unsupported bytes per packet for "
                        "kAudioStreamPropertyPhysicalFormat");
                ThrowIf(theNewFormat->mFramesPerPacket != 1,
                        CAException(kAudioDeviceUnsupportedFormatError),
                        "BGM_Stream::SetPropertyData: unsupported frames per packet for "
                        "kAudioStreamPropertyPhysicalFormat");
                ThrowIf(theNewFormat->mBytesPerFrame != 8,
                        CAException(kAudioDeviceUnsupportedFormatError),
                        "BGM_Stream::SetPropertyData: unsupported bytes per frame for "
                        "kAudioStreamPropertyPhysicalFormat");
                ThrowIf(theNewFormat->mChannelsPerFrame != 2,
                        CAException(kAudioDeviceUnsupportedFormatError),
                        "BGM_Stream::SetPropertyData: unsupported channels per frame for "
                        "kAudioStreamPropertyPhysicalFormat");
                ThrowIf(theNewFormat->mBitsPerChannel != 32,
                        CAException(kAudioDeviceUnsupportedFormatError),
                        "BGM_Stream::SetPropertyData: unsupported bits per channel for "
                        "kAudioStreamPropertyPhysicalFormat");
                ThrowIf(theNewFormat->mSampleRate < 1.0,
                        CAException(kAudioDeviceUnsupportedFormatError),
                        "BGM_Stream::SetPropertyData: unsupported sample rate for "
                        "kAudioStreamPropertyPhysicalFormat");
            }
            break;

        default:
            BGM_Object::SetPropertyData(inObjectID,
                                        inClientPID,
                                        inAddress,
                                        inQualifierDataSize,
                                        inQualifierData,
                                        inDataSize,
                                        inData);
            break;
    };
}

#pragma mark Accessors

void    BGM_Stream::SetSampleRate(Float64 inSampleRate)
{
    CAMutex::Locker theStateLocker(mStateMutex);
    mSampleRate = inSampleRate;
}

#pragma clang assume_nonnull end


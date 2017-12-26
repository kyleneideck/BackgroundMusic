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
//  BGM_AbstractDevice.cpp
//  BGMDriver
//
//  Copyright © 2017 Kyle Neideck
//  Portions copyright (C) 2013 Apple Inc. All Rights Reserved.
//

// Self Include
#include "BGM_AbstractDevice.h"

// Local Includes
#include "BGM_Utils.h"

// PublicUtility Includes
#include "CAException.h"
#include "CADebugMacros.h"


#pragma clang assume_nonnull begin

BGM_AbstractDevice::BGM_AbstractDevice(AudioObjectID inObjectID, AudioObjectID inOwnerObjectID)
:
    BGM_Object(inObjectID, kAudioDeviceClassID, kAudioObjectClassID, inOwnerObjectID)
{
}

BGM_AbstractDevice::~BGM_AbstractDevice()
{
}

#pragma mark Property Operations

bool    BGM_AbstractDevice::HasProperty(AudioObjectID inObjectID,
                                        pid_t inClientPID,
                                        const AudioObjectPropertyAddress& inAddress) const
{
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
        case kAudioDevicePropertyNominalSampleRate:
        case kAudioDevicePropertyAvailableNominalSampleRates:
        case kAudioDevicePropertyIsHidden:
        case kAudioDevicePropertyZeroTimeStampPeriod:
            theAnswer = true;
            break;
            
        default:
            theAnswer = BGM_Object::HasProperty(inObjectID, inClientPID, inAddress);
            break;
    };

    return theAnswer;
}

bool    BGM_AbstractDevice::IsPropertySettable(AudioObjectID inObjectID,
                                               pid_t inClientPID,
                                               const AudioObjectPropertyAddress& inAddress) const
{
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
        case kAudioDevicePropertyNominalSampleRate:
        case kAudioDevicePropertyAvailableNominalSampleRates:
        case kAudioDevicePropertyIsHidden:
        case kAudioDevicePropertyZeroTimeStampPeriod:
            theAnswer = false;
            break;
            
        default:
            theAnswer = BGM_Object::IsPropertySettable(inObjectID, inClientPID, inAddress);
            break;
    };

    return theAnswer;
}

UInt32    BGM_AbstractDevice::GetPropertyDataSize(AudioObjectID inObjectID,
                                                  pid_t inClientPID,
                                                  const AudioObjectPropertyAddress& inAddress,
                                                  UInt32 inQualifierDataSize,
                                                  const void* __nullable inQualifierData) const
{
    UInt32 theAnswer = 0;

    switch(inAddress.mSelector)
    {
        case kAudioObjectPropertyName:
            theAnswer = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyManufacturer:
            theAnswer = sizeof(CFStringRef);
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

        case kAudioDevicePropertyDeviceCanBeDefaultDevice:
            theAnswer = sizeof(UInt32);
            break;

        case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
            theAnswer = sizeof(UInt32);
            break;

        case kAudioDevicePropertyDeviceIsAlive:
            theAnswer = sizeof(AudioClassID);
            break;

        case kAudioDevicePropertyDeviceIsRunning:
            theAnswer = sizeof(UInt32);
            break;

        case kAudioDevicePropertyLatency:
            theAnswer = sizeof(UInt32);
            break;

        case kAudioDevicePropertyStreams:
            theAnswer = 0;
            break;

        case kAudioObjectPropertyControlList:
            theAnswer = 0;
            break;

        case kAudioDevicePropertySafetyOffset:
            theAnswer = sizeof(UInt32);
            break;

        case kAudioDevicePropertyNominalSampleRate:
            theAnswer = sizeof(Float64);
            break;

        case kAudioDevicePropertyAvailableNominalSampleRates:
            theAnswer = 0;
            break;

        case kAudioDevicePropertyIsHidden:
            theAnswer = sizeof(UInt32);
            break;

        case kAudioDevicePropertyZeroTimeStampPeriod:
            theAnswer = sizeof(UInt32);
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

void    BGM_AbstractDevice::GetPropertyData(AudioObjectID inObjectID,
                                            pid_t inClientPID,
                                            const AudioObjectPropertyAddress& inAddress,
                                            UInt32 inQualifierDataSize,
                                            const void* __nullable inQualifierData,
                                            UInt32 inDataSize,
                                            UInt32& outDataSize,
                                            void* outData) const
{
    UInt32 theNumberItemsToFetch;

    switch(inAddress.mSelector)
    {
        case kAudioObjectPropertyName:
        case kAudioObjectPropertyManufacturer:
        case kAudioDevicePropertyDeviceUID:
        case kAudioDevicePropertyModelUID:
        case kAudioDevicePropertyDeviceIsRunning:
        case kAudioDevicePropertyZeroTimeStampPeriod:
        case kAudioDevicePropertyNominalSampleRate:
        case kAudioDevicePropertyAvailableNominalSampleRates:
            // Should be unreachable. Reaching this point would mean a concrete device has delegated
            // a required property that can't be handled by this class or its parent, BGM_Object.
            //
            // See BGM_Device for info about these properties.
            //
            // TODO: Write a test that checks all required properties for each subclass.
            BGMAssert(false,
                      "BGM_AbstractDevice::GetPropertyData: Property %u not handled in subclass",
                      inAddress.mSelector);
            // Throw in release builds.
            Throw(CAException(kAudioHardwareIllegalOperationError));

        case kAudioDevicePropertyTransportType:
            // This value represents how the device is attached to the system. This can be
            // any 32 bit integer, but common values for this property are defined in
            // <CoreAudio/AudioHardwareBase.h>.
            ThrowIf(inDataSize < sizeof(UInt32),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_AbstractDevice::GetPropertyData: not enough space for the return value of "
                    "kAudioDevicePropertyTransportType for the device");
            // Default to virtual device.
            *reinterpret_cast<UInt32*>(outData) = kAudioDeviceTransportTypeVirtual;
            outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyRelatedDevices:
            // The related devices property identifies device objects that are very closely
            // related. Generally, this is for relating devices that are packaged together
            // in the hardware such as when the input side and the output side of a piece
            // of hardware can be clocked separately and therefore need to be represented
            // as separate AudioDevice objects. In such case, both devices would report
            // that they are related to each other. Note that at minimum, a device is
            // related to itself, so this list will always be at least one item long.

            // Calculate the number of items that have been requested. Note that this
            // number is allowed to be smaller than the actual size of the list. In such
            // case, only that number of items will be returned
            theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);

            // Default to only have the one device.
            if(theNumberItemsToFetch > 1)
            {
                theNumberItemsToFetch = 1;
            }

            // Write the devices' object IDs into the return value.
            if(theNumberItemsToFetch > 0)
            {
                reinterpret_cast<AudioObjectID*>(outData)[0] = GetObjectID();
            }
            
            // Report how much we wrote.
            outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
            break;

        case kAudioDevicePropertyClockDomain:
            // This property allows the device to declare what other devices it is
            // synchronized with in hardware. The way it works is that if two devices have
            // the same value for this property and the value is not zero, then the two
            // devices are synchronized in hardware. Note that a device that either can't
            // be synchronized with others or doesn't know should return 0 for this
            // property.
            //
            // Default to 0.
            ThrowIf(inDataSize < sizeof(UInt32),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_AbstractDevice::GetPropertyData: not enough space for the return value of "
                    "kAudioDevicePropertyClockDomain for the device");
            *reinterpret_cast<UInt32*>(outData) = 0;
            outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyDeviceIsAlive:
            // Default to alive.
            ThrowIf(inDataSize < sizeof(UInt32),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_AbstractDevice::GetPropertyData: not enough space for the return value of "
                    "kAudioDevicePropertyDeviceIsAlive for the device");
            *reinterpret_cast<UInt32*>(outData) = 1;
            outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyDeviceCanBeDefaultDevice:
            // This property returns whether or not the device wants to be able to be the
            // default device for content. This is the device that iTunes and QuickTime
            // will use to play their content on and FaceTime will use as it's microphone.
            // Nearly all devices should allow for this.
            //
            // Default to true.
            ThrowIf(inDataSize < sizeof(UInt32),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_AbstractDevice::GetPropertyData: not enough space for the return value of "
                    "kAudioDevicePropertyDeviceCanBeDefaultDevice for the device");
            *reinterpret_cast<UInt32*>(outData) = 1;
            outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
            // This property returns whether or not the device wants to be the system
            // default device. This is the device that is used to play interface sounds and
            // other incidental or UI-related sounds on. Most devices should allow this
            // although devices with lots of latency may not want to.
            //
            // Default to true.
            ThrowIf(inDataSize < sizeof(UInt32),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_AbstractDevice::GetPropertyData: not enough space for the return value of "
                    "kAudioDevicePropertyDeviceCanBeDefaultSystemDevice for the device");
            *reinterpret_cast<UInt32*>(outData) = 1;
            outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyLatency:
            // This property returns the presentation latency of the device. Default to 0.
            ThrowIf(inDataSize < sizeof(UInt32),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_AbstractDevice::GetPropertyData: not enough space for the return value of "
                    "kAudioDevicePropertyLatency for the device");
            *reinterpret_cast<UInt32*>(outData) = 0;
            outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyStreams:
            // Default to not having any streams.
            outDataSize = 0;
            break;

        case kAudioObjectPropertyControlList:
            // Default to not having any controls.
            outDataSize = 0;
            break;

        case kAudioDevicePropertySafetyOffset:
            // This property returns the how close to now the HAL can read and write. Default to 0.
            ThrowIf(inDataSize < sizeof(UInt32),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_AbstractDevice::GetPropertyData: not enough space for the return value of "
                    "kAudioDevicePropertySafetyOffset for the device");
            *reinterpret_cast<UInt32*>(outData) = 0;
            outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyIsHidden:
            // This returns whether or not the device is visible to clients. Default to not hidden.
            ThrowIf(inDataSize < sizeof(UInt32),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_AbstractDevice::GetPropertyData: not enough space for the return value of "
                    "kAudioDevicePropertyIsHidden for the device");
            *reinterpret_cast<UInt32*>(outData) = 0;
            outDataSize = sizeof(UInt32);
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

#pragma clang assume_nonnull end


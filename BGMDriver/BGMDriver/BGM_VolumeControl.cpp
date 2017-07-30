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
//  BGM_VolumeControl.cpp
//  BGMDriver
//
//  Copyright © 2016, 2017 Kyle Neideck
//  Portions copyright (C) 2013 Apple Inc. All Rights Reserved.
//

// Self Include
#include "BGM_VolumeControl.h"

// Local Includes
#include "BGM_PlugIn.h"

// PublicUtility Includes
#include "CAException.h"
#include "CADebugMacros.h"
#include "CADispatchQueue.h"

// STL Includes
#include <algorithm>

// System Includes
#include <CoreAudio/AudioHardwareBase.h>


#pragma clang assume_nonnull begin

#pragma mark Construction/Destruction

BGM_VolumeControl::BGM_VolumeControl(AudioObjectID inObjectID,
                                     AudioObjectID inOwnerObjectID,
                                     AudioObjectPropertyScope inScope,
                                     AudioObjectPropertyElement inElement)
:
    BGM_Control(inObjectID,
                kAudioVolumeControlClassID,
                kAudioLevelControlClassID,
                inOwnerObjectID,
                inScope,
                inElement),
    mMutex("Volume Control"),
    mVolumeRaw(kDefaultMinRawVolume),
    mMinVolumeRaw(kDefaultMinRawVolume),
    mMaxVolumeRaw(kDefaultMaxRawVolume),
    mMinVolumeDb(kDefaultMinDbVolume),
    mMaxVolumeDb(kDefaultMaxDbVolume)
{
    // Setup the volume curve with the one range
    mVolumeCurve.AddRange(mMinVolumeRaw, mMaxVolumeRaw, mMinVolumeDb, mMaxVolumeDb);
}

#pragma mark Property Operations

bool    BGM_VolumeControl::HasProperty(AudioObjectID inObjectID,
                                       pid_t inClientPID,
                                       const AudioObjectPropertyAddress& inAddress) const
{
    CheckObjectID(inObjectID);

    bool theAnswer = false;

    switch(inAddress.mSelector)
    {
        case kAudioLevelControlPropertyScalarValue:
        case kAudioLevelControlPropertyDecibelValue:
        case kAudioLevelControlPropertyDecibelRange:
        case kAudioLevelControlPropertyConvertScalarToDecibels:
        case kAudioLevelControlPropertyConvertDecibelsToScalar:
            theAnswer = true;
            break;

        default:
            theAnswer = BGM_Control::HasProperty(inObjectID, inClientPID, inAddress);
            break;
    };

    return theAnswer;
}

bool    BGM_VolumeControl::IsPropertySettable(AudioObjectID inObjectID,
                                              pid_t inClientPID,
                                              const AudioObjectPropertyAddress& inAddress) const
{
    CheckObjectID(inObjectID);

    bool theAnswer = false;

    switch(inAddress.mSelector)
    {
        case kAudioLevelControlPropertyDecibelRange:
        case kAudioLevelControlPropertyConvertScalarToDecibels:
        case kAudioLevelControlPropertyConvertDecibelsToScalar:
            theAnswer = false;
            break;

        case kAudioLevelControlPropertyScalarValue:
        case kAudioLevelControlPropertyDecibelValue:
            theAnswer = true;
            break;

        default:
            theAnswer = BGM_Control::IsPropertySettable(inObjectID, inClientPID, inAddress);
            break;
    };

    return theAnswer;
}

UInt32  BGM_VolumeControl::GetPropertyDataSize(AudioObjectID inObjectID,
                                               pid_t inClientPID,
                                               const AudioObjectPropertyAddress& inAddress,
                                               UInt32 inQualifierDataSize,
                                               const void* inQualifierData) const
{
    CheckObjectID(inObjectID);

    UInt32 theAnswer = 0;

    switch(inAddress.mSelector)
    {
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

        default:
            theAnswer = BGM_Control::GetPropertyDataSize(inObjectID,
                                                         inClientPID,
                                                         inAddress,
                                                         inQualifierDataSize,
                                                         inQualifierData);
            break;
    };

    return theAnswer;
}

void    BGM_VolumeControl::GetPropertyData(AudioObjectID inObjectID,
                                           pid_t inClientPID,
                                           const AudioObjectPropertyAddress& inAddress,
                                           UInt32 inQualifierDataSize,
                                           const void* inQualifierData,
                                           UInt32 inDataSize,
                                           UInt32& outDataSize,
                                           void* outData) const
{
    CheckObjectID(inObjectID);

    switch(inAddress.mSelector)
    {
        case kAudioLevelControlPropertyScalarValue:
            // This returns the value of the control in the normalized range of 0 to 1.
            {
                ThrowIf(inDataSize < sizeof(Float32),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_VolumeControl::GetPropertyData: not enough space for the return value "
                        "of kAudioLevelControlPropertyScalarValue for the volume control");

                CAMutex::Locker theLocker(mMutex);

                *reinterpret_cast<Float32*>(outData) = mVolumeCurve.ConvertRawToScalar(mVolumeRaw);
                outDataSize = sizeof(Float32);
            }
            break;

        case kAudioLevelControlPropertyDecibelValue:
            // This returns the dB value of the control.
            {
                ThrowIf(inDataSize < sizeof(Float32),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_VolumeControl::GetPropertyData: not enough space for the return value "
                        "of kAudioLevelControlPropertyDecibelValue for the volume control");

                CAMutex::Locker theLocker(mMutex);

                *reinterpret_cast<Float32*>(outData) = mVolumeCurve.ConvertRawToDB(mVolumeRaw);
                outDataSize = sizeof(Float32);
            }
            break;

        case kAudioLevelControlPropertyDecibelRange:
            // This returns the dB range of the control.
            ThrowIf(inDataSize < sizeof(AudioValueRange),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_VolumeControl::GetPropertyData: not enough space for the return value of "
                    "kAudioLevelControlPropertyDecibelRange for the volume control");
            reinterpret_cast<AudioValueRange*>(outData)->mMinimum = mVolumeCurve.GetMinimumDB();
            reinterpret_cast<AudioValueRange*>(outData)->mMaximum = mVolumeCurve.GetMaximumDB();
            outDataSize = sizeof(AudioValueRange);
            break;

        case kAudioLevelControlPropertyConvertScalarToDecibels:
            // This takes the scalar value in outData and converts it to dB.
            {
                ThrowIf(inDataSize < sizeof(Float32),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_VolumeControl::GetPropertyData: not enough space for the return value "
                        "of kAudioLevelControlPropertyConvertScalarToDecibels for the volume "
                        "control");

                // clamp the value to be between 0 and 1
                Float32 theVolumeValue = *reinterpret_cast<Float32*>(outData);
                theVolumeValue = std::min(1.0f, std::max(0.0f, theVolumeValue));

                // do the conversion
                *reinterpret_cast<Float32*>(outData) =
                        mVolumeCurve.ConvertScalarToDB(theVolumeValue);

                // report how much we wrote
                outDataSize = sizeof(Float32);
            }
            break;

        case kAudioLevelControlPropertyConvertDecibelsToScalar:
            // This takes the dB value in outData and converts it to scalar.
            {
                ThrowIf(inDataSize < sizeof(Float32),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_VolumeControl::GetPropertyData: not enough space for the return value "
                        "of kAudioLevelControlPropertyConvertDecibelsToScalar for the volume "
                        "control");

                // clamp the value to be between mMinVolumeDb and mMaxVolumeDb
                Float32 theVolumeValue = *reinterpret_cast<Float32*>(outData);
                theVolumeValue = std::min(mMaxVolumeDb, std::max(mMinVolumeDb, theVolumeValue));

                // do the conversion
                *reinterpret_cast<Float32*>(outData) =
                        mVolumeCurve.ConvertDBToScalar(theVolumeValue);

                // report how much we wrote
                outDataSize = sizeof(Float32);
            }
            break;

        default:
            BGM_Control::GetPropertyData(inObjectID,
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

void    BGM_VolumeControl::SetPropertyData(AudioObjectID inObjectID,
                                           pid_t inClientPID,
                                           const AudioObjectPropertyAddress& inAddress,
                                           UInt32 inQualifierDataSize,
                                           const void* inQualifierData,
                                           UInt32 inDataSize,
                                           const void* inData)
{
    CheckObjectID(inObjectID);

    switch(inAddress.mSelector)
    {
        case kAudioLevelControlPropertyScalarValue:
            // For the scalar volume, we clamp the new value to [0, 1]. Note that if this
            // value changes, it implies that the dB value changed too.
            {
                ThrowIf(inDataSize != sizeof(Float32),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_VolumeControl::SetPropertyData: wrong size for the data for "
                        "kAudioLevelControlPropertyScalarValue");

                // Read the new scalar volume and clamp it.
                Float32 theNewVolumeScalar = *reinterpret_cast<const Float32*>(inData);
                theNewVolumeScalar = std::min(1.0f, std::max(0.0f, theNewVolumeScalar));

                // Store the new volume.
                SInt32 theNewVolumeRaw = mVolumeCurve.ConvertScalarToRaw(theNewVolumeScalar);
                SetVolumeRaw(theNewVolumeRaw);
            }
            break;

        case kAudioLevelControlPropertyDecibelValue:
            // For the dB value, we first convert it to a raw value since that is how
            // the value is tracked. Note that if this value changes, it implies that the
            // scalar value changes as well.
            {
                ThrowIf(inDataSize != sizeof(Float32),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_VolumeControl::SetPropertyData: wrong size for the data for "
                        "kAudioLevelControlPropertyDecibelValue");

                // Read the new volume in dB and clamp it.
                Float32 theNewVolumeDb = *reinterpret_cast<const Float32*>(inData);
                theNewVolumeDb =
                        std::min(mMaxVolumeDb, std::max(mMinVolumeDb, theNewVolumeDb));

                // Store the new volume.
                SInt32 theNewVolumeRaw = mVolumeCurve.ConvertDBToRaw(theNewVolumeDb);
                SetVolumeRaw(theNewVolumeRaw);
            }
            break;

        default:
            BGM_Control::SetPropertyData(inObjectID,
                                         inClientPID,
                                         inAddress,
                                         inQualifierDataSize,
                                         inQualifierData,
                                         inDataSize,
                                         inData);
            break;
    };
}

void    BGM_VolumeControl::SetVolumeRaw(SInt32 inNewVolumeRaw)
{
    CAMutex::Locker theLocker(mMutex);

    // Make sure the new raw value is in the proper range.
    inNewVolumeRaw = std::min(std::max(mMinVolumeRaw, inNewVolumeRaw), mMaxVolumeRaw);

    // Store the new volume.
    if(mVolumeRaw != inNewVolumeRaw)
    {
        mVolumeRaw = inNewVolumeRaw;

        // Send notifications.
        CADispatchQueue::GetGlobalSerialQueue().Dispatch(false, ^{
            AudioObjectPropertyAddress theChangedProperties[2];
            theChangedProperties[0] = { kAudioLevelControlPropertyScalarValue, mScope, mElement };
            theChangedProperties[1] = { kAudioLevelControlPropertyDecibelValue, mScope, mElement };

            BGM_PlugIn::Host_PropertiesChanged(GetObjectID(), 2, theChangedProperties);
        });
    }
}

#pragma clang assume_nonnull end


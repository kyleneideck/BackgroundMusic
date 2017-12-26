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
#include "BGM_Utils.h"

// STL Includes
#include <algorithm>

// System Includes
#include <CoreAudio/AudioHardwareBase.h>
#include <Accelerate/Accelerate.h>


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
    mAmplitudeGain(0.0f),
    mMinVolumeRaw(kDefaultMinRawVolume),
    mMaxVolumeRaw(kDefaultMaxRawVolume),
    mMinVolumeDb(kDefaultMinDbVolume),
    mMaxVolumeDb(kDefaultMaxDbVolume),
    mWillApplyVolumeToAudio(false)
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
            {
                ThrowIf(inDataSize != sizeof(Float32),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_VolumeControl::SetPropertyData: wrong size for the data for "
                        "kAudioLevelControlPropertyScalarValue");

                // Read the new scalar volume.
                Float32 theNewVolumeScalar = *reinterpret_cast<const Float32*>(inData);
                SetVolumeScalar(theNewVolumeScalar);
            }
            break;

        case kAudioLevelControlPropertyDecibelValue:
            {
                ThrowIf(inDataSize != sizeof(Float32),
                        CAException(kAudioHardwareBadPropertySizeError),
                        "BGM_VolumeControl::SetPropertyData: wrong size for the data for "
                        "kAudioLevelControlPropertyDecibelValue");

                // Read the new volume in dB.
                Float32 theNewVolumeDb = *reinterpret_cast<const Float32*>(inData);
                SetVolumeDb(theNewVolumeDb);
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

#pragma mark Accessors

void    BGM_VolumeControl::SetVolumeScalar(Float32 inNewVolumeScalar)
{
    // For the scalar volume, we clamp the new value to [0, 1]. Note that if this value changes, it
    // implies that the dB value changes too.
    inNewVolumeScalar = std::min(1.0f, std::max(0.0f, inNewVolumeScalar));

    // Store the new volume.
    SInt32 theNewVolumeRaw = mVolumeCurve.ConvertScalarToRaw(inNewVolumeScalar);
    SetVolumeRaw(theNewVolumeRaw);
}

void    BGM_VolumeControl::SetVolumeDb(Float32 inNewVolumeDb)
{
    // For the dB value, we first convert it to a raw value since that is how the value is tracked.
    // Note that if this value changes, it implies that the scalar value changes as well.

    // Clamp the new volume.
    inNewVolumeDb = std::min(mMaxVolumeDb, std::max(mMinVolumeDb, inNewVolumeDb));

    // Store the new volume.
    SInt32 theNewVolumeRaw = mVolumeCurve.ConvertDBToRaw(inNewVolumeDb);
    SetVolumeRaw(theNewVolumeRaw);
}

void    BGM_VolumeControl::SetWillApplyVolumeToAudio(bool inWillApplyVolumeToAudio)
{
    mWillApplyVolumeToAudio = inWillApplyVolumeToAudio;
}

#pragma mark IO Operations

bool    BGM_VolumeControl::WillApplyVolumeToAudioRT() const
{
    return mWillApplyVolumeToAudio;
}

void    BGM_VolumeControl::ApplyVolumeToAudioRT(Float32* ioBuffer, UInt32 inBufferFrameSize) const
{
    ThrowIf(!mWillApplyVolumeToAudio,
            CAException(kAudioHardwareIllegalOperationError),
            "BGM_VolumeControl::ApplyVolumeToAudioRT: This control doesn't process audio data");

    // Don't bother if the change is very unlikely to be perceptible.
    if((mAmplitudeGain < 0.99f) || (mAmplitudeGain > 1.01f))
    {
        // Apply the amount of gain/loss for the current volume to the audio signal by multiplying
        // each sample. This call to vDSP_vsmul is equivalent to
        //
        // for(UInt32 i = 0; i < inBufferFrameSize * 2; i++)
        // {
        //     ioBuffer[i] *= mAmplitudeGain;
        // }
        //
        // but a bit faster on processors with newer SIMD instructions. However, it shouldn't take
        // more than a few microseconds either way. (Unless some of the samples were subnormal
        // numbers for some reason.)
        //
        // It would be a tiny bit faster still to not do this in-place, i.e. use separate input and
        // output buffers, but then we'd have to copy the data into the output buffer when the
        // volume is at 1.0. With our current use of this class, most people will leave the volume
        // at 1.0, so it wouldn't be worth it.
        vDSP_vsmul(ioBuffer, 1, &mAmplitudeGain, ioBuffer, 1, inBufferFrameSize * 2);
    }
}

#pragma mark Implementation

void    BGM_VolumeControl::SetVolumeRaw(SInt32 inNewVolumeRaw)
{
    CAMutex::Locker theLocker(mMutex);

    // Make sure the new raw value is in the proper range.
    inNewVolumeRaw = std::min(std::max(mMinVolumeRaw, inNewVolumeRaw), mMaxVolumeRaw);

    // Store the new volume.
    if(mVolumeRaw != inNewVolumeRaw)
    {
        mVolumeRaw = inNewVolumeRaw;

        // CAVolumeCurve deals with volumes in three different scales: scalar, dB and raw. Raw
        // volumes are the number of steps along the dB curve, so dB and raw volumes are linearly
        // related.
        //
        // macOS uses the scalar volume to set the position of its volume sliders for the
        // device. We have to set the scalar volume to the position of our volume slider for a
        // device (more specifically, a linear mapping of it onto [0,1]) or macOS's volume sliders
        // or it will work differently to our own.
        //
        // When we set a new slider position as the device's scalar volume, we convert it to raw
        // with CAVolumeCurve::ConvertScalarToRaw, which will "undo the curve". However, we haven't
        // applied the curve at that point.
        //
        // So, to actually apply the curve, we use CAVolumeCurve::ConvertRawToScalar to get the
        // linear slider position back, map it onto the range of raw volumes and use
        // CAVolumeCurve::ConvertRawToScalar again to apply the curve.
        //
        // It might be that we should be using CAVolumeCurve with transfer functions x^n where
        // 0 < n < 1, but a lot more of the transfer functions it supports have n >= 1, including
        // the default one. So I'm a bit confused.
        //
        // TODO: I think this means the dB volume we report will be wrong. It also makes the code
        //       pretty confusing.
        Float32 theSliderPosition = mVolumeCurve.ConvertRawToScalar(mVolumeRaw);

        // TODO: This assumes the control should never boost the signal. (So, technically, it never
        //       actually applies gain, only loss.)
        SInt32 theRawRange = mMaxVolumeRaw - mMinVolumeRaw;
        SInt32 theSliderPositionInRawSteps = static_cast<SInt32>(theSliderPosition * theRawRange);
        theSliderPositionInRawSteps += mMinVolumeRaw;

        mAmplitudeGain = mVolumeCurve.ConvertRawToScalar(theSliderPositionInRawSteps);

        BGMAssert((mAmplitudeGain >= 0.0f) && (mAmplitudeGain <= 1.0f), "Gain not in [0,1]");

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


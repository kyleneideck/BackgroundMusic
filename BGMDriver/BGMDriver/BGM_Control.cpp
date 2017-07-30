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
//  BGM_Control.cpp
//  BGMDriver
//
//  Copyright © 2017 Kyle Neideck
//  Portions copyright (C) 2013 Apple Inc. All Rights Reserved.
//

// Self Include
#include "BGM_Control.h"

// PublicUtility Includes
#include "CADebugMacros.h"
#include "CAException.h"

// System Includes
#include <CoreAudio/AudioHardwareBase.h>


#pragma clang assume_nonnull begin

BGM_Control::BGM_Control(AudioObjectID inObjectID,
                         AudioClassID inClassID,
                         AudioClassID inBaseClassID,
                         AudioObjectID inOwnerObjectID,
                         AudioObjectPropertyScope inScope,
                         AudioObjectPropertyElement inElement)
:
    BGM_Object(inObjectID, inClassID, inBaseClassID, inOwnerObjectID),
    mScope(inScope),
    mElement(inElement)
{
}

bool    BGM_Control::HasProperty(AudioObjectID inObjectID,
                                 pid_t inClientPID,
                                 const AudioObjectPropertyAddress& inAddress) const
{
    CheckObjectID(inObjectID);

    bool theAnswer = false;

    switch(inAddress.mSelector)
    {
        case kAudioControlPropertyScope:
        case kAudioControlPropertyElement:
            theAnswer = true;
            break;

        default:
            theAnswer = BGM_Object::HasProperty(inObjectID, inClientPID, inAddress);
            break;
    };

    return theAnswer;
}

bool    BGM_Control::IsPropertySettable(AudioObjectID inObjectID,
                                        pid_t inClientPID,
                                        const AudioObjectPropertyAddress& inAddress) const
{
    CheckObjectID(inObjectID);

    bool theAnswer = false;

    switch(inAddress.mSelector)
    {
        case kAudioControlPropertyScope:
        case kAudioControlPropertyElement:
            theAnswer = false;
            break;

        default:
            theAnswer = BGM_Object::IsPropertySettable(inObjectID, inClientPID, inAddress);
            break;
    };

    return theAnswer;
}

UInt32  BGM_Control::GetPropertyDataSize(AudioObjectID inObjectID,
                                         pid_t inClientPID,
                                         const AudioObjectPropertyAddress& inAddress,
                                         UInt32 inQualifierDataSize,
                                         const void* inQualifierData) const
{
    CheckObjectID(inObjectID);

    UInt32 theAnswer = 0;

    switch(inAddress.mSelector)
    {
        case kAudioControlPropertyScope:
            theAnswer = sizeof(AudioObjectPropertyScope);
            break;

        case kAudioControlPropertyElement:
            theAnswer = sizeof(AudioObjectPropertyElement);
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

void    BGM_Control::GetPropertyData(AudioObjectID inObjectID,
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
        case kAudioControlPropertyScope:
            // This property returns the scope that the control is attached to.
            ThrowIf(inDataSize < sizeof(AudioObjectPropertyScope),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_Control::GetPropertyData: not enough space for the return value of "
                    "kAudioControlPropertyScope for the control");
            *reinterpret_cast<AudioObjectPropertyScope*>(outData) = mScope;
            outDataSize = sizeof(AudioObjectPropertyScope);
            break;

        case kAudioControlPropertyElement:
            // This property returns the element that the control is attached to.
            ThrowIf(inDataSize < sizeof(AudioObjectPropertyElement),
                    CAException(kAudioHardwareBadPropertySizeError),
                    "BGM_Control::GetPropertyData: not enough space for the return value of "
                    "kAudioControlPropertyElement for the control");
            *reinterpret_cast<AudioObjectPropertyElement*>(outData) = mElement;
            outDataSize = sizeof(AudioObjectPropertyElement);
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

void    BGM_Control::CheckObjectID(AudioObjectID inObjectID) const
{
    ThrowIf(inObjectID == kAudioObjectUnknown || inObjectID != GetObjectID(),
            CAException(kAudioHardwareBadObjectError),
            "BGM_Control::CheckObjectID: wrong audio object ID for the control");
}

#pragma clang assume_nonnull end


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
//  BGM_VolumeControl.h
//  BGMDriver
//
//  Copyright © 2017 Kyle Neideck
//

#ifndef BGMDriver__BGM_VolumeControl
#define BGMDriver__BGM_VolumeControl

// Superclass Includes
#include "BGM_Control.h"

// PublicUtility Includes
#include "CAVolumeCurve.h"
#include "CAMutex.h"


#pragma clang assume_nonnull begin

class BGM_VolumeControl
:
    public BGM_Control
{

#pragma mark Construction/Destruction

public:
                        BGM_VolumeControl(AudioObjectID inObjectID,
                                          AudioObjectID inOwnerObjectID,
                                          AudioObjectPropertyScope inScope,
                                          AudioObjectPropertyElement inElement =
                                                  kAudioObjectPropertyElementMaster);

#pragma mark Property Operations

    virtual bool        HasProperty(AudioObjectID inObjectID,
                                    pid_t inClientPID,
                                    const AudioObjectPropertyAddress& inAddress) const;
    virtual bool        IsPropertySettable(AudioObjectID inObjectID,
                                           pid_t inClientPID,
                                           const AudioObjectPropertyAddress& inAddress) const;
    virtual UInt32      GetPropertyDataSize(AudioObjectID inObjectID,
                                            pid_t inClientPID,
                                            const AudioObjectPropertyAddress& inAddress,
                                            UInt32 inQualifierDataSize,
                                            const void* inQualifierData) const;
    virtual void        GetPropertyData(AudioObjectID inObjectID,
                                        pid_t inClientPID,
                                        const AudioObjectPropertyAddress& inAddress,
                                        UInt32 inQualifierDataSize,
                                        const void* inQualifierData,
                                        UInt32 inDataSize,
                                        UInt32& outDataSize,
                                        void* outData) const;
    virtual void        SetPropertyData(AudioObjectID inObjectID,
                                        pid_t inClientPID,
                                        const AudioObjectPropertyAddress& inAddress,
                                        UInt32 inQualifierDataSize,
                                        const void* inQualifierData,
                                        UInt32 inDataSize,
                                        const void* inData);

#pragma mark Implementation

protected:
    void                SetVolumeRaw(SInt32 inNewVolumeRaw);

private:
    const SInt32        kDefaultMinRawVolume = 0;
    const SInt32        kDefaultMaxRawVolume = 96;
    const Float32       kDefaultMinDbVolume  = -96.0f;
    const Float32       kDefaultMaxDbVolume  = 0.0f;

    CAMutex             mMutex;

    SInt32              mVolumeRaw;
    SInt32              mMinVolumeRaw;
    SInt32              mMaxVolumeRaw;
    Float32             mMinVolumeDb;
    Float32             mMaxVolumeDb;

    CAVolumeCurve       mVolumeCurve;

};

#pragma clang assume_nonnull end

#endif /* BGMDriver__BGM_VolumeControl */


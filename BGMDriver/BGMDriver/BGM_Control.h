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
//  BGM_Control.h
//  BGMDriver
//
//  Copyright © 2017 Kyle Neideck
//
//  An AudioObject that represents a user-controllable aspect of a device or stream, such as volume
//  or balance.
//

#ifndef BGMDriver__BGM_Control
#define BGMDriver__BGM_Control

// Superclass Includes
#include "BGM_Object.h"


#pragma clang assume_nonnull begin

class BGM_Control
:
    public BGM_Object
{

protected:
                        BGM_Control(AudioObjectID inObjectID,
                                    AudioClassID inClassID,
                                    AudioClassID inBaseClassID,
                                    AudioObjectID inOwnerObjectID,
                                    AudioObjectPropertyScope inScope =
                                            kAudioObjectPropertyScopeOutput,
                                    AudioObjectPropertyElement inElement =
                                            kAudioObjectPropertyElementMaster);

#pragma mark Property Operations

public:
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

#pragma mark Implementation

protected:
    void                CheckObjectID(AudioObjectID inObjectID) const;

protected:
    const AudioObjectPropertyScope      mScope;
    const AudioObjectPropertyElement    mElement;

};

#pragma clang assume_nonnull end

#endif /* BGMDriver__BGM_Control */


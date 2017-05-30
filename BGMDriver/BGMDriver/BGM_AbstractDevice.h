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
//  BGM_AbstractDevice.h
//  BGMDriver
//
//  Copyright © 2017 Kyle Neideck
//

#ifndef BGM_Driver__BGM_AbstractDevice
#define BGM_Driver__BGM_AbstractDevice

// SuperClass Includes
#include "BGM_Object.h"


#pragma clang assume_nonnull begin

class BGM_AbstractDevice
:
    public BGM_Object
{

#pragma mark Construction/Destruction

protected:
                                BGM_AbstractDevice(AudioObjectID inObjectID,
                                                   AudioObjectID inOwnerObjectID);
    virtual                     ~BGM_AbstractDevice();

#pragma mark Property Operations

public:
    virtual bool                HasProperty(AudioObjectID inObjectID,
                                            pid_t inClientPID,
                                            const AudioObjectPropertyAddress& inAddress) const;
    virtual bool                IsPropertySettable(AudioObjectID inObjectID,
                                                   pid_t inClientPID,
                                                   const AudioObjectPropertyAddress& inAddress) const;
    virtual UInt32              GetPropertyDataSize(AudioObjectID inObjectID,
                                                    pid_t inClientPID,
                                                    const AudioObjectPropertyAddress& inAddress,
                                                    UInt32 inQualifierDataSize,
                                                    const void* __nullable inQualifierData) const;
    virtual void                GetPropertyData(AudioObjectID inObjectID,
                                                pid_t inClientPID,
                                                const AudioObjectPropertyAddress& inAddress,
                                                UInt32 inQualifierDataSize,
                                                const void* __nullable inQualifierData,
                                                UInt32 inDataSize,
                                                UInt32& outDataSize,
                                                void* outData) const;

#pragma mark IO Operations

public:
    virtual void                StartIO(UInt32 inClientID) = 0;
    virtual void                StopIO(UInt32 inClientID) = 0;

    virtual void                GetZeroTimeStamp(Float64& outSampleTime,
                                                 UInt64& outHostTime,
                                                 UInt64& outSeed) = 0;

    virtual void                WillDoIOOperation(UInt32 inOperationID,
                                                  bool& outWillDo,
                                                  bool& outWillDoInPlace) const = 0;
    virtual void                BeginIOOperation(UInt32 inOperationID,
                                                 UInt32 inIOBufferFrameSize,
                                                 const AudioServerPlugInIOCycleInfo& inIOCycleInfo,
                                                 UInt32 inClientID) = 0;
    virtual void                DoIOOperation(AudioObjectID inStreamObjectID,
                                              UInt32 inClientID, UInt32 inOperationID,
                                              UInt32 inIOBufferFrameSize,
                                              const AudioServerPlugInIOCycleInfo& inIOCycleInfo,
                                              void* ioMainBuffer,
                                              void* __nullable ioSecondaryBuffer) = 0;
    virtual void                EndIOOperation(UInt32 inOperationID,
                                               UInt32 inIOBufferFrameSize,
                                               const AudioServerPlugInIOCycleInfo& inIOCycleInfo,
                                               UInt32 inClientID) = 0;

#pragma mark Implementation

public:
    virtual CFStringRef         CopyDeviceUID() const = 0;
    virtual void                AddClient(const AudioServerPlugInClientInfo* inClientInfo) = 0;
    virtual void                RemoveClient(const AudioServerPlugInClientInfo* inClientInfo) = 0;
    virtual void                PerformConfigChange(UInt64 inChangeAction,
                                                    void* __nullable inChangeInfo) = 0;
    virtual void                AbortConfigChange(UInt64 inChangeAction,
                                                  void* __nullable inChangeInfo) = 0;
    
};

#pragma clang assume_nonnull end

#endif /* BGM_Driver__BGM_AbstractDevice */


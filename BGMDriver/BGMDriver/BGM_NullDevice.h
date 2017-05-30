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
//  BGM_NullDevice.h
//  BGMDriver
//
//  Copyright © 2017 Kyle Neideck
//
//  A device with one output stream that ignores any audio played on that stream.
//
//  If we change BGMDevice's controls list, to match the output device set in BGMApp, we need to
//  change the OS X default device so other programs (including the OS X audio UI) will update
//  themselves. We could just change to the real output device and change back, but that could have
//  side effects the user wouldn't expect. For example, an app the user had muted might be unmuted
//  for a short period.
//
//  Instead, BGMApp temporarily enables this device and uses it to toggle the default device. This
//  device is disabled at all other times so it can be hidden from the user. (We can't just use
//  kAudioDevicePropertyIsHidden because hidden devices can't be default and the HAL doesn't seem to
//  let devices change kAudioDevicePropertyIsHidden after setting it initially.)
//
//  It might be worth eventually having a virtual device for each real output device, but this is
//  simpler and seems to work well enough for now.
//

#ifndef BGMDriver__BGM_NullDevice
#define BGMDriver__BGM_NullDevice

// SuperClass Includes
#include "BGM_AbstractDevice.h"

// Local Includes
#include "BGM_Types.h"
#include "BGM_Stream.h"

// PublicUtility Includes
#include "CAMutex.h"

// System Includes
#include <pthread.h>


#pragma clang assume_nonnull begin

class BGM_NullDevice
:
    public BGM_AbstractDevice
{

#pragma mark Construction/Destruction

public:
    static BGM_NullDevice&      GetInstance();

private:
    static void                 StaticInitializer();

protected:
                                BGM_NullDevice();
    virtual                     ~BGM_NullDevice();

public:
    virtual void                Activate();
    virtual void                Deactivate();

private:
    void                        SendDeviceIsAlivePropertyNotifications();

#pragma mark Property Operations

public:
    bool                        HasProperty(AudioObjectID inObjectID,
                                            pid_t inClientPID,
                                            const AudioObjectPropertyAddress& inAddress) const;
    bool                        IsPropertySettable(AudioObjectID inObjectID,
                                                   pid_t inClientPID,
                                                   const AudioObjectPropertyAddress& inAddress) const;
    UInt32                      GetPropertyDataSize(AudioObjectID inObjectID,
                                                    pid_t inClientPID,
                                                    const AudioObjectPropertyAddress& inAddress,
                                                    UInt32 inQualifierDataSize,
                                                    const void* __nullable inQualifierData) const;
    void                        GetPropertyData(AudioObjectID inObjectID,
                                                pid_t inClientPID,
                                                const AudioObjectPropertyAddress& inAddress,
                                                UInt32 inQualifierDataSize,
                                                const void* __nullable inQualifierData,
                                                UInt32 inDataSize,
                                                UInt32& outDataSize,
                                                void* outData) const;
    void                        SetPropertyData(AudioObjectID inObjectID,
                                                pid_t inClientPID,
                                                const AudioObjectPropertyAddress& inAddress,
                                                UInt32 inQualifierDataSize,
                                                const void* inQualifierData,
                                                UInt32 inDataSize,
                                                const void* inData);

#pragma mark IO Operations

public:
    void                        StartIO(UInt32 inClientID);
    void                        StopIO(UInt32 inClientID);

    void                        GetZeroTimeStamp(Float64& outSampleTime,
                                                 UInt64& outHostTime,
                                                 UInt64& outSeed);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
    void                        WillDoIOOperation(UInt32 inOperationID,
                                                  bool& outWillDo,
                                                  bool& outWillDoInPlace) const;
    void                        BeginIOOperation(UInt32 inOperationID,
                                                 UInt32 inIOBufferFrameSize,
                                                 const AudioServerPlugInIOCycleInfo& inIOCycleInfo,
                                                 UInt32 inClientID)
                                    { /* No-op */ };
    void                        DoIOOperation(AudioObjectID inStreamObjectID,
                                              UInt32 inClientID,
                                              UInt32 inOperationID,
                                              UInt32 inIOBufferFrameSize,
                                              const AudioServerPlugInIOCycleInfo& inIOCycleInfo,
                                              void* ioMainBuffer,
                                              void* __nullable ioSecondaryBuffer);
    void                        EndIOOperation(UInt32 inOperationID,
                                               UInt32 inIOBufferFrameSize,
                                               const AudioServerPlugInIOCycleInfo& inIOCycleInfo,
                                               UInt32 inClientID)
                                    { /* No-op */ };

#pragma mark Implementation

public:
    CFStringRef                 CopyDeviceUID() const
                                    { return CFSTR(kBGMNullDeviceUID); };

    void                        AddClient(const AudioServerPlugInClientInfo* inClientInfo)
                                    { /* No-op */ };
    void                        RemoveClient(const AudioServerPlugInClientInfo* inClientInfo)
                                    { /* No-op */ };
    void                        PerformConfigChange(UInt64 inChangeAction,
                                                    void* __nullable inChangeInfo)
                                    { /* No-op */ };
    void                        AbortConfigChange(UInt64 inChangeAction,
                                                  void* __nullable inChangeInfo)
                                    { /* No-op */ };
#pragma clang diagnostic pop

private:
    static pthread_once_t       sStaticInitializer;
    static BGM_NullDevice*      sInstance;

    #define kNullDeviceName     "Background Music Null Device"
    #define kNullDeviceManufacturerName \
                                "Background Music contributors"

    CAMutex                     mStateMutex;
    CAMutex                     mIOMutex;

    BGM_Stream                  mStream;

    UInt32                      mClientsDoingIO    = 0;

    Float64                     mHostTicksPerFrame = 0.0;
    UInt64                      mNumberTimeStamps  = 0;
    UInt64                      mAnchorHostTime    = 0;

};

#pragma clang assume_nonnull end

#endif /* BGMDriver__BGM_NullDevice */


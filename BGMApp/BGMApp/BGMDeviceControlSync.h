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
//  BGMDeviceControlSync.h
//  BGMApp
//
//  Copyright © 2016, 2017 Kyle Neideck
//
//  Synchronises BGMDevice's controls (just volume and mute currently) with the output device's
//  controls. This allows the user to control the output device normally while BGMDevice is set as
//  the default device.
//
//  BGMDeviceControlSync disables any BGMDevice controls that the output device doesn't also have.
//  When the value of one of BGMDevice's controls is changed, BGMDeviceControlSync copies the new
//  value to the output device.
//
//  Thread safe.
//

#ifndef BGMApp__BGMDeviceControlSync
#define BGMApp__BGMDeviceControlSync

// Local Includes
#include "BGMAudioDevice.h"
#include "BGMDeviceControlsList.h"

// PublicUtility Includes
#include "CAHALAudioSystemObject.h"
#include "CAMutex.h"

// System Includes
#include <AudioToolbox/AudioServices.h>


#pragma clang assume_nonnull begin

class BGMDeviceControlSync
{

#pragma mark Construction/Destruction

public:
                        BGMDeviceControlSync(AudioObjectID inBGMDevice,
                                             AudioObjectID inOutputDevice,
                                             CAHALAudioSystemObject inAudioSystem
                                                     = CAHALAudioSystemObject());
                        ~BGMDeviceControlSync();
                        // Disallow copying
                        BGMDeviceControlSync(const BGMDeviceControlSync&) = delete;
                        BGMDeviceControlSync& operator=(const BGMDeviceControlSync&) = delete;

#ifdef __OBJC__
                        // Only intended as a convenience for Objective-C instance vars
                        BGMDeviceControlSync()
                        : BGMDeviceControlSync(kAudioObjectUnknown, kAudioObjectUnknown) { };
#endif

    /*!
     Begin synchronising BGMDevice's controls with the output device's.

     @throws BGM_DeviceNotSetException if BGMDevice isn't set.
     @throws CAException if the HAL or one of the devices returns an error when this function
                         registers for device property notifications or when it copies the current
                         values of the output device's controls to BGMDevice. This
                         BGMDeviceControlSync will remain inactive if this function throws.
     */
    void                Activate();
    /*! Stop synchronising BGMDevice's controls with the output device's. */
    void                Deactivate();

#pragma mark Accessors

    /*!
     Set the IDs of BGMDevice and the output device to synchronise with.

     @throws BGM_DeviceNotSetException if BGMDevice isn't set.
     @throws CAException if the HAL or one of the new devices returns an error while restarting
                         synchronisation. This BGMDeviceControlSync will be deactivated if this
                         function throws, but its devices will still be set.
     */
    void                SetDevices(AudioObjectID inBGMDevice, AudioObjectID inOutputDevice);

#pragma mark Listener Procs
    
private:
    /*! Receives HAL notifications about the BGMDevice properties this class listens to. */
    static OSStatus     BGMDeviceListenerProc(AudioObjectID inObjectID,
                                              UInt32 inNumberAddresses,
                                              const AudioObjectPropertyAddress* inAddresses,
                                              void* __nullable inClientData);
    
private:
    CAMutex             mMutex         { "Device Control Sync" };
    bool                mActive        = false;

    CAHALAudioSystemObject mAudioSystem;
    
    BGMAudioDevice      mBGMDevice     { (AudioObjectID)kAudioObjectUnknown };
    BGMAudioDevice      mOutputDevice  { (AudioObjectID)kAudioObjectUnknown };

    BGMDeviceControlsList mBGMDeviceControlsList;
    
};

#pragma clang assume_nonnull end

#endif /* BGMApp__BGMDeviceControlSync */


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
//  BGMDeviceControlsList.h
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//

#ifndef BGMApp__BGMDeviceControlsList
#define BGMApp__BGMDeviceControlsList

// Local Includes
#include "BGMAudioDevice.h"

// PublicUtility Includes
#include "CAHALAudioDevice.h"
#include "CAHALAudioSystemObject.h"
#include "CAMutex.h"

// System Includes
#include <dispatch/dispatch.h>
#include <AudioToolbox/AudioServices.h>


#pragma clang assume_nonnull begin

class BGMDeviceControlsList
{

#pragma mark Construction/Destruction

public:

                        BGMDeviceControlsList(AudioObjectID inBGMDevice,
                                              CAHALAudioSystemObject inAudioSystem
                                                      = CAHALAudioSystemObject());
                        ~BGMDeviceControlsList();
                        // Disallow copying
                        BGMDeviceControlsList(const BGMDeviceControlsList&) = delete;
                        BGMDeviceControlsList& operator=(const BGMDeviceControlsList&) = delete;

#pragma mark Accessors

    /*! @param inBGMDeviceID The ID of BGMDevice. */
    void                SetBGMDevice(AudioObjectID inBGMDeviceID);

#pragma mark Update Controls List

    /*!
     Enable the BGMDevice controls (volume and mute currently) that can be matched to controls of
     the given device, and disable the ones that can't.

     @param inDeviceID The ID of the device.
     @return True if BGMDevice's list of controls was updated.
     @throws CAException if an error is received from either device.
     */
    bool                MatchControlsListOf(AudioObjectID inDeviceID);
    /*!
     After updating BGMDevice's controls list, we need to change the default device so programs
     (including OS X's audio UI) will update themselves. We could just change to the real output
     device and change back, but that could have side effects the user wouldn't expect. For example,
     an app the user has muted might be unmuted for a short period.

     Instead we tell BGMDriver to enable the Null Device -- a device that does nothing -- so we can
     use it to toggle the default device. The Null Device is normally disabled so it can be hidden
     from the user. OS X won't let us make a hidden device temporarily visible or set a hidden
     device as the default, so we have to completely remove the Null Device from the system while
     we're not using it.
     
     @throws CAException if it fails to enable the Null Device.
     */
    void                PropagateControlListChange();

#pragma mark Implementation

private:
    /*! Lazily initialises the fields used to toggle the default device. */
    void                InitDeviceToggling();
    /*! Changes the OS X default audio device to the Null Device and then back to BGMDevice. */
    void                ToggleDefaultDevice();
    /*!
     Enable or disable the Null Device. See PropagateControlListChange and BGM_NullDevice in 
     BGMDriver.

     @throws CAException if we can't get the BGMDriver plug-in audio object from the HAL or the HAL
                         returns an error when setting kAudioPlugInCustomPropertyNullDeviceActive.
     */
    void                SetNullDeviceEnabled(bool inEnabled);

    dispatch_block_t __nullable CreateDeviceToggleBlock();
    dispatch_block_t __nullable CreateDeviceToggleBackBlock();
    dispatch_block_t __nullable CreateDisableNullDeviceBlock();

    void                DestroyBlock(dispatch_block_t __nullable & block);

private:
    CAMutex             mMutex { "Device Controls List" };
    bool                mDeviceTogglingInitialised = false;
    // OS X 10.9 doesn't have the functions we use for PropagateControlListChange.
    bool                mCanToggleDeviceOnSystem;

    BGMAudioDevice      mBGMDevice;
    CAHALAudioSystemObject mAudioSystem;  // Not guarded by the mutex.

    enum ToggleState
    {
        NotToggling, SettingNullDeviceAsDefault, SettingBGMDeviceAsDefault, DisablingNullDevice
    };
    BGMDeviceControlsList::ToggleState mDeviceToggleState = ToggleState::NotToggling;

    dispatch_block_t __nullable mDeviceToggleBlock      = nullptr;
    dispatch_block_t __nullable mDeviceToggleBackBlock  = nullptr;
    dispatch_block_t __nullable mDisableNullDeviceBlock = nullptr;

    // These will only ever be null after construction on 10.9, since toggling will be disabled.
    dispatch_queue_t __nullable                 mListenerQueue = nullptr;
    AudioObjectPropertyListenerBlock __nullable mListenerBlock = nullptr;

};

#pragma clang assume_nonnull end

#endif /* BGMApp__BGMDeviceControlsList */


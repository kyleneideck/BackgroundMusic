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
//  BGMVolumeChangeListener.cpp
//  BGMApp
//
//  Copyright © 2019 Kyle Neideck
//

// Self Include
#include "BGMVolumeChangeListener.h"

// Local Includes
#import "BGM_Utils.h"
#import "BGMAudioDevice.h"

// PublicUtility Includes
#import "CAException.h"
#import "CAPropertyAddress.h"


#pragma clang assume_nonnull begin

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
const static std::vector<CAPropertyAddress> kVolumeChangeProperties = {
        // Output volume changes
        CAPropertyAddress(kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput),
        // Mute/unmute
        CAPropertyAddress(kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput),
        // Received when controls are added to or removed from the device.
        CAPropertyAddress(kAudioObjectPropertyControlList),
        // Received when the device has changed and "clients should re-evaluate everything they need
        // to know about the device, particularly the layout and values of the controls".
        CAPropertyAddress(kAudioDevicePropertyDeviceHasChanged)
};
#pragma clang diagnostic pop

BGMVolumeChangeListener::BGMVolumeChangeListener(BGMAudioDevice device,
                                                 std::function<void(void)> handler)
:
    mDevice(device)
{
    // Register a listener that will update the slider when the user changes the volume or
    // mutes/unmutes their audio.
    mListenerBlock =
            Block_copy(^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress* inAddresses) {
                // The docs for AudioObjectPropertyListenerBlock say inAddresses will always contain
                // at least one property the block is listening to, so there's no need to check it.
                (void)inNumberAddresses;
                (void)inAddresses;

                // Call the callback.
                handler();
            });

    // Register for a number of properties that might indicate that clients need to update. For
    // example, the mute property changing means UI elements that display the volume will need to be
    // updated, even though it's not strictly a change in volume.
    for(CAPropertyAddress property : kVolumeChangeProperties)
    {
        // Instead of swallowing exceptions here, we could try again later, but I doubt it would be
        // worth the effort. And the documentation doesn't actually explain what could cause this
        // call to fail.
        BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
            mDevice.AddPropertyListenerBlock(property, dispatch_get_main_queue(), mListenerBlock);
        });
    }
}

BGMVolumeChangeListener::~BGMVolumeChangeListener()
{
    // Deregister and release the listener block.
    for(CAPropertyAddress property : kVolumeChangeProperties)
    {
        BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
            mDevice.RemovePropertyListenerBlock(property,
                                                dispatch_get_main_queue(),
                                                mListenerBlock);
        });
    }

    Block_release(mListenerBlock);
}

#pragma clang assume_nonnull end


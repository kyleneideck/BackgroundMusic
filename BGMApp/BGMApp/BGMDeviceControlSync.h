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
//  Copyright © 2016, 2017, 2026 Kyle Neideck
//
//  Normalises the output devices' volume controls so BGMApp's software volume is the
//  authoritative one. BGMApp applies the master volume and each output device's relative volume
//  in its play-through IOProcs (see BGMPlayThrough), so the output devices' hardware volume
//  controls are set to full (and unmuted) when they're activated, to avoid attenuating the audio
//  a second time.
//
//  Thread safe.
//

#ifndef BGMApp__BGMDeviceControlSync
#define BGMApp__BGMDeviceControlSync

// Local Includes
#include "BGMAudioDevice.h"

// PublicUtility Includes
#include "CAMutex.h"

// STL Includes
#include <vector>


#pragma clang assume_nonnull begin

class BGMDeviceControlSync
{

#pragma mark Construction/Destruction

public:
                        BGMDeviceControlSync(AudioObjectID inBGMDevice,
                                             const std::vector<BGMAudioDevice>& inOutputDevices);
                        ~BGMDeviceControlSync();
                        // Disallow copying
                        BGMDeviceControlSync(const BGMDeviceControlSync&) = delete;
                        BGMDeviceControlSync& operator=(const BGMDeviceControlSync&) = delete;

#ifdef __OBJC__
                        // Only intended as a convenience for Objective-C instance vars
                        BGMDeviceControlSync()
                        : BGMDeviceControlSync(kAudioObjectUnknown, std::vector<BGMAudioDevice>()) { };
#endif

    /*!
     Set the output devices' volume controls to full and unmute them.

     @throws BGM_DeviceNotSetException if BGMDevice isn't set.
     @throws CAException if the HAL returns an error.
     */
    void                Activate();
    /*! No-op. Kept for API symmetry with Activate. */
    void                Deactivate();

#pragma mark Accessors

    /*!
     Set the IDs of BGMDevice and the output devices to normalise.

     @throws BGM_DeviceNotSetException if BGMDevice isn't set.
     */
    void                SetDevices(AudioObjectID inBGMDevice,
                                   const std::vector<BGMAudioDevice>& inOutputDevices);

private:
    CAMutex             mMutex { "BGMDeviceControlSync" };
    
    AudioObjectID       mBGMDevice;
    std::vector<BGMAudioDevice> mOutputDevices;
    
    bool                mActive = false;

};

#pragma clang assume_nonnull end

#endif /* BGMApp__BGMDeviceControlSync */

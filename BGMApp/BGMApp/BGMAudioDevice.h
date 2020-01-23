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

//  BGMAudioDevice.h
//  BGMApp
//
//  Copyright © 2017, 2020 Kyle Neideck
//
//  A HAL audio device. Note that this class's only state is the AudioObjectID of the device.
//

#ifndef BGMApp__BGMAudioDevice
#define BGMApp__BGMAudioDevice

// PublicUtility Includes
#include "CAHALAudioDevice.h"


class BGMAudioDevice
:
    public CAHALAudioDevice
{

#pragma mark Construction/Destruction

public:
                       BGMAudioDevice(AudioObjectID inAudioDevice);
    /*!
     Creates a BGMAudioDevice with the Audio Object ID of the device whose UID is inUID or, if no
     such device is found, kAudioObjectUnknown.

     @throws CAException If the HAL returns an error when queried for the device's ID.
     @see kAudioPlugInPropertyTranslateUIDToDevice in AudioHardwareBase.h.
     */
                       BGMAudioDevice(CFStringRef inUID);
                       BGMAudioDevice(const CAHALAudioDevice& inDevice);
    virtual            ~BGMAudioDevice();

#if defined(__OBJC__)

    // Hack/workaround for Objective-C classes so we don't have to use pointers for instance
    // variables.
                       BGMAudioDevice() : BGMAudioDevice(kAudioObjectUnknown) { }

#endif /* defined(__OBJC__) */

                       operator AudioObjectID() const { return GetObjectID(); }

    /*!
     @return True if this device is BGMDevice. (Specifically, the main instance of BGMDevice, not
             the instance used for UI sounds.)
     @throws CAException If the HAL returns an error when queried.
     */
    bool               IsBGMDevice() const { return IsBGMDevice(false); };
    /*!
     @return True if this device is either the main instance of BGMDevice (the device named
             "Background Music") or the instance used for UI sounds (the device named "Background
             Music (UI Sounds)").
     @throws CAException If the HAL returns an error when queried.
     */
    bool               IsBGMDeviceInstance() const { return IsBGMDevice(true); };

    /*!
     @return True if this device can be set as the output device in BGMApp.
     @throws CAException If the HAL returns an error when queried.
     */
    bool               CanBeOutputDeviceInBGMApp() const;

#pragma mark Available Controls

    bool               HasSettableMasterVolume(AudioObjectPropertyScope inScope) const;
    bool               HasSettableVirtualMasterVolume(AudioObjectPropertyScope inScope) const;
    bool               HasSettableMasterMute(AudioObjectPropertyScope inScope) const;

#pragma mark Control Values Accessors

    void               CopyMuteFrom(const BGMAudioDevice inDevice,
                                    AudioObjectPropertyScope inScope);
    void               CopyVolumeFrom(const BGMAudioDevice inDevice,
                                      AudioObjectPropertyScope inScope);

    bool               SetMasterVolumeScalar(AudioObjectPropertyScope inScope, Float32 inVolume);
    
    bool               GetVirtualMasterVolumeScalar(AudioObjectPropertyScope inScope,
                                                    Float32& outVirtualMasterVolume) const;
    bool               SetVirtualMasterVolumeScalar(AudioObjectPropertyScope inScope,
                                                    Float32 inVolume);

    bool               GetVirtualMasterBalance(AudioObjectPropertyScope inScope,
                                               Float32& outVirtualMasterBalance) const;

#pragma mark Implementation

private:
    bool               IsBGMDevice(bool inIncludingUISoundsInstance) const;

    static OSStatus    AHSGetPropertyData(AudioObjectID inObjectID,
                                          const AudioObjectPropertyAddress* inAddress,
                                          UInt32* ioDataSize,
                                          void* outData);
    static OSStatus    AHSSetPropertyData(AudioObjectID inObjectID,
                                          const AudioObjectPropertyAddress* inAddress,
                                          UInt32 inDataSize,
                                          const void* inData);

};

#endif /* BGMApp__BGMAudioDevice */


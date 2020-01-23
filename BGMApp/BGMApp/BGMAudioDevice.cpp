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
//  BGMAudioDevice.cpp
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//

// Self Include
#include "BGMAudioDevice.h"

// Local Includes
#include "BGM_Types.h"

// System Includes
#include <AudioToolbox/AudioServices.h>


#pragma mark Construction/Destruction

BGMAudioDevice::BGMAudioDevice(AudioObjectID inAudioDevice)
:
    CAHALAudioDevice(inAudioDevice)
{
}

BGMAudioDevice::BGMAudioDevice(CFStringRef inUID)
:
    CAHALAudioDevice(inUID)
{
}

BGMAudioDevice::BGMAudioDevice(const CAHALAudioDevice& inDevice)
:
    BGMAudioDevice(inDevice.GetObjectID())
{
}

BGMAudioDevice::~BGMAudioDevice()
{
}

bool    BGMAudioDevice::CanBeOutputDeviceInBGMApp() const
{
    CFStringRef uid = CopyDeviceUID();
    bool isNullDevice = CFEqual(uid, CFSTR(kBGMNullDeviceUID));
    CFRelease(uid);

    bool hasOutputChannels = GetTotalNumberChannels(/* inIsInput = */ false) > 0;
    bool canBeDefault = CanBeDefaultDevice(/* inIsInput = */ false, /* inIsSystem = */ false);

    return !IsBGMDeviceInstance() &&
            !isNullDevice &&
            !IsHidden() &&
            hasOutputChannels &&
            canBeDefault;
}

#pragma mark Available Controls

bool    BGMAudioDevice::HasSettableMasterVolume(AudioObjectPropertyScope inScope) const
{
    return HasVolumeControl(inScope, kMasterChannel) &&
        VolumeControlIsSettable(inScope, kMasterChannel);
}

bool    BGMAudioDevice::HasSettableVirtualMasterVolume(AudioObjectPropertyScope inScope) const
{
    AudioObjectPropertyAddress virtualMasterVolumeAddress = {
        kAudioHardwareServiceDeviceProperty_VirtualMasterVolume,
        inScope,
        kAudioObjectPropertyElementMaster
    };

    // TODO: Replace these calls deprecated AudioToolbox functions. There are more below.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    Boolean virtualMasterVolumeIsSettable;
    OSStatus err = AudioHardwareServiceIsPropertySettable(GetObjectID(),
                                                          &virtualMasterVolumeAddress,
                                                          &virtualMasterVolumeIsSettable);
    virtualMasterVolumeIsSettable &= (err == kAudioServicesNoError);

    bool hasVirtualMasterVolume =
        AudioHardwareServiceHasProperty(GetObjectID(), &virtualMasterVolumeAddress);
#pragma clang diagnostic pop

    return hasVirtualMasterVolume && virtualMasterVolumeIsSettable;
}

bool    BGMAudioDevice::HasSettableMasterMute(AudioObjectPropertyScope inScope) const
{
    return HasMuteControl(inScope, kMasterChannel) &&
        MuteControlIsSettable(inScope, kMasterChannel);
}

#pragma mark Control Values Accessors

void    BGMAudioDevice::CopyMuteFrom(const BGMAudioDevice inDevice,
                                     AudioObjectPropertyScope inScope)
{
    // TODO: Support for devices that have per-channel mute controls but no master mute control
    if(HasSettableMasterMute(inScope) && inDevice.HasMuteControl(inScope, kMasterChannel))
    {
        SetMuteControlValue(inScope,
                            kMasterChannel,
                            inDevice.GetMuteControlValue(inScope, kMasterChannel));
    }
}

void    BGMAudioDevice::CopyVolumeFrom(const BGMAudioDevice inDevice,
                                       AudioObjectPropertyScope inScope)
{
    // Get the volume of the other device.
    bool didGetVolume = false;
    Float32 volume = FLT_MIN;

    if(inDevice.HasVolumeControl(inScope, kMasterChannel))
    {
        volume = inDevice.GetVolumeControlScalarValue(inScope, kMasterChannel);
        didGetVolume = true;
    }

    // Use the average channel volume of the other device if it has no master volume.
    if(!didGetVolume)
    {
        UInt32 numChannels =
            inDevice.GetTotalNumberChannels(inScope == kAudioObjectPropertyScopeInput);
        volume = 0;

        for(UInt32 channel = 1; channel <= numChannels; channel++)
        {
            if(inDevice.HasVolumeControl(inScope, channel))
            {
                volume += inDevice.GetVolumeControlScalarValue(inScope, channel);
                didGetVolume = true;
            }
        }

        if(numChannels > 0)  // Avoid divide by zero.
        {
            volume /= numChannels;
        }
    }

    // Set the volume of this device.
    if(didGetVolume && volume != FLT_MIN)
    {
        bool didSetVolume = false;

        try
        {
            didSetVolume = SetMasterVolumeScalar(inScope, volume);
        }
        catch(CAException e)
        {
            OSStatus err = e.GetError();
            char err4CC[5] = CA4CCToCString(err);
            CFStringRef uid = CopyDeviceUID();
            LogWarning("BGMAudioDevice::CopyVolumeFrom: CAException '%s' trying to set master "
                       "volume of %s",
                       err4CC,
                       CFStringGetCStringPtr(uid, kCFStringEncodingUTF8));
            CFRelease(uid);
        }

        if(!didSetVolume)
        {
            // Couldn't find a master volume control to set, so try to find a virtual one
            Float32 virtualMasterVolume;
            bool success = inDevice.GetVirtualMasterVolumeScalar(inScope, virtualMasterVolume);
            if(success)
            {
                didSetVolume = SetVirtualMasterVolumeScalar(inScope, virtualMasterVolume);
            }
        }

        if(!didSetVolume)
        {
            // Couldn't set a master or virtual master volume, so as a fallback try to set each
            // channel individually.
            UInt32 numChannels = GetTotalNumberChannels(inScope == kAudioObjectPropertyScopeInput);
            for(UInt32 channel = 1; channel <= numChannels; channel++)
            {
                if(HasVolumeControl(inScope, channel) && VolumeControlIsSettable(inScope, channel))
                {
                    SetVolumeControlScalarValue(inScope, channel, volume);
                }
            }
        }
    }
}

bool    BGMAudioDevice::SetMasterVolumeScalar(AudioObjectPropertyScope inScope, Float32 inVolume)
{
    if(HasSettableMasterVolume(inScope))
    {
        SetVolumeControlScalarValue(inScope, kMasterChannel, inVolume);
        return true;
    }

    return false;
}

bool    BGMAudioDevice::GetVirtualMasterVolumeScalar(AudioObjectPropertyScope inScope,
                                                     Float32& outVirtualMasterVolume) const
{
    AudioObjectPropertyAddress virtualMasterVolumeAddress = {
        kAudioHardwareServiceDeviceProperty_VirtualMasterVolume,
        inScope,
        kAudioObjectPropertyElementMaster
    };

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if(!AudioHardwareServiceHasProperty(GetObjectID(), &virtualMasterVolumeAddress))
    {
        return false;
    }
#pragma clang diagnostic pop

    UInt32 virtualMasterVolumePropertySize = sizeof(Float32);
    return kAudioServicesNoError == AHSGetPropertyData(GetObjectID(),
                                                       &virtualMasterVolumeAddress,
                                                       &virtualMasterVolumePropertySize,
                                                       &outVirtualMasterVolume);
}

bool    BGMAudioDevice::SetVirtualMasterVolumeScalar(AudioObjectPropertyScope inScope,
                                                     Float32 inVolume)
{
    // TODO: For me, setting the virtual master volume sets all the device's channels to the same volume, meaning you can't
    //       keep any channels quieter than the others. The expected behaviour is to scale the channel volumes
    //       proportionally. So to do this properly I think we'd have to store BGMDevice's previous volume and calculate
    //       each channel's new volume from its current volume and the distance between BGMDevice's old and new volumes.
    //
    //       The docs kAudioHardwareServiceDeviceProperty_VirtualMasterVolume for say
    //           "If the device has individual channel volume controls, this property will apply to those identified by the
    //           device's preferred multi-channel layout (or preferred stereo pair if the device is stereo only). Note that
    //           this control maintains the relative balance between all the channels it affects.
    //       so I'm not sure why that's not working here. As a workaround we take the to device's (virtual master) balance
    //       before changing the volume and set it back after, but of course that'll only work for stereo devices.

    bool didSetVolume = false;

    if(HasSettableVirtualMasterVolume(inScope))
    {
        // Not sure why, but setting the virtual master volume sets all channels to the same volume. As a workaround, we store
        // the current balance here so we can reset it after setting the volume.
        Float32 virtualMasterBalance;
        bool didGetVirtualMasterBalance = GetVirtualMasterBalance(inScope, virtualMasterBalance);

        AudioObjectPropertyAddress virtualMasterVolumeAddress = {
            kAudioHardwareServiceDeviceProperty_VirtualMasterVolume,
            inScope,
            kAudioObjectPropertyElementMaster
        };

        didSetVolume = (kAudioServicesNoError == AHSSetPropertyData(GetObjectID(),
                                                                    &virtualMasterVolumeAddress,
                                                                    sizeof(Float32),
                                                                    &inVolume));

        // Reset the balance
        AudioObjectPropertyAddress virtualMasterBalanceAddress = {
            kAudioHardwareServiceDeviceProperty_VirtualMasterBalance,
            inScope,
            kAudioObjectPropertyElementMaster
        };

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        if(didSetVolume &&
           didGetVirtualMasterBalance &&
           AudioHardwareServiceHasProperty(GetObjectID(), &virtualMasterBalanceAddress))
        {
            Boolean balanceIsSettable;
            OSStatus err = AudioHardwareServiceIsPropertySettable(GetObjectID(),
                                                                  &virtualMasterBalanceAddress,
                                                                  &balanceIsSettable);
            if(err == kAudioServicesNoError && balanceIsSettable)
            {
                AHSSetPropertyData(GetObjectID(),
                                   &virtualMasterBalanceAddress,
                                   sizeof(Float32),
                                   &virtualMasterBalance);
            }
        }
#pragma clang diagnostic pop
    }

    return didSetVolume;
}

bool    BGMAudioDevice::GetVirtualMasterBalance(AudioObjectPropertyScope inScope,
                                                Float32& outVirtualMasterBalance) const
{
    AudioObjectPropertyAddress virtualMasterBalanceAddress = {
        kAudioHardwareServiceDeviceProperty_VirtualMasterBalance,
        inScope,
        kAudioObjectPropertyElementMaster
    };

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if(!AudioHardwareServiceHasProperty(GetObjectID(), &virtualMasterBalanceAddress))
    {
        return false;
    }
#pragma clang diagnostic pop

    UInt32 virtualMasterVolumePropertySize = sizeof(Float32);
    return kAudioServicesNoError == AHSGetPropertyData(GetObjectID(),
                                                       &virtualMasterBalanceAddress,
                                                       &virtualMasterVolumePropertySize,
                                                       &outVirtualMasterBalance);
}

#pragma mark Implementation

bool    BGMAudioDevice::IsBGMDevice(bool inIncludeUISoundsInstance) const
{
    bool isBGMDevice = false;

    if(GetObjectID() != kAudioObjectUnknown)
    {
        // Check the device's UID to see whether it's BGMDevice.
        CFStringRef uid = CopyDeviceUID();

        isBGMDevice =
            CFEqual(uid, CFSTR(kBGMDeviceUID)) ||
                    (inIncludeUISoundsInstance && CFEqual(uid, CFSTR(kBGMDeviceUID_UISounds)));

        CFRelease(uid);
    }

    return isBGMDevice;
}

// static
OSStatus    BGMAudioDevice::AHSGetPropertyData(AudioObjectID inObjectID,
                                               const AudioObjectPropertyAddress* inAddress,
                                               UInt32* ioDataSize,
                                               void* outData)
{
    // The docs for AudioHardwareServiceGetPropertyData specifically allow passing NULL for
    // inQualifierData as we do here, but it's declared in an assume_nonnull section so we have to
    // disable the warning here. I'm not sure why inQualifierData isn't __nullable. I'm assuming
    // it's either a backwards compatibility thing or just a bug.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // The non-depreciated version of this (and the setter below) doesn't seem to support devices
    // other than the default
    return AudioHardwareServiceGetPropertyData(inObjectID, inAddress, 0, NULL, ioDataSize, outData);
#pragma clang diagnostic pop
}

// static
OSStatus    BGMAudioDevice::AHSSetPropertyData(AudioObjectID inObjectID,
                                               const AudioObjectPropertyAddress* inAddress,
                                               UInt32 inDataSize,
                                               const void* inData)
{
    // See the explanation about these pragmas in AHSGetPropertyData
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return AudioHardwareServiceSetPropertyData(inObjectID, inAddress, 0, NULL, inDataSize, inData);
#pragma clang diagnostic pop
}


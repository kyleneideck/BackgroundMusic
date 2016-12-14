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
//  BGMDeviceControlSync.cpp
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#include "BGMDeviceControlSync.h"

// Local Includes
#include "BGM_Types.h"
#include "BGM_Utils.h"

// System Includes
#include <AudioToolbox/AudioServices.h>


#pragma clang assume_nonnull begin

// AudioObjectPropertyElement docs: "Elements are numbered sequentially where 0 represents the master element."
static const AudioObjectPropertyElement kMasterChannel = 0;

static const AudioObjectPropertyAddress kMutePropertyAddress =
    { kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster };

static const AudioObjectPropertyAddress kVolumePropertyAddress =
    { kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster };

#pragma mark Construction/Destruction

BGMDeviceControlSync::BGMDeviceControlSync(CAHALAudioDevice inBGMDevice, CAHALAudioDevice inOutputDevice)
:
    mBGMDevice(inBGMDevice),
    mOutputDevice(inOutputDevice)
{
    Activate();
}

BGMDeviceControlSync::~BGMDeviceControlSync()
{
    Deactivate();
}

void    BGMDeviceControlSync::Activate()
{
    ThrowIf((mBGMDevice.GetObjectID() == kAudioDeviceUnknown || mOutputDevice.GetObjectID() == kAudioDeviceUnknown),
            BGM_DeviceNotSetException(),
            "BGMDeviceControlSync::Activate: Both the output device and BGMDevice must be set to start synchronizing their controls");
    
    // Init BGMDevice controls to match output device
    CopyVolume(mOutputDevice, mBGMDevice, kAudioObjectPropertyScopeOutput);
    CopyMute(mOutputDevice, mBGMDevice, kAudioObjectPropertyScopeOutput);
    
    if(!mActive)
    {
        // Register listeners for volume and mute values
        mBGMDevice.AddPropertyListener(kVolumePropertyAddress, &BGMDeviceControlSync::BGMDeviceListenerProc, this);
        
        try
        {
            mBGMDevice.AddPropertyListener(kMutePropertyAddress, &BGMDeviceControlSync::BGMDeviceListenerProc, this);
        }
        catch(CAException)
        {
            CATry
            mBGMDevice.RemovePropertyListener(kVolumePropertyAddress, &BGMDeviceControlSync::BGMDeviceListenerProc, this);
            CACatch
            
            throw;
        }
        
        mActive = true;
    }
}

void    BGMDeviceControlSync::Deactivate()
{
    if(mActive && mBGMDevice.GetObjectID() != kAudioDeviceUnknown)
    {
        // Unregister listeners
        mBGMDevice.RemovePropertyListener(kVolumePropertyAddress, &BGMDeviceControlSync::BGMDeviceListenerProc, this);
        mBGMDevice.RemovePropertyListener(kMutePropertyAddress, &BGMDeviceControlSync::BGMDeviceListenerProc, this);
    }
}


void    BGMDeviceControlSync::Swap(BGMDeviceControlSync& inDeviceControlSync)
{
    mBGMDevice = inDeviceControlSync.mBGMDevice;
    mOutputDevice = inDeviceControlSync.mOutputDevice;
    
    BGMLogAndSwallowExceptionsMsg("BGMDeviceControlSync::Swap", "Deactivate", [&inDeviceControlSync]() {
        inDeviceControlSync.Deactivate();
    });
    
    BGMLogAndSwallowExceptionsMsg("BGMDeviceControlSync::Swap", "Activate", [&]() {
        Activate();
    });
}

#pragma mark Get/Set Control Values

// static
void    BGMDeviceControlSync::CopyMute(CAHALAudioDevice inFromDevice, CAHALAudioDevice inToDevice, AudioObjectPropertyScope inScope)
{
    // TODO: Support for devices that have per-channel mute controls but no master mute control
    bool toHasSettableMasterMute = inToDevice.HasMuteControl(inScope, kMasterChannel) && inToDevice.MuteControlIsSettable(inScope, kMasterChannel);
    if(toHasSettableMasterMute && inFromDevice.HasMuteControl(inScope, kMasterChannel))
    {
        inToDevice.SetMuteControlValue(inScope,
                                       kMasterChannel,
                                       inFromDevice.GetMuteControlValue(inScope, kMasterChannel));
    }
}

// static
void    BGMDeviceControlSync::CopyVolume(CAHALAudioDevice inFromDevice, CAHALAudioDevice inToDevice, AudioObjectPropertyScope inScope)
{
    // Get the volume of the from device
    bool didGetFromVolume = false;
    Float32 fromVolume = FLT_MIN;
    
    if(inFromDevice.HasVolumeControl(inScope, kMasterChannel))
    {
        fromVolume = inFromDevice.GetVolumeControlScalarValue(inScope, kMasterChannel);
        didGetFromVolume = true;
    }

    // Use the average channel volume of the from device if it has no master volume
    if(!didGetFromVolume)
    {
        UInt32 fromNumChannels = inFromDevice.GetTotalNumberChannels(inScope == kAudioObjectPropertyScopeInput);
        fromVolume = 0;
        
        for(UInt32 channel = 1; channel <= fromNumChannels; channel++)
        {
            if(inFromDevice.HasVolumeControl(inScope, channel))
            {
                fromVolume += inFromDevice.GetVolumeControlScalarValue(inScope, channel);
                didGetFromVolume = true;
            }
        }
        
        fromVolume /= fromNumChannels;
    }

    // Set the volume of the to device
    if(didGetFromVolume && fromVolume != FLT_MIN)
    {
        bool didSetVolume = false;
        
        try
        {
            didSetVolume = SetMasterVolumeScalar(inToDevice, inScope, fromVolume);
        }
        catch(CAException e)
        {
            OSStatus err = e.GetError();
            char err4CC[5] = CA4CCToCString(err);
            CFStringRef uid = inToDevice.CopyDeviceUID();
            LogWarning("BGMDeviceControlSync::CopyVolume: CAException '%s' trying to set master volume of %s",
                       err4CC,
                       CFStringGetCStringPtr(uid, kCFStringEncodingUTF8));
            CFRelease(uid);
        }

        if(!didSetVolume)
        {
            // Couldn't find a master volume control to set, so try to find a virtual one
            Float32 fromVirtualMasterVolume;
            bool success = GetVirtualMasterVolume(inFromDevice, inScope, fromVirtualMasterVolume);
            if(success)
            {
                didSetVolume = SetVirtualMasterVolume(inToDevice, inScope, fromVirtualMasterVolume);
            }
        }
    
        if(!didSetVolume)
        {
            // Couldn't set a master or virtual master volume, so as a fallback try to set each channel individually
            UInt32 numChannels = inToDevice.GetTotalNumberChannels(inScope == kAudioObjectPropertyScopeInput);
            for(UInt32 channel = 1; channel <= numChannels; channel++)
            {
                if(inToDevice.HasVolumeControl(inScope, channel) && inToDevice.VolumeControlIsSettable(inScope, channel))
                {
                    inToDevice.SetVolumeControlScalarValue(inScope, channel, fromVolume);
                }
            }
        }
    }
}

// static
bool    BGMDeviceControlSync::SetMasterVolumeScalar(CAHALAudioDevice inDevice, AudioObjectPropertyScope inScope, Float32 inVolume)
{
    bool hasSettableMasterVolume =
        inDevice.HasVolumeControl(inScope, kMasterChannel) && inDevice.VolumeControlIsSettable(inScope, kMasterChannel);
    
    if(hasSettableMasterVolume)
    {
        inDevice.SetVolumeControlScalarValue(inScope, kMasterChannel, inVolume);
        return true;
    }
    
    return false;
}

// static
bool    BGMDeviceControlSync::GetVirtualMasterVolume(CAHALAudioDevice inDevice, AudioObjectPropertyScope inScope, Float32& outVirtualMasterVolume)
{
    AudioObjectPropertyAddress virtualMasterVolumeAddress =
        { kAudioHardwareServiceDeviceProperty_VirtualMasterVolume, inScope, kAudioObjectPropertyElementMaster };
    
    if(!AudioHardwareServiceHasProperty(inDevice.GetObjectID(), &virtualMasterVolumeAddress))
    {
        return false;
    }
    
    UInt32 virtualMasterVolumePropertySize = sizeof(Float32);
    return kAudioServicesNoError == AHSGetPropertyData(inDevice.GetObjectID(),
                                                       &virtualMasterVolumeAddress,
                                                       &virtualMasterVolumePropertySize,
                                                       &outVirtualMasterVolume);
}

// static
bool    BGMDeviceControlSync::SetVirtualMasterVolume(CAHALAudioDevice inDevice, AudioObjectPropertyScope inScope, Float32 inVolume)
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
    AudioObjectPropertyAddress virtualMasterVolumeAddress =
        { kAudioHardwareServiceDeviceProperty_VirtualMasterVolume, inScope, kAudioObjectPropertyElementMaster };
    bool hasVirtualMasterVolume = AudioHardwareServiceHasProperty(inDevice.GetObjectID(), &virtualMasterVolumeAddress);
    
    Boolean virtualMasterVolumeIsSettable;
    OSStatus err = AudioHardwareServiceIsPropertySettable(inDevice.GetObjectID(), &virtualMasterVolumeAddress, &virtualMasterVolumeIsSettable);
    virtualMasterVolumeIsSettable &= (err == kAudioServicesNoError);
    
    if(hasVirtualMasterVolume && virtualMasterVolumeIsSettable)
    {
        // Not sure why, but setting the virtual master volume sets all channels to the same volume. As a workaround, we store
        // the current balance here so we can reset it after setting the volume.
        Float32 virtualMasterBalance;
        bool didGetVirtualMasterBalance = GetVirtualMasterBalance(inDevice, inScope, virtualMasterBalance);
        
        didSetVolume = kAudioServicesNoError == AHSSetPropertyData(inDevice.GetObjectID(), &virtualMasterVolumeAddress, sizeof(Float32), &inVolume);
        
        // Reset the balance
        AudioObjectPropertyAddress virtualMasterBalanceAddress =
            { kAudioHardwareServiceDeviceProperty_VirtualMasterBalance, inScope, kAudioObjectPropertyElementMaster };
        
        if(didSetVolume && didGetVirtualMasterBalance && AudioHardwareServiceHasProperty(inDevice.GetObjectID(), &virtualMasterBalanceAddress))
        {
            Boolean balanceIsSettable;
            err = AudioHardwareServiceIsPropertySettable(inDevice.GetObjectID(), &virtualMasterBalanceAddress, &balanceIsSettable);
            if(err == kAudioServicesNoError && balanceIsSettable)
            {
                AHSSetPropertyData(inDevice.GetObjectID(), &virtualMasterBalanceAddress, sizeof(Float32), &virtualMasterBalance);
            }
        }
    }
    
    return didSetVolume;
}

// static
bool    BGMDeviceControlSync::GetVirtualMasterBalance(CAHALAudioDevice inDevice, AudioObjectPropertyScope inScope, Float32& outVirtualMasterBalance)
{
    AudioObjectPropertyAddress virtualMasterBalanceAddress =
        { kAudioHardwareServiceDeviceProperty_VirtualMasterBalance, inScope, kAudioObjectPropertyElementMaster };
    
    if(!AudioHardwareServiceHasProperty(inDevice.GetObjectID(), &virtualMasterBalanceAddress))
    {
        return false;
    }
    
    UInt32 virtualMasterVolumePropertySize = sizeof(Float32);
    return kAudioServicesNoError == AHSGetPropertyData(inDevice.GetObjectID(),
                                                       &virtualMasterBalanceAddress,
                                                       &virtualMasterVolumePropertySize,
                                                       &outVirtualMasterBalance);
}

// static
OSStatus    BGMDeviceControlSync::AHSGetPropertyData(AudioObjectID inObjectID, const AudioObjectPropertyAddress* inAddress, UInt32* ioDataSize, void* outData)
{
    // The docs for AudioHardwareServiceGetPropertyData specifically allow passing NULL for inQualifierData as we do here,
    // but it's declared in an assume_nonnull section so we have to disable the warning here. I'm not sure why inQualifierData
    // isn't __nullable. I'm assuming it's either a backwards compatibility thing or just a bug.
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wnonnull"
    // The non-depreciated version of this (and the setter below) doesn't seem to support devices other than the default
    return AudioHardwareServiceGetPropertyData(inObjectID, inAddress, 0, NULL, ioDataSize, outData);
    #pragma clang diagnostic pop
}

// static
OSStatus    BGMDeviceControlSync::AHSSetPropertyData(AudioObjectID inObjectID, const AudioObjectPropertyAddress* inAddress, UInt32 inDataSize, const void* inData)
{
    // See the explanation about these pragmas in AHSGetPropertyData
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wnonnull"
    return AudioHardwareServiceSetPropertyData(inObjectID, inAddress, 0, NULL, inDataSize, inData);
    #pragma clang diagnostic pop
}

#pragma mark Listener

// static
OSStatus    BGMDeviceControlSync::BGMDeviceListenerProc(AudioObjectID inObjectID, UInt32 inNumberAddresses, const AudioObjectPropertyAddress* __nonnull inAddresses, void* __nullable inClientData)
{
    // refCon (reference context) is the instance that registered this listener proc
    BGMDeviceControlSync* refCon = static_cast<BGMDeviceControlSync*>(inClientData);
    
    if(refCon->mActive)
    {
        ThrowIf(inObjectID != refCon->mBGMDevice.GetObjectID(),
                CAException(kAudioHardwareBadObjectError),
                "BGMDeviceControlSync::BGMDeviceListenerProc: notified about audio object other than BGMDevice");
        
        for(int i = 0; i < inNumberAddresses; i++)
        {
            AudioObjectPropertyScope scope = inAddresses[i].mScope;
            
            switch(inAddresses[i].mSelector)
            {
                case kAudioDevicePropertyVolumeScalar:
                    // Update the output device
                    CopyVolume(refCon->mBGMDevice, refCon->mOutputDevice, scope);
                    break;
                    
                case kAudioDevicePropertyMute:
                    // Update the output device. Note that this also runs when you change the volume (on BGMDevice)
                    CopyMute(refCon->mBGMDevice, refCon->mOutputDevice, scope);
                    break;
                    
                default:
                    break;
            }
        }
    }
    
    return 0;
}

#pragma clang assume_nonnull end


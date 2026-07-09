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
//  Copyright © 2016, 2017, 2026 Kyle Neideck
//

// Self Include
#include "BGMDeviceControlSync.h"

// Local Includes
#include "BGM_Types.h"
#include "BGM_Utils.h"

// PublicUtility Includes
#include "CAPropertyAddress.h"


#pragma clang assume_nonnull begin

static const AudioObjectPropertyAddress kMutePropertyAddress =
    { kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain };

static const AudioObjectPropertyAddress kVolumePropertyAddress =
    { kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain };

#pragma mark Construction/Destruction

BGMDeviceControlSync::BGMDeviceControlSync(AudioObjectID inBGMDevice,
                                           AudioObjectID inOutputDevice,
                                           CAHALAudioSystemObject inAudioSystem)
:
    mBGMDevice(inBGMDevice),
    mOutputDevice(inOutputDevice),
    mAudioSystem(inAudioSystem),
    mBGMDeviceControlsList(inBGMDevice)
{
}

BGMDeviceControlSync::~BGMDeviceControlSync()
{
    BGMLogAndSwallowExceptions("BGMDeviceControlSync::~BGMDeviceControlSync", [&] {
        CAMutex::Locker locker(mMutex);

        Deactivate();
    });
}

void    BGMDeviceControlSync::Activate()
{
    CAMutex::Locker locker(mMutex);

    ThrowIf((mBGMDevice.GetObjectID() == kAudioObjectUnknown || mOutputDevice.GetObjectID() == kAudioObjectUnknown),
            BGM_DeviceNotSetException(),
            "BGMDeviceControlSync::Activate: Both the output device and BGMDevice must be set to start synchronizing their controls");

    if(!mActive)
    {
        DebugMsg("BGMDeviceControlSync::Activate: Activating control sync");

        // Decide whether to apply the output volume in software. We do this when software output
        // volume is enabled and the output device has no hardware volume control (e.g. an HDMI
        // monitor). In that case BGMDevice applies its volume to the audio itself (in BGMDriver) and
        // we keep BGMDevice's volume control enabled so the slider stays usable. Otherwise we keep
        // the original behaviour: mirror BGMDevice's volume onto the output device's hardware volume
        // control.
        bool outputHasVolume = false;
        BGMLogAndSwallowExceptionsMsg("BGMDeviceControlSync::Activate", "Output volume check", [&] {
            outputHasVolume =
                BGMDeviceControlsList::OutputDeviceHasSettableVolume(mOutputDevice);
        });

        mInSoftwareVolumeMode = mSoftwareVolumeEnabled && !outputHasVolume;

        DebugMsg("BGMDeviceControlSync::Activate: %s software output volume",
                 mInSoftwareVolumeMode ? "Using" : "Not using");

        // Tell BGMDriver whether to apply BGMDevice's volume to the audio it outputs.
        BGMLogAndSwallowExceptionsMsg("BGMDeviceControlSync::Activate", "Applying-volume property",
                                      [&] {
            mBGMDevice.SetPropertyData_CFType(kBGMApplyingVolumeToAudioAddress,
                                              mInSoftwareVolumeMode ? kCFBooleanTrue
                                                                    : kCFBooleanFalse);
        });

        // Disable BGMDevice controls that the output device doesn't have and reenable any that were
        // disabled for the previous output device. In software volume mode we keep the volume
        // control enabled regardless of the output device.
        //
        // Continue anyway if this fails because it's better to have extra/missing controls than to
        // be unable to use the device.
        BGMLogAndSwallowExceptionsMsg("BGMDeviceControlSync::Activate", "Controls list", [&] {
            bool wasUpdated =
                mBGMDeviceControlsList.MatchControlsListOf(mOutputDevice, mInSoftwareVolumeMode);
            if(wasUpdated)
            {
                mBGMDeviceControlsList.PropagateControlListChange();
            }
        });

        // Init BGMDevice controls to match output device. In software volume mode we leave
        // BGMDevice's volume as the user set it (the output device has no volume to copy anyway) so
        // we don't reset the user's chosen software volume.
        if(!mInSoftwareVolumeMode)
        {
            mBGMDevice.CopyVolumeFrom(mOutputDevice, kAudioObjectPropertyScopeOutput);
        }
        mBGMDevice.CopyMuteFrom(mOutputDevice, kAudioObjectPropertyScopeOutput);

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
    else
    {
        DebugMsg("BGMDeviceControlSync::Activate: Already active");
    }
}

void    BGMDeviceControlSync::Deactivate()
{
    CAMutex::Locker locker(mMutex);

    if(mActive)
    {
        DebugMsg("BGMDeviceControlSync::Deactivate: Deactivating control sync");

        // Deregister listeners
        if(mBGMDevice.GetObjectID() != kAudioDeviceUnknown)
        {
            BGMLogAndSwallowExceptions("BGMDeviceControlSync::Deactivate", [&] {
                mBGMDevice.RemovePropertyListener(kVolumePropertyAddress,
                                                  &BGMDeviceControlSync::BGMDeviceListenerProc,
                                                  this);
            });

            BGMLogAndSwallowExceptions("BGMDeviceControlSync::Deactivate", [&] {
                mBGMDevice.RemovePropertyListener(kMutePropertyAddress,
                                                  &BGMDeviceControlSync::BGMDeviceListenerProc,
                                                  this);
            });
        }

        mActive = false;
    }
    else
    {
        DebugMsg("BGMDeviceControlSync::Deactivate: Not active");
    }
}

#pragma mark Accessors

void    BGMDeviceControlSync::SetDevices(AudioObjectID inBGMDevice, AudioObjectID inOutputDevice)
{
    CAMutex::Locker locker(mMutex);

    bool wasActive = mActive;

    Deactivate();

    mBGMDevice = inBGMDevice;
    mBGMDeviceControlsList.SetBGMDevice(inBGMDevice);
    mOutputDevice = inOutputDevice;
    
    if(wasActive)
    {
        Activate();
    }
}

void    BGMDeviceControlSync::SetSoftwareVolumeEnabled(bool inEnabled)
{
    CAMutex::Locker locker(mMutex);

    if(mSoftwareVolumeEnabled == inEnabled)
    {
        return;
    }

    mSoftwareVolumeEnabled = inEnabled;

    // Re-evaluate the mode with the new setting. Deactivate/Activate here mirror SetDevices.
    if(mActive)
    {
        Deactivate();
        Activate();
    }
}

#pragma mark Listener Procs

// static
OSStatus    BGMDeviceControlSync::BGMDeviceListenerProc(AudioObjectID inObjectID, UInt32 inNumberAddresses, const AudioObjectPropertyAddress* inAddresses, void* __nullable inClientData)
{
    // refCon (reference context) is the instance that registered this listener proc.
    BGMDeviceControlSync* refCon = static_cast<BGMDeviceControlSync*>(inClientData);

    auto checkState = [&] {
        if(!refCon)
        {
            LogError("BGMDeviceControlSync::BGMDeviceListenerProc: !refCon");
            return false;
        }

        if(!refCon->mActive ||
           (refCon->mBGMDevice.GetObjectID() == kAudioObjectUnknown) ||
           (refCon->mOutputDevice.GetObjectID() == kAudioObjectUnknown))
        {
            return false;
        }

        if(inObjectID != refCon->mBGMDevice.GetObjectID())
        {
            LogError("BGMDeviceControlSync::BGMDeviceListenerProc: notified about audio object other than BGMDevice");
            return false;
        }
        
        return true;
    };

    for(int i = 0; i < inNumberAddresses; i++)
    {
        AudioObjectPropertyScope scope = inAddresses[i].mScope;
        
        switch(inAddresses[i].mSelector)
        {
            case kAudioDevicePropertyVolumeScalar:
                {
                    CAMutex::Locker locker(refCon->mMutex);

                    // Update the output device's volume. In software volume mode BGMDevice applies
                    // its volume to the audio itself, so we must not also mirror it onto the output
                    // device (that would either double-attenuate or, more likely, do nothing since
                    // the output has no volume control).
                    if(checkState() && !refCon->mInSoftwareVolumeMode)
                    {
                        refCon->mOutputDevice.CopyVolumeFrom(refCon->mBGMDevice, scope);
                    }
                }
                break;
                
            case kAudioDevicePropertyMute:
                {
                    CAMutex::Locker locker(refCon->mMutex);

                    // Update the output device's mute control. Note that this also runs when you
                    // change the volume (on BGMDevice).
                    if(checkState())
                    {
                        refCon->mOutputDevice.CopyMuteFrom(refCon->mBGMDevice, scope);
                    }
                }
                break;
        }
    }

    // "The return value [of an AudioObjectPropertyListenerProc] is currently unused and should always be 0."
    return 0;
}

#pragma clang assume_nonnull end


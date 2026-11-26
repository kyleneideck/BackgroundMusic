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

static const AudioObjectPropertyScope kOutputScope = kAudioObjectPropertyScopeOutput;

#pragma mark Construction/Destruction

BGMDeviceControlSync::BGMDeviceControlSync(AudioObjectID inBGMDevice,
                                           const std::vector<BGMAudioDevice>& inOutputDevices)
:
    mBGMDevice(inBGMDevice),
    mOutputDevices(inOutputDevices)
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

    ThrowIf((mBGMDevice == kAudioObjectUnknown),
            BGM_DeviceNotSetException(),
            "BGMDeviceControlSync::Activate: BGMDevice must be set to normalise the output "
            "devices' controls");

    if(!mActive)
    {
        DebugMsg("BGMDeviceControlSync::Activate: Normalising the output devices' volume controls");

        // Set each output device's volume control to full and unmute it. BGMApp applies the
        // master volume and each device's relative volume in software (see BGMPlayThrough), so
        // the devices' hardware volume controls would just attenuate the audio a second time.
        //
        // Continue anyway if a device fails because it's better to have a device at the wrong
        // level than to be unable to use it at all.
        for(const auto& outputDeviceRef : mOutputDevices)
        {
            BGMAudioDevice outputDevice(outputDeviceRef.GetObjectID());

            BGMLogAndSwallowExceptionsMsg("BGMDeviceControlSync::Activate", "Volume", [&] {
                if(outputDevice.HasSettableMainVolume(kOutputScope))
                {
                    outputDevice.SetVolumeControlScalarValue(kOutputScope, kMainChannel, 1.0f);
                }
            });

            BGMLogAndSwallowExceptionsMsg("BGMDeviceControlSync::Activate", "Mute", [&] {
                if(outputDevice.HasSettableMainMute(kOutputScope))
                {
                    outputDevice.SetMuteControlValue(kOutputScope, kMainChannel, false);
                }
            });
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
        
        mActive = false;
    }
    else
    {
        DebugMsg("BGMDeviceControlSync::Deactivate: Not active");
    }
}

#pragma mark Accessors

void    BGMDeviceControlSync::SetDevices(AudioObjectID inBGMDevice,
                                         const std::vector<BGMAudioDevice>& inOutputDevices)
{
    CAMutex::Locker locker(mMutex);

    bool wasActive = mActive;

    Deactivate();

    mBGMDevice = inBGMDevice;
    mOutputDevices = inOutputDevices;
    
    if(wasActive)
    {
        Activate();
    }
}

#pragma clang assume_nonnull end

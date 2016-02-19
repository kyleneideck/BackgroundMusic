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
//  Copyright © 2016 Kyle Neideck
//
//  Listens for notifications that BGMDevice's controls (just volume and mute currently) have changed value, and
//  copies the new values to the output device.
//

#ifndef BGMDeviceControlSync_h
#define BGMDeviceControlSync_h

// PublicUtility Includes
#include "CAHALAudioDevice.h"


#pragma clang assume_nonnull begin

class BGMDeviceControlSync
{
    
public:
                        BGMDeviceControlSync(CAHALAudioDevice inBGMDevice, CAHALAudioDevice inOutputDevice);
                        ~BGMDeviceControlSync();
                        // Disallow copying
                        BGMDeviceControlSync(const BGMDeviceControlSync&) = delete;
                        BGMDeviceControlSync& operator=(const BGMDeviceControlSync&) = delete;
                        // Move constructor/assignment
                        BGMDeviceControlSync(BGMDeviceControlSync&& inDeviceControlSync) { Swap(inDeviceControlSync); }
                        BGMDeviceControlSync& operator=(BGMDeviceControlSync&& inDeviceControlSync) { Swap(inDeviceControlSync); return *this; }
    
#ifdef __OBJC__
                        // Only intended as a convenience for Objective-C instance vars
                        BGMDeviceControlSync() { };
#endif
    
private:
    void                Activate();
    void                Deactivate();
    void                Swap(BGMDeviceControlSync& inDeviceControlSync);
    
    static void         CopyMute(CAHALAudioDevice inFromDevice, CAHALAudioDevice inToDevice, AudioObjectPropertyScope inScope);
    static void         CopyVolume(CAHALAudioDevice inFromDevice, CAHALAudioDevice inToDevice, AudioObjectPropertyScope inScope);
    
    static bool         SetMasterVolumeScalar(CAHALAudioDevice inDevice, AudioObjectPropertyScope inScope, Float32 inVolume);
    static bool         GetVirtualMasterVolume(CAHALAudioDevice inDevice, AudioObjectPropertyScope inScope, Float32& outVirtualMasterVolume);
    static bool         SetVirtualMasterVolume(CAHALAudioDevice inDevice, AudioObjectPropertyScope inScope, Float32 inVolume);
    static bool         GetVirtualMasterBalance(CAHALAudioDevice inDevice, AudioObjectPropertyScope inScope, Float32& outVirtualMasterBalance);
    
    static OSStatus     AHSGetPropertyData(AudioObjectID inObjectID, const AudioObjectPropertyAddress* inAddress, UInt32* ioDataSize, void* outData);
    static OSStatus     AHSSetPropertyData(AudioObjectID inObjectID, const AudioObjectPropertyAddress* inAddress, UInt32 inDataSize, const void* inData);
    
    static OSStatus     BGMDeviceListenerProc(AudioObjectID inObjectID, UInt32 inNumberAddresses, const AudioObjectPropertyAddress* inAddresses, void* __nullable inClientData);
    
private:
    bool                mActive = false;
    CAHALAudioDevice    mBGMDevice = CAHALAudioDevice(kAudioDeviceUnknown);
    CAHALAudioDevice    mOutputDevice = CAHALAudioDevice(kAudioDeviceUnknown);
    
};

#pragma clang assume_nonnull end

#endif /* BGMDeviceControlSync_h */


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
//  BGMBackgroundMusicDevice.cpp
//  BGMApp
//
//  Copyright © 2016, 2017 Kyle Neideck
//  Copyright © 2017 Andrew Tonner
//

// Self Include
#include "BGMBackgroundMusicDevice.h"

// Local Includes
#include "BGM_Types.h"
#include "BGM_Utils.h"

// PublicUtility Includes
#include "CADebugMacros.h"
#include "CAHALAudioSystemObject.h"
#include "CACFArray.h"
#include "CACFDictionary.h"

// STL Includes
#include <map>


#pragma clang assume_nonnull begin

#pragma mark Construction/Destruction

BGMBackgroundMusicDevice::BGMBackgroundMusicDevice()
:
    BGMAudioDevice(CFSTR(kBGMDeviceUID)),
    mUISoundsBGMDevice(CFSTR(kBGMDeviceUID_UISounds))
{
    if((GetObjectID() == kAudioObjectUnknown) || (mUISoundsBGMDevice == kAudioObjectUnknown))
    {
        Throw(CAException(kAudioHardwareIllegalOperationError));
    }
};

BGMBackgroundMusicDevice::~BGMBackgroundMusicDevice()
{
}

#pragma mark Systemwide Default Device

void BGMBackgroundMusicDevice::SetAsOSDefault()
{
    DebugMsg("BGMBackgroundMusicDevice::SetAsOSDefault: Setting the system's default audio device "
             "to BGMDevice");

    CAHALAudioSystemObject audioSystem;

    AudioDeviceID defaultDevice = audioSystem.GetDefaultAudioDevice(false, false);
    AudioDeviceID systemDefaultDevice = audioSystem.GetDefaultAudioDevice(false, true);

    if(systemDefaultDevice == defaultDevice)
    {
        // The default system device is the same as the default device, so change both of them.
        //
        // Use the UI sounds instance of BGMDevice because the default system output device is the
        // device "to use for system related sound". The allows BGMDriver to tell when the audio it
        // receives is UI-related.
        audioSystem.SetDefaultAudioDevice(false, true, mUISoundsBGMDevice);
    }

    audioSystem.SetDefaultAudioDevice(false, false, GetObjectID());
}

void BGMBackgroundMusicDevice::UnsetAsOSDefault(AudioDeviceID inOutputDeviceID)
{
    CAHALAudioSystemObject audioSystem;

    // Set BGMApp's output device as OS X's default output device.
    bool bgmDeviceIsDefault =
            (audioSystem.GetDefaultAudioDevice(false, false) == GetObjectID());

    if(bgmDeviceIsDefault)
    {
        DebugMsg("BGMBackgroundMusicDevice::UnsetAsOSDefault: Setting the system's default output "
                 "device back to device %d", inOutputDeviceID);

        audioSystem.SetDefaultAudioDevice(false, false, inOutputDeviceID);
    }

    // Set BGMApp's output device as OS X's default system output device.
    bool bgmDeviceIsSystemDefault =
            (audioSystem.GetDefaultAudioDevice(false, true) == mUISoundsBGMDevice);

    // If we changed the default system output device to BGMDevice, which we only do if it's set to
    // the same device as the default output device, change it back to the previous device.
    if(bgmDeviceIsSystemDefault)
    {
        DebugMsg("BGMBackgroundMusicDevice::UnsetAsOSDefault: Setting the system's default system "
                 "output device back to device %d", inOutputDeviceID);

        audioSystem.SetDefaultAudioDevice(false, true, inOutputDeviceID);
    }
}

#pragma mark App Volumes

void BGMBackgroundMusicDevice::SetAppVolume(SInt32 inVolume,
                                            pid_t inAppProcessID,
                                            CFStringRef inAppBundleID)
{
    BGMAssert((kAppRelativeVolumeMinRawValue <= inVolume) &&
                      (inVolume <= kAppRelativeVolumeMaxRawValue),
              "BGMBackgroundMusicDevice::SetAppVolume: Volume out of bounds");

    // Clamp the volume to [kAppRelativeVolumeMinRawValue, kAppPanRightRawValue].
    inVolume = std::max(kAppRelativeVolumeMinRawValue, inVolume);
    inVolume = std::min(kAppRelativeVolumeMaxRawValue, inVolume);

    SendAppVolumeOrPanToBGMDevice(inVolume,
                                  CFSTR(kBGMAppVolumesKey_RelativeVolume),
                                  inAppProcessID,
                                  inAppBundleID);
}

void BGMBackgroundMusicDevice::SetAppPanPosition(SInt32 inPanPosition,
                                                 pid_t inAppProcessID,
                                                 CFStringRef inAppBundleID)
{
    BGMAssert((kAppPanLeftRawValue <= inPanPosition) && (inPanPosition <= kAppPanRightRawValue),
              "BGMBackgroundMusicDevice::SetAppPanPosition: Pan position out of bounds");

    // Clamp the pan position to [kAppPanLeftRawValue, kAppPanRightRawValue].
    inPanPosition = std::max(kAppPanLeftRawValue, inPanPosition);
    inPanPosition = std::min(kAppPanRightRawValue, inPanPosition);

    SendAppVolumeOrPanToBGMDevice(inPanPosition,
                                  CFSTR(kBGMAppVolumesKey_PanPosition),
                                  inAppProcessID,
                                  inAppBundleID);
}

void BGMBackgroundMusicDevice::SendAppVolumeOrPanToBGMDevice(SInt32 inNewValue,
                                                             CFStringRef inVolumeTypeKey,
                                                             pid_t inAppProcessID,
                                                             CFStringRef inAppBundleID)
{
    CACFArray appVolumeChanges(true);

    auto addVolumeChange = [&] (pid_t pid, CFStringRef bundleID)
    {
        CACFDictionary appVolumeChange(true);

        appVolumeChange.AddSInt32(CFSTR(kBGMAppVolumesKey_ProcessID), pid);
        appVolumeChange.AddString(CFSTR(kBGMAppVolumesKey_BundleID), bundleID);
        appVolumeChange.AddSInt32(inVolumeTypeKey, inNewValue);

        appVolumeChanges.AppendDictionary(appVolumeChange.GetDict());
    };

    addVolumeChange(inAppProcessID, inAppBundleID);

    // Add the same change for each process the app is responsible for.
    for(CFStringRef responsibleBundleID : ResponsibleBundleIDsOf(inAppBundleID))
    {
        // Send -1 as the PID so this volume will only ever be matched by bundle ID.
        addVolumeChange(-1, responsibleBundleID);
    }

    CFPropertyListRef changesPList = appVolumeChanges.AsPropertyList();

    // Send the change to BGMDevice.
    SetPropertyData_CFType(kBGMAppVolumesAddress, changesPList);

    // Also send it to the instance of BGMDevice that handles UI sounds.
    mUISoundsBGMDevice.SetPropertyData_CFType(kBGMAppVolumesAddress, changesPList);
}

// This is a temporary solution that lets us control the volumes of some multiprocess apps, i.e.
// apps that play their audio from a process with a different bundle ID.
//
// We can't just check the child processes of the apps' main processes because they're usually
// created with launchd rather than being actual child processes. There's a private API to get the
// processes that an app is "responsible for", so we'll try to use it in the proper fix and only use
// this list if the API doesn't work.
//
// static
std::vector<CFStringRef>
BGMBackgroundMusicDevice::ResponsibleBundleIDsOf(CFStringRef inParentBundleID)
{
    std::map<CFStringRef, std::vector<CFStringRef>> bundleIDMap = {
            // Finder
            { CFSTR("com.apple.finder"), { CFSTR("com.apple.quicklook.ui.helper") } },
            // Safari
            { CFSTR("com.apple.Safari"), { CFSTR("com.apple.WebKit.WebContent") } },
            // Firefox
            { CFSTR("org.mozilla.firefox"), { CFSTR("org.mozilla.plugincontainer") } },
            // Firefox Nightly
            { CFSTR("org.mozilla.nightly"), { CFSTR("org.mozilla.plugincontainer") } },
            // VMWare Fusion
            { CFSTR("com.vmware.fusion"), { CFSTR("com.vmware.vmware-vmx") } },
            // Parallels
            { CFSTR("com.parallels.desktop.console"), { CFSTR("com.parallels.vm") } },
            // MPlayer OSX Extended
            { CFSTR("hu.mplayerhq.mplayerosx.extended"),
                    { CFSTR("ch.sttz.mplayerosx.extended.binaries.officialsvn") } }
    };

    // Parallels' VM "dock helper" apps have bundle IDs like
    // com.parallels.winapp.87f6bfc236d64d70a81c47f6243add4c.f5a25fdede514f7aa0a475a1873d3287.fs
    if(CFStringHasPrefix(inParentBundleID, CFSTR("com.parallels.winapp.")))
    {
        return { CFSTR("com.parallels.vm") };
    }

    return bundleIDMap[inParentBundleID];
}

#pragma clang assume_nonnull end


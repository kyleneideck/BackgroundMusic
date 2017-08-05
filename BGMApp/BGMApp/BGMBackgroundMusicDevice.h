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
//  BGMBackgroundMusicDevice.h
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//
//  The interface to BGMDevice, the main virtual device published by BGMDriver, and the second
//  instance of that device, which handles UI-related audio. In most cases, users of this class
//  should be able to think of it as representing a single device.
//
//  BGMDevice is the device that appears as "Background Music" in programs that list the output
//  devices, e.g. System Preferences. It receives the system's audio, processes it and sends it to
//  BGMApp by publishing an input stream. BGMApp then plays the audio on the user's real output
//  device.
//
//  See BGMDriver/BGMDriver/BGM_Device.h.
//

#ifndef BGMApp__BGMBackgroundMusicDevice
#define BGMApp__BGMBackgroundMusicDevice

// Superclass Includes
#include "BGMAudioDevice.h"

// STL Includes
#include <vector>


#pragma clang assume_nonnull begin

class BGMBackgroundMusicDevice
:
    public BGMAudioDevice
{

#pragma mark Construction/Destruction

public:
    /*!
     @throws CAException If BGMDevice is not found or the HAL returns an error when queried for
                         BGMDevice's current Audio Object ID.
     */
                        BGMBackgroundMusicDevice();
    virtual            ~BGMBackgroundMusicDevice();

#pragma mark Systemwide Default Device

    /*!
     Set BGMDevice as the default audio device for all processes.

     @throws CAException If the HAL responds with an error.
     */
    void                SetAsOSDefault();
    /*!
     Replace BGMDevice as the default device with the output device.

     @throws CAException If the HAL responds with an error.
     */
    void                UnsetAsOSDefault(AudioDeviceID inOutputDeviceID);

#pragma mark App Volumes

    /*!
     @param inVolume A value between kAppRelativeVolumeMinRawValue and kAppRelativeVolumeMaxRawValue
                     from BGM_Types.h. See kBGMAppVolumesKey_RelativeVolume in BGM_Types.h.
     @param inAppProcessID The ID of app's main process (or the process it uses to play audio, if
                           you've managed to figure that out). If an app has multiple audio
                           processes, you can just set the volume for each of them. Pass -1 to omit
                           this param.
     @param inAppBundleID The app's bundle ID. Pass null to omit this param.
     @throws CAException If the HAL returns an error when this function sends the volume change to
                         BGMDevice.
     */
    void                SetAppVolume(SInt32 inVolume,
                                     pid_t inAppProcessID,
                                     CFStringRef inAppBundleID);
    /*!
     @param inPanPosition A value between kAppPanLeftRawValue and kAppPanRightRawValue from
                          BGM_Types.h. A negative value has a higher proportion of left channel, and
                          a positive value has a higher proportion of right channel.
     @param inAppProcessID The ID of app's main process (or the process it uses to play audio, if
                           you've managed to figure that out). If an app has multiple audio
                           processes, you can just set the pan position for each of them. Pass -1 to
                           omit this param.
     @param inAppBundleID The app's bundle ID. Pass null to omit this param.
     @throws CAException If the HAL returns an error when this function sends the pan position
                         change to BGMDevice.
     */
    void                SetAppPanPosition(SInt32 inPanPosition,
                                          pid_t inAppProcessID,
                                          CFStringRef inAppBundleID);

private:
    void                SendAppVolumeOrPanToBGMDevice(SInt32 inNewValue,
                                                      CFStringRef inVolumeTypeKey,
                                                      pid_t inAppProcessID,
                                                      CFStringRef inAppBundleID);

    static std::vector<CFStringRef>
                        ResponsibleBundleIDsOf(CFStringRef inParentBundleID);

#pragma mark UI Sounds Instance

public:
    /*! @return The instance of BGMDevice that handles UI sounds. */
    BGMAudioDevice      GetUISoundsBGMDeviceInstance() const { return mUISoundsBGMDevice; }

private:
    /*! The instance of BGMDevice that handles UI sounds. */
    BGMAudioDevice      mUISoundsBGMDevice;

};

#pragma clang assume_nonnull end

#endif /* BGMApp__BGMBackgroundMusicDevice */


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
//  BGMOutputVolumeMenuItem.mm
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//

// Self Include
#import "BGMOutputVolumeMenuItem.h"

// Local Includes
#import "BGMAudioDevice.h"

// PublicUtility Includes
#import "CAException.h"
#import "CAPropertyAddress.h"

// System Includes
#import <CoreAudio/AudioHardware.h>


const float                    SLIDER_EPSILON = 1e-10f;
const AudioObjectPropertyScope SCOPE          = kAudioDevicePropertyScopeOutput;
const UInt32                   CHANNEL        = kMasterChannel;

@implementation BGMOutputVolumeMenuItem {
    BGMAudioDeviceManager* audioDevices;
    NSTextField* outputVolumeLabel;
}

// TODO: Update the UI when the output device is changed.
// TODO: Show the output device's icon next to its name.
// TODO: Disable the slider if the output device doesn't have a volume control.
// TODO: Should the menu (bgmMenu) hide after you change the output volume slider, like the normal
//       menu bar volume slider does?
// TODO: Move the output devices from Preferences to the main menu so they're slightly easier to
//       access?
// TODO: Update the screenshot in the README.
- (instancetype) initWithAudioDevices:(BGMAudioDeviceManager*)devices
                                 view:(NSView*)view
                               slider:(NSSlider*)slider
                          deviceLabel:(NSTextField*)label {
    if ((self = [super initWithTitle:@"" action:nil keyEquivalent:@""])) {
        audioDevices = devices;
        outputVolumeLabel = label;

        // Apply our custom view from MainMenu.xib.
        self.view = view;

        try {
            [self initSlider:slider];
            [self setOutputVolumeLabel];
        } catch (const CAException& e) {
            NSLog(@"BGMOutputVolumeMenuItem::initWithBGMMenu: Exception: %d", e.GetError());
        }
    }

    return self;
}

- (void) initSlider:(NSSlider*)slider {
    slider.target = self;
    slider.action = @selector(sliderChanged:);

    BGMAudioDevice bgmDevice = [audioDevices bgmDevice];

    // This block updates the value of the output volume slider.
    AudioObjectPropertyListenerBlock updateSlider =
        ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress* inAddresses) {
            // The docs for AudioObjectPropertyListenerBlock say inAddresses will always contain
            // at least one property the block is listening to, so there's no need to check
            // inAddresses.
            #pragma unused (inNumberAddresses, inAddresses)

            try {
                if (bgmDevice.GetMuteControlValue(SCOPE, kMasterChannel)) {
                    // The output device is muted, so show the volume as 0 on the slider.
                    slider.doubleValue = 0.0;
                } else {
                    // The slider values and volume values are both from 0 to 1, so we can use the
                    // volume as is.
                    slider.doubleValue =
                        bgmDevice.GetVolumeControlScalarValue(SCOPE, kMasterChannel);
                }
            } catch (const CAException& e) {
                NSLog(@"BGMOutputVolumeMenuItem::initSlider: Failed to update slider. (%d)",
                      e.GetError());
            }
        };

    // Initialise the slider. (The args are ignored.)
    updateSlider(0, {});

    // Register a listener that will update the slider when the user changes the volume from
    // somewhere else.
    audioDevices.bgmDevice.AddPropertyListenerBlock(
        CAPropertyAddress(kAudioDevicePropertyVolumeScalar, SCOPE),
        dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0),
        updateSlider);

    // Register the same listener for mute/unmute.
    audioDevices.bgmDevice.AddPropertyListenerBlock(
        CAPropertyAddress(kAudioDevicePropertyMute, SCOPE),
        dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0),
        updateSlider);
}

// Sets the label to the name of the output device.
- (void) setOutputVolumeLabel {
    BGMAudioDevice device = audioDevices.outputDevice;

    if (device.HasDataSourceControl(SCOPE, CHANNEL)) {
        UInt32 dataSourceID = device.GetCurrentDataSourceID(SCOPE, CHANNEL);

        outputVolumeLabel.stringValue =
            (__bridge_transfer NSString*)device.CopyDataSourceNameForID(SCOPE,
                                                                        CHANNEL,
                                                                        dataSourceID);
        
        outputVolumeLabel.toolTip = (__bridge_transfer NSString*)device.CopyName();
    } else {
        outputVolumeLabel.stringValue = (__bridge_transfer NSString*)device.CopyName();
    }

    // Take the label out of the accessibility hierarchy, which also moves the slider up a level.
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101000  // MAC_OS_X_VERSION_10_10
    if ([outputVolumeLabel.cell respondsToSelector:@selector(setAccessibilityElement:)]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
        outputVolumeLabel.cell.accessibilityElement = NO;
#pragma clang diagnostic pop
    }
#endif
}

// Called when the user slides the slider.
- (IBAction) sliderChanged:(NSSlider*)sender {
    float newValue = sender.floatValue;

    DebugMsg("BGMOutputVolumeMenuItem::sliderChanged: New value: %f", newValue);

    // Update BGMDevice's volume to the new value selected by the user.
    try {
        // The slider values and volume values are both from 0.0f to 1.0f, so we can use the slider
        // value as is.
        audioDevices.bgmDevice.SetVolumeControlScalarValue(SCOPE, CHANNEL, newValue);

        // Mute BGMDevice if they set the slider to zero, and unmute it for non-zero. Muting makes
        // sure the audio doesn't play very quietly instead being completely silent. This matches
        // the behaviour of the Volume menu built-in to macOS.
        if (audioDevices.bgmDevice.HasMuteControl(SCOPE, CHANNEL)) {
            audioDevices.bgmDevice.SetMuteControlValue(SCOPE, CHANNEL, (newValue < SLIDER_EPSILON));
        }
    } catch (const CAException& e) {
        NSLog(@"BGMOutputVolumeMenuItem::sliderChanged: Failed to set volume (%d)", e.GetError());
    }
}

@end


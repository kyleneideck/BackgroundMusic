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
#import "BGM_Utils.h"
#import "BGMAudioDevice.h"

// PublicUtility Includes
#import "CAException.h"
#import "CAPropertyAddress.h"

// System Includes
#import <CoreAudio/AudioHardware.h>


const float                    kSliderEpsilon           = 1e-10f;
const AudioObjectPropertyScope kScope                   = kAudioDevicePropertyScopeOutput;
NSString* const __nonnull      kGenericOutputDeviceName = @"Output Device";

@implementation BGMOutputVolumeMenuItem {
    BGMAudioDeviceManager* audioDevices;
    NSTextField* outputVolumeLabel;
    NSSlider* volumeSlider;
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
        volumeSlider = slider;

        // Apply our custom view from MainMenu.xib.
        self.view = view;

        [self initSlider];
        [self setOutputVolumeLabel];
    }

    return self;
}

- (void) initSlider {
    BGMAssert([NSThread isMainThread],
              "initSlider must be called from the main thread because it calls UI functions.");

    volumeSlider.target = self;
    volumeSlider.action = @selector(sliderChanged:);

    // Initialise the slider.
    [self updateVolumeSlider];

    // Register a listener that will update the slider when the user changes the volume or
    // mutes/unmutes their audio.
    AudioObjectPropertyListenerBlock updateSlider =
        ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress* inAddresses) {
            // The docs for AudioObjectPropertyListenerBlock say inAddresses will always contain
            // at least one property the block is listening to, so there's no need to check
            // inAddresses.
            #pragma unused (inNumberAddresses, inAddresses)

            dispatch_async(dispatch_get_main_queue(), ^{
                [self updateVolumeSlider];
            });
        };

    // Instead of swallowing exceptions, we could try again later, but I doubt it would be worth the
    // effort. And the documentation doesn't actually explain what could cause this to fail.
    BGMLogAndSwallowExceptions("BGMOutputVolumeMenuItem::initSlider", ([&] {
        // Register the listener to receive volume notifications.
        audioDevices.bgmDevice.AddPropertyListenerBlock(
            CAPropertyAddress(kAudioDevicePropertyVolumeScalar, kScope),
            dispatch_get_main_queue(),
            updateSlider);

        // Register the same listener for mute/unmute notifications.
        audioDevices.bgmDevice.AddPropertyListenerBlock(
            CAPropertyAddress(kAudioDevicePropertyMute, kScope),
            dispatch_get_main_queue(),
            updateSlider);
    }));
}

// Updates the value of the output volume slider. Should only be called on the main thread because
// it calls UI functions.
- (void) updateVolumeSlider {
    BGMAssert([[NSThread currentThread] isMainThread], "updateVolumeSlider on non-main thread.");

    BGMAudioDevice bgmDevice = [audioDevices bgmDevice];

    // BGMDevice should never return an error for these calls, so we just swallow any exceptions and
    // give up. (That said, we do check mute last so that, if it did throw, it wouldn't affect the
    // more important calls.)
    BGMLogAndSwallowExceptions("BGMOutputVolumeMenuItem::updateVolumeSlider", ([&] {
        BOOL hasVolume = bgmDevice.HasSettableMasterVolume(kScope);

        // If the device doesn't have a master volume control, we disable the slider and set it to
        // full (or to zero, if muted).
        volumeSlider.enabled = hasVolume;

        if (hasVolume) {
            // Set the slider to the current output volume. The slider values and volume values are
            // both from 0 to 1, so we can use the volume as is.
            volumeSlider.doubleValue =
                bgmDevice.GetVolumeControlScalarValue(kScope, kMasterChannel);
        } else {
            volumeSlider.doubleValue = 1.0;
        }

        // Set the slider to zero if the device is muted.
        if (bgmDevice.HasSettableMasterMute(kScope) &&
            bgmDevice.GetMuteControlValue(kScope, kMasterChannel)) {
            volumeSlider.doubleValue = 0.0;
        }
    }));
};

- (void) outputDeviceDidChange {
    dispatch_async(dispatch_get_main_queue(), ^{
        // Update the label to use the name of the new output device.
        [self setOutputVolumeLabel];
        // Set the slider to the volume of the new device.
        [self updateVolumeSlider];
    });
}

// Sets the label to the name of the output device. Falls back to a generic name if the device
// returns an error when queried.
- (void) setOutputVolumeLabel {
    BGMAudioDevice device = audioDevices.outputDevice;
    BOOL didSetLabel = NO;

    try {
        if (device.HasDataSourceControl(kScope, kMasterChannel)) {
            // The device has datasources, so use the current datasource's name like macOS does.
            UInt32 dataSourceID = device.GetCurrentDataSourceID(kScope, kMasterChannel);

            outputVolumeLabel.stringValue =
                (__bridge_transfer NSString*)device.CopyDataSourceNameForID(kScope,
                                                                            kMasterChannel,
                                                                            dataSourceID);
            didSetLabel = YES;  // So we know not to change the text if setting the tooltip fails.

            outputVolumeLabel.toolTip = (__bridge_transfer NSString*)device.CopyName();
        } else {
            outputVolumeLabel.stringValue = (__bridge_transfer NSString*)device.CopyName();
        }
    } catch (const CAException& e) {
        BGMLogException(e);

        // The device returned an error, so set the label to a generic device name, since we don't
        // want to leave it set to the previous device's name.
        outputVolumeLabel.toolTip = nil;

        if (!didSetLabel) {
            outputVolumeLabel.stringValue = kGenericOutputDeviceName;
        }
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
        audioDevices.bgmDevice.SetVolumeControlScalarValue(kScope, kMasterChannel, newValue);

        // Mute BGMDevice if they set the slider to zero, and unmute it for non-zero. Muting makes
        // sure the audio doesn't play very quietly instead being completely silent. This matches
        // the behaviour of the Volume menu built-in to macOS.
        if (audioDevices.bgmDevice.HasMuteControl(kScope, kMasterChannel)) {
            audioDevices.bgmDevice.SetMuteControlValue(kScope,
                                                       kMasterChannel,
                                                       (newValue < kSliderEpsilon));
        }
    } catch (const CAException& e) {
        NSLog(@"BGMOutputVolumeMenuItem::sliderChanged: Failed to set volume (%d)", e.GetError());
    }
}

@end


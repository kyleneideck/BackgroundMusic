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
//  Copyright © 2017-2019, 2026 Kyle Neideck
//

// Self Include
#import "BGMOutputVolumeMenuItem.h"

// Local Includes
#import "BGM_Utils.h"
#import "BGMAudioDevice.h"
#import "BGMVolumeChangeListener.h"

// PublicUtility Includes
#import "CAException.h"

// System Includes
#import <CoreAudio/AudioHardware.h>


#pragma clang assume_nonnull begin

const float                    kSliderEpsilon = 1e-10f;
const AudioObjectPropertyScope kScope         = kAudioDevicePropertyScopeOutput;
NSString* const __nonnull      kMasterVolumeLabel = @"Master";

@implementation BGMOutputVolumeMenuItem {
    BGMAudioDeviceManager* audioDevices;
    NSTextField* deviceLabel;
    NSSlider* volumeSlider;
    BGMVolumeChangeListener* volumeChangeListener;
}

// TODO: Should the menu (bgmMenu) hide after you change the output volume slider, like the normal
//       menu bar volume slider does?
// TODO: Update the screenshot in the README at some point.
- (instancetype) initWithAudioDevices:(BGMAudioDeviceManager*)devices
                                 view:(NSView*)view
                               slider:(NSSlider*)slider
                          deviceLabel:(NSTextField*)label {
    if ((self = [super initWithTitle:@"" action:nil keyEquivalent:@""])) {
        audioDevices = devices;
        deviceLabel = label;
        volumeSlider = slider;

        // volumeChangeListener is initialised in the method called below.

        // Apply our custom view from MainMenu.xib.
        self.view = view;

        // Set up the UI components in the view.
        [self initSlider];
        [self updateLabelAndToolTip];
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
    BGMOutputVolumeMenuItem* __weak weakSelf = self;
    volumeChangeListener = new BGMVolumeChangeListener(audioDevices.bgmDevice, [=] {
        [weakSelf updateVolumeSlider];
    });
}

// Updates the value of the master volume slider. Should only be called on the main thread because
// it calls UI functions.
- (void) updateVolumeSlider {
    BGMAssert([[NSThread currentThread] isMainThread], "updateVolumeSlider on non-main thread.");

    BGMAudioDevice bgmDevice = [audioDevices bgmDevice];

    // BGMDevice should never return an error for these calls, so we just swallow any exceptions and
    // give up. (That said, we do check mute last so that, if it did throw, it wouldn't affect the
    // more important calls.)
    BGMLogAndSwallowExceptions("BGMOutputVolumeMenuItem::updateVolumeSlider", ([&] {
        BOOL hasVolume = bgmDevice.HasSettableMainVolume(kScope);

        // If the device doesn't have a main volume control, we disable the slider and set it to
        // full (or to zero, if muted).
        volumeSlider.enabled = hasVolume;

        if (hasVolume) {
            // Set the slider to the current master volume. The slider values and volume values are
            // both from 0 to 1, so we can use the volume as is.
            volumeSlider.doubleValue =
                bgmDevice.GetVolumeControlScalarValue(kScope, kMainChannel);
        } else {
            volumeSlider.doubleValue = 1.0;
        }

        // Set the slider to zero if the device is muted.
        if (bgmDevice.HasSettableMainMute(kScope) &&
            bgmDevice.GetMuteControlValue(kScope, kMainChannel)) {
            volumeSlider.doubleValue = 0.0;
        }
    }));
}

- (void) outputDeviceDidChange {
    // The master volume is independent of the output devices, so there's nothing device-specific to
    // update. We just make sure the slider is up to date.
    dispatch_async(dispatch_get_main_queue(), ^{
        [self updateVolumeSlider];
    });
}

// Sets the label to a static "Master" name. (This slider controls the master volume, which is
// applied to all of the enabled output devices by BGMPlayThrough.)
- (void) updateLabelAndToolTip {
    self.toolTip = nil;
    deviceLabel.stringValue = kMasterVolumeLabel;

    DebugMsg("BGMOutputVolumeMenuItem::updateLabelAndToolTip: Label: '%s'",
             deviceLabel.stringValue.UTF8String);

    // Take the label out of the accessibility hierarchy, which also moves the slider up a level.
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101000  // MAC_OS_X_VERSION_10_10
    if ([deviceLabel.cell respondsToSelector:@selector(setAccessibilityElement:)]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
        deviceLabel.cell.accessibilityElement = NO;
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
        audioDevices.bgmDevice.SetVolumeControlScalarValue(kScope, kMainChannel, newValue);

        // Mute BGMDevice if they set the slider to zero, and unmute it for non-zero. Muting makes
        // sure the audio doesn't play very quietly instead being completely silent. This matches
        // the behaviour of the Volume menu built-in to macOS.
        if (audioDevices.bgmDevice.HasMuteControl(kScope, kMainChannel)) {
            audioDevices.bgmDevice.SetMuteControlValue(kScope,
                                                       kMainChannel,
                                                       (newValue < kSliderEpsilon));
        }
    } catch (const CAException& e) {
        NSLog(@"BGMOutputVolumeMenuItem::sliderChanged: Failed to set volume (%d)", e.GetError());
    }
}

@end

#pragma clang assume_nonnull end

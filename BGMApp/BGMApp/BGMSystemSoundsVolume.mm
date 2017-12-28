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
//  BGMSystemSoundsVolume.mm
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//

// Self Include
#import "BGMSystemSoundsVolume.h"

// Local Includes
#import "BGM_Types.h"
#import "BGM_Utils.h"


#pragma clang assume_nonnull begin

// TODO: It's a bit confusing that this slider's default position is all the way right, but the App
//       Volumes sliders default to 50%. After you move the slider there's no way to tell how to put
//       it back to its normal position.

NSString* const kMenuItemToolTip =
    @"Alerts, notification sounds, etc. Usually short. Can be played by any app.";

@implementation BGMSystemSoundsVolume {
    BGMAudioDevice uiSoundsDevice;
    NSSlider* volumeSlider;
}

- (instancetype) initWithUISoundsDevice:(BGMAudioDevice)inUISoundsDevice
                                   view:(NSView*)inView
                                 slider:(NSSlider*)inSlider {
    if ((self = [super init])) {
        uiSoundsDevice = inUISoundsDevice;
        volumeSlider = inSlider;

        _menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        _menuItem.toolTip = kMenuItemToolTip;

        // Apply our custom view from MainMenu.xib. It's very similar to the one for app volumes.
        _menuItem.view = inView;

        try {
            volumeSlider.floatValue =
                uiSoundsDevice.GetVolumeControlScalarValue(kAudioObjectPropertyScopeOutput,
                                                           kMasterChannel);
        } catch (const CAException& e) {
            BGMLogException(e);
            volumeSlider.floatValue = 1.0f;  // Full volume
        }

        volumeSlider.target = self;
        volumeSlider.action = @selector(systemSoundsSliderChanged:);
    }

    return self;
}

- (void) systemSoundsSliderChanged:(id)sender {
    #pragma unused(sender)

    float sliderLevel = volumeSlider.floatValue;

    BGMAssert((sliderLevel >= 0.0f) && (sliderLevel <= 1.0f), "Invalid value from slider");
    DebugMsg("BGMSystemSoundsVolume::systemSoundsSliderChanged: UI sounds volume: %f", sliderLevel);

    BGMLogAndSwallowExceptions("BGMSystemSoundsVolume::systemSoundsSliderChanged", ([&] {
        uiSoundsDevice.SetVolumeControlScalarValue(kAudioObjectPropertyScopeOutput,
                                                   kMasterChannel,
                                                   sliderLevel);
    }));
}

@end

#pragma clang assume_nonnull end


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
//  BGMAppUITests.mm
//  BGMAppUITests
//
//  Copyright © 2017, 2018 Kyle Neideck
//
//  You might want to use Xcode's UI test recording feature if you add new tests.
//

// Local Includes
#import "BGM_TestUtils.h"
#import "BGM_Types.h"
#import "BGMBackgroundMusicDevice.h"

// Scripting Bridge Includes
#import "BGMApp.h"


// TODO: Skip these tests if macOS SDK 10.11 or higher isn't available.
// TODO: Mock BGMDevice and music players.

#if __clang_major__ >= 9

@interface BGMAppUITests : XCTestCase
@end

@implementation BGMAppUITests {
    // The BGMApp instance.
    XCUIApplication* app;

    // Convenience vars.
    //
    // The menu bar icon. (Called the status bar icon in some places.)
    XCUIElement* icon;
    // The menu items in the main menu.
    XCUIElementQuery* menuItems;
    // The Preferences menu item.
    XCUIElement* prefs;
}

- (void) setUp {
    [super setUp];
    
    // In UI tests it is usually best to stop immediately when a failure occurs.
    self.continueAfterFailure = NO;

    // Set up the app object and some convenience vars.
    app = [[XCUIApplication alloc] init];
    menuItems = app.menuBars.menuItems;
    prefs = menuItems[@"Preferences"];
    icon = [app.menuBars childrenMatchingType:XCUIElementTypeStatusItem].element;

    // TODO: Make sure BGMDevice isn't set as the OS X default device before launching BGMApp.

    // Tell BGMApp not to load/store user defaults (settings) and to use
    // NSApplicationActivationPolicyRegular. If it used the "accessory" policy as usual, the tests
    // would fail to start because of a bug in Xcode.
    app.launchArguments = @[ @"--no-persistent-data", @"--show-dock-icon" ];

    // Launch BGMApp.
    [app launch];

     if (![icon waitForExistenceWithTimeout:1.0]) {
        // The status bar icon/button has this type when using older versions of XCTest, so try
        // both. (Actually, it might depend on the macOS or Xcode version. I'm not sure.)
        XCUIElement* iconOldType =
                [app.menuBars childrenMatchingType:XCUIElementTypeMenuBarItem].element;
         if (![iconOldType waitForExistenceWithTimeout:5.0]) {
             icon = iconOldType;
         }
    }

    // Wait for the initial elements.
    XCTAssert([app waitForExistenceWithTimeout:10.0]);
    XCTAssert([icon waitForExistenceWithTimeout:10.0]);
}

- (void) tearDown {
    // Click the quit menu item.
    if (!menuItems.count) {
        [icon click];
    }

    [menuItems[@"Quit Background Music"] click];

    // BGMApp should quit.
    XCTAssert(!app.exists);
    
    [super tearDown];
}

- (void) testCycleOutputDevices {
    const int NUM_CYCLES = 2;

    // sbApp lets us use AppleScript to query BGMApp and check the test has made the changes to its
    // settings we expect.
    BGMAppApplication* sbApp = [SBApplication applicationWithBundleIdentifier:@kBGMAppBundleID];

    // Get macOS to show the "'Xcode' wants to control 'Background Music'" dialog before we start
    // the test so it doesn't interrupt it.
    [[sbApp selectedOutputDevice] name];

    // Click the icon to open the main menu.
    [icon click];

    // Get the list of output devices from the main menu.
    // BGMOutputDeviceMenuSection::createMenuItemForDevice gives every output device menu item the
    // accessibility identifier "output-device" so we can find all of them here.
    NSArray<XCUIElement*>* outputDeviceMenuItems =
        [menuItems matchingIdentifier:@"output-device"].allElementsBoundByIndex;

    // For debugging certain issues, it can be useful to repeatedly switch between two
    // devices:
    // outputDeviceMenuItems = [outputDeviceMenuItems subarrayWithRange:NSMakeRange(0,2)];

    XCTAssertGreaterThan(outputDeviceMenuItems.count, 0);

    // Click the last device to close the menu again.
    [outputDeviceMenuItems.lastObject click];

    for (int i = 0; i < NUM_CYCLES; i++) {
        // Select each output device.
        for (XCUIElement* item in outputDeviceMenuItems) {
            [icon click];
            [item click];

            // Assert that the device we clicked is the selected device now.
            for (BGMAppOutputDevice* device in [sbApp outputDevices]) {
                // TODO: This seems a bit fragile. Would it still work with long device names?
                if ([device.name isEqualToString:[item title]]) {
                    XCTAssert(device.selected);
                } else {
                    XCTAssertFalse(device.selected);
                }
            }
        }
    }
}

- (void) testSelectMusicPlayer {
    // Select VLC as the music player.
    [icon click];
    [prefs hover];
    [prefs.menuItems[@"VLC"] click];

    // The name of the Auto-pause menu item should change. Also check the accessibility identifier.
    [icon click];
    XCTAssertEqualObjects(menuItems[@"Auto-pause VLC"].identifier, @"Auto-pause enabled");
    
    // Select iTunes as the music player.
    [prefs hover];
    [prefs.menuItems[@"iTunes"] click];

    // The name of the Auto-pause menu item should change back.
    [icon click];
    XCTAssert(menuItems[@"Auto-pause iTunes"].exists);
}

- (void) testOutputVolumeSlider {
    const AudioObjectPropertyScope scope = kAudioDevicePropertyScopeOutput;
    const UInt32 channel = kMasterChannel;

    [icon click];
    
    XCUIElement* slider = menuItems.sliders[@"Output Volume"];

    // Try to slide the slider all the way to the right.
    [slider adjustToNormalizedSliderPosition:1.0f];

    // For whatever reason, XCTest usually doesn't quite make it to the position you ask for. So
    // just check that it got close enough.
    XCTAssertGreaterThan(slider.normalizedSliderPosition, 0.9f);

    // BGMDevice's volume should be set to its max, or as close as XCTest was able to get the
    // slider. Probably shouldn't be comparing floats for equality like this, but it's working fine
    // so far.
    BGMBackgroundMusicDevice bgmDevice;
    XCTAssertEqual(slider.normalizedSliderPosition,
                   bgmDevice.GetVolumeControlScalarValue(scope, channel));

    // Try to slide the slider all the way to the left.
    [slider adjustToNormalizedSliderPosition:0.0f];

    // BGMDevice's volume should be set to the new value of the slider.
    XCTAssertLessThan(slider.normalizedSliderPosition, 0.1f);
    XCTAssertEqual(slider.normalizedSliderPosition,
                   bgmDevice.GetVolumeControlScalarValue(scope, channel));

    // Try to slide the slider to 75%.
    [slider adjustToNormalizedSliderPosition:0.75f];

    // BGMDevice's volume should be set to the new value of the slider, about 75% of its max.
    XCTAssertEqual(slider.normalizedSliderPosition,
                   bgmDevice.GetVolumeControlScalarValue(scope, channel));

    // BGMDevice should be unmuted.
    XCTAssertEqual(false, bgmDevice.GetMuteControlValue(scope, channel));

    // Set BGMDevice's volume to its min.
    bgmDevice.SetVolumeControlScalarValue(scope, channel, 0.0f);

    // The slider should be set to its min value. Use a wait for this check because the change
    // happens asynchronously.
    [self expectationForPredicate:[NSPredicate predicateWithFormat:@"normalizedSliderPosition == 0"]
              evaluatedWithObject:slider
                          handler:nil];
    [self waitForExpectationsWithTimeout:10.0 handler:nil];

    XCTAssertEqual(0.0f, slider.normalizedSliderPosition);

    // Click the slider without changing it to simulate the user setting the slider to zero.
    [slider adjustToNormalizedSliderPosition:0.0f];

    // BGMDevice's volume should still be set to its min.
    XCTAssertEqual(0.0f, bgmDevice.GetVolumeControlScalarValue(scope, channel));

    // BGMDevice should now be muted.
    XCTAssertEqual(true, bgmDevice.GetMuteControlValue(scope, channel));
}

@end

#endif /* __clang_major__ >= 9 */


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
//  BGMAppUITests.m
//  BGMAppUITests
//
//  Copyright © 2017 Kyle Neideck
//
//  You might want to use Xcode's UI test recording feature if you add new tests.
//

// Local Includes
#import "BGM_TestUtils.h"
#import "BGM_Types.h"

// Scripting Bridge Includes
#import "BGMApp.h"


// TODO: Skip these tests if macOS SDK 10.11 or higher isn't available.
// TODO: Mock BGMDevice and music players.

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
    icon = [app.menuBars childrenMatchingType:XCUIElementTypeMenuBarItem].element;
    prefs = menuItems[@"Preferences"];

    // TODO: Make sure BGMDevice isn't set as the OS X default device before launching BGMApp.

    // Tell BGMApp not to load/store user defaults (settings) and to use
    // NSApplicationActivationPolicyRegular. If it used the "accessory" policy as usual, the tests
    // would fail to start because of a bug in Xcode.
    app.launchArguments = @[ @"--no-persistent-data", @"--show-dock-icon" ];

    // Launch BGMApp.
    [app launch];
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

    // Get the list of output devices from the preferences menu.
    [icon click];
    [prefs hover];
    NSArray<XCUIElement*>* outputDeviceMenuItems = [self outputDeviceMenuItems];

    // For debugging certain issues, it can be useful to repeatedly switch between two
    // devices:
    // outputDeviceMenuItems = [outputDeviceMenuItems subarrayWithRange:NSMakeRange(0,2)];

    XCTAssertGreaterThan(outputDeviceMenuItems.count, 0);

    // Click the last device to close the menu again.
    [outputDeviceMenuItems.lastObject click];

    BGMAppApplication* sbApp = [SBApplication applicationWithBundleIdentifier:@kBGMAppBundleID];

    for (int i = 0; i < NUM_CYCLES; i++) {
        // Select each output device.
        for (XCUIElement* item in outputDeviceMenuItems) {
            [icon click];
            [prefs hover];

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

// Find the menu items for the output devices in the preferences menu.
- (NSArray<XCUIElement*>*) outputDeviceMenuItems {
    NSArray<XCUIElement*>* items = @[];
    BOOL inOutputDeviceSection = NO;

    for (int i = 0; i < prefs.menuItems.count; i++) {
        XCUIElement* menuItem = [prefs.menuItems elementBoundByIndex:i];

        if ([menuItem.title isEqual:@"Output Device"]) {
            inOutputDeviceSection = YES;
        } else if (inOutputDeviceSection) {
            // Assume that finding a separator menu item means we've reached the end of the section.
            if (((NSMenuItem*)menuItem.value).separatorItem || [menuItem.title isEqual:@""]) {
                break;
            }

            items = [items arrayByAddingObject:menuItem];
        }
    }

    return items;
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

@end


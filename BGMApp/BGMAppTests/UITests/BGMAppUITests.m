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
//  You'll probably want to use Xcode's UI test recording feature if you add new tests.
//

// Local includes
#import "BGM_TestUtils.h"


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


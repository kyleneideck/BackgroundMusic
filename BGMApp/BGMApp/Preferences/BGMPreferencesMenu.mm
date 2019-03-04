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
//  BGMPreferencesMenu.mm
//  BGMApp
//
//  Copyright © 2016, 2018, 2019 Kyle Neideck
//

// Self Include
#import "BGMPreferencesMenu.h"

// Local Includes
#import "BGMAutoPauseMusicPrefs.h"
#import "BGMAboutPanel.h"


NS_ASSUME_NONNULL_BEGIN

// Interface Builder tags
static NSInteger const kPreferencesMenuItemTag = 1;
static NSInteger const kBGMIconMenuItemTag     = 2;
static NSInteger const kVolumeIconMenuItemTag  = 3;
static NSInteger const kAboutPanelMenuItemTag  = 4;

@implementation BGMPreferencesMenu {
    // Menu sections/items
    BGMAutoPauseMusicPrefs* autoPauseMusicPrefs;
    NSMenuItem* bgmIconMenuItem;
    NSMenuItem* volumeIconMenuItem;

    // The menu item you press to open BGMApp's main menu.
    BGMStatusBarItem* statusBarItem;

    // The About Background Music window
    BGMAboutPanel* aboutPanel;
}

- (id) initWithBGMMenu:(NSMenu*)inBGMMenu
          audioDevices:(BGMAudioDeviceManager*)inAudioDevices
          musicPlayers:(BGMMusicPlayers*)inMusicPlayers
         statusBarItem:(BGMStatusBarItem*)inStatusBarItem
            aboutPanel:(NSPanel*)inAboutPanel
 aboutPanelLicenseView:(NSTextView*)inAboutPanelLicenseView {
    if ((self = [super init])) {
        NSMenu* prefsMenu = [[inBGMMenu itemWithTag:kPreferencesMenuItemTag] submenu];
        
        autoPauseMusicPrefs = [[BGMAutoPauseMusicPrefs alloc] initWithPreferencesMenu:prefsMenu
                                                                         audioDevices:inAudioDevices
                                                                         musicPlayers:inMusicPlayers];
        
        aboutPanel = [[BGMAboutPanel alloc] initWithPanel:inAboutPanel licenseView:inAboutPanelLicenseView];

        statusBarItem = inStatusBarItem;

        // Set up the menu items under the "Status Bar Icon" heading.
        bgmIconMenuItem = [prefsMenu itemWithTag:kBGMIconMenuItemTag];
        bgmIconMenuItem.state =
                (statusBarItem.icon == BGMFermataStatusBarIcon) ? NSOnState : NSOffState;
        [bgmIconMenuItem setTarget:self];
        [bgmIconMenuItem setAction:@selector(useBGMStatusBarIcon)];

        volumeIconMenuItem = [prefsMenu itemWithTag:kVolumeIconMenuItemTag];
        volumeIconMenuItem.state =
                (statusBarItem.icon == BGMVolumeStatusBarIcon) ? NSOnState : NSOffState;
        [volumeIconMenuItem setTarget:self];
        [volumeIconMenuItem setAction:@selector(useVolumeStatusBarIcon)];

        // Set up the "About Background Music" menu item
        NSMenuItem* aboutMenuItem = [prefsMenu itemWithTag:kAboutPanelMenuItemTag];
        [aboutMenuItem setTarget:aboutPanel];
        [aboutMenuItem setAction:@selector(show)];
    }
    
    return self;
}

- (void) useBGMStatusBarIcon {
    // Change the icon.
    statusBarItem.icon = BGMFermataStatusBarIcon;

    // Select/deselect the menu items.
    bgmIconMenuItem.state = NSOnState;
    volumeIconMenuItem.state = NSOffState;
}

- (void) useVolumeStatusBarIcon {
    // TODO: Maybe we should show a message that tells the user how to hide the built-in volume
    //       icon. They probably won't want two status bar items that look the same. Or we might be
    //       able to automatically hide the built-in icon while BGMApp is running and show it again
    //       when BGMApp is closed.

    // Change the icon.
    statusBarItem.icon = BGMVolumeStatusBarIcon;

    // Select/deselect the menu items.
    bgmIconMenuItem.state = NSOffState;
    volumeIconMenuItem.state = NSOnState;
}

@end

NS_ASSUME_NONNULL_END


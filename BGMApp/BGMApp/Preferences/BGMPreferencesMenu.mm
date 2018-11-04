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
//  Copyright © 2016, 2018 Kyle Neideck
//

// Self Include
#import "BGMPreferencesMenu.h"

// Local Includes
#import "BGMAutoPauseMusicPrefs.h"
#import "BGMAboutPanel.h"


NS_ASSUME_NONNULL_BEGIN

// Interface Builder tags
static NSInteger const kPreferencesMenuItemTag = 1;
static NSInteger const kAboutPanelMenuItemTag = 3;

@implementation BGMPreferencesMenu {
    // Menu sections
    BGMAutoPauseMusicPrefs* autoPauseMusicPrefs;
    
    // The About Background Music window
    BGMAboutPanel* aboutPanel;
}

- (id) initWithBGMMenu:(NSMenu*)inBGMMenu
          audioDevices:(BGMAudioDeviceManager*)inAudioDevices
          musicPlayers:(BGMMusicPlayers*)inMusicPlayers
            aboutPanel:(NSPanel*)inAboutPanel
 aboutPanelLicenseView:(NSTextView*)inAboutPanelLicenseView {
    if ((self = [super init])) {
        NSMenu* prefsMenu = [[inBGMMenu itemWithTag:kPreferencesMenuItemTag] submenu];
        
        autoPauseMusicPrefs = [[BGMAutoPauseMusicPrefs alloc] initWithPreferencesMenu:prefsMenu
                                                                         audioDevices:inAudioDevices
                                                                         musicPlayers:inMusicPlayers];
        
        aboutPanel = [[BGMAboutPanel alloc] initWithPanel:inAboutPanel licenseView:inAboutPanelLicenseView];
        
        // Set up the "About Background Music" menu item
        NSMenuItem* aboutMenuItem = [prefsMenu itemWithTag:kAboutPanelMenuItemTag];
        [aboutMenuItem setTarget:aboutPanel];
        [aboutMenuItem setAction:@selector(show)];
    }
    
    return self;
}

@end

NS_ASSUME_NONNULL_END


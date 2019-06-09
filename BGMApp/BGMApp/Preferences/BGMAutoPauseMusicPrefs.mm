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
//  BGMAutoPauseMusicPrefs.mm
//  BGMApp
//
//  Copyright © 2016, 2019 Kyle Neideck
//

// Self Includes
#import "BGMAutoPauseMusicPrefs.h"

// Local Includes
#import "BGM_Types.h"
#import "BGMMusicPlayer.h"


#pragma clang assume_nonnull begin

static float const kMenuItemIconScalingFactor = 1.15f;
static NSInteger const kPrefsMenuAutoPauseHeaderTag = 1;

@implementation BGMAutoPauseMusicPrefs {
    BGMAudioDeviceManager* audioDevices;
    BGMMusicPlayers* musicPlayers;
    NSMenu* prefsMenu;
    NSArray<NSMenuItem*>* musicPlayerMenuItems;
}

- (id) initWithPreferencesMenu:(NSMenu*)inPrefsMenu
                  audioDevices:(BGMAudioDeviceManager*)inAudioDevices
                  musicPlayers:(BGMMusicPlayers*)inMusicPlayers {
    if ((self = [super init])) {
        prefsMenu = inPrefsMenu;
        audioDevices = inAudioDevices;
        musicPlayers = inMusicPlayers;
        
        musicPlayerMenuItems = @[];
        
        [self initPreferencesMenuSection];
    }
    
    return self;
}

- (void) initPreferencesMenuSection {
    // Add the menu items related to auto-pausing music to the Preferences submenu
    
    // The index to start inserting music player menu items at
    NSInteger musicPlayerItemsIndex = [prefsMenu indexOfItemWithTag:kPrefsMenuAutoPauseHeaderTag] + 1;
    
    // Insert the menu items used to change the music player app.
    for (id<BGMMusicPlayer> musicPlayer in musicPlayers.musicPlayers) {
        // Create an menu item for this music player.
        NSMenuItem* menuItem = [prefsMenu insertItemWithTitle:musicPlayer.name
                                                       action:@selector(handleMusicPlayerChange:)
                                                keyEquivalent:@""
                                                      atIndex:musicPlayerItemsIndex];
        menuItem.toolTip = musicPlayer.toolTip;
        
        musicPlayerMenuItems = [musicPlayerMenuItems arrayByAddingObject:menuItem];
        
        // Associate the music player with the menu item
        menuItem.representedObject = musicPlayer;
        
        // Show the menu item for the selected music player as selected
        if (musicPlayers.selectedMusicPlayer == musicPlayer) {
            menuItem.state = NSOnState;
        }
        
        // Set the menu item's icon
        NSImage* __nullable icon = musicPlayer.icon;
        if (icon == nil) {
            // Set a blank icon so the text lines up
            icon = [NSImage new];
        }
        
        // Size the icon relative to the size of the item's text
        CGFloat length = [NSFont menuBarFontOfSize:0].pointSize * kMenuItemIconScalingFactor;
        icon.size = NSMakeSize(length, length);
        menuItem.image = icon;
        
        menuItem.target = self;
        menuItem.indentationLevel = 1;
    }
}

- (void) handleMusicPlayerChange:(NSMenuItem*)sender {
    // Set the new music player as the selected music player
    id<BGMMusicPlayer> musicPlayer = sender.representedObject;
    NSAssert(musicPlayer, @"BGMAutoPauseMusicPrefs::handleMusicPlayerChange: !musicPlayer");
    
    musicPlayers.selectedMusicPlayer = musicPlayer;
    
    // Select/deselect the menu items
    for (NSMenuItem* item in musicPlayerMenuItems) {
        BOOL isNewlySelectedMusicPlayer = (item == sender);
        item.state = (isNewlySelectedMusicPlayer ? NSOnState : NSOffState);
    }
}

@end

#pragma clang assume_nonnull end


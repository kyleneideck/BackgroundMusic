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
//  BGMAutoPauseMenuItem.m
//  BGMApp
//
//  Copyright © 2016, 2019 Kyle Neideck
//  Copyright © 2016 Tanner Hoke
//

// Self Include
#import "BGMAutoPauseMenuItem.h"

// Local Includes
#import "BGMAppWatcher.h"


#pragma clang assume_nonnull begin

static NSString* const kMenuItemTitleFormat = @"Auto-pause %@";
static NSString* const kMenuItemDisabledToolTipFormat = @"%@ doesn't appear to be running.";

// Wait time to disable/enable the auto-pause menu item, in seconds.
static SInt64 const kMenuItemUpdateWaitTime = 1;

@implementation BGMAutoPauseMenuItem {
    BGMUserDefaults* userDefaults;
    NSMenuItem* menuItem;
    BGMAutoPauseMusic* autoPauseMusic;
    BGMMusicPlayers* musicPlayers;
    BGMAppWatcher* appWatcher;
}

- (instancetype) initWithMenuItem:(NSMenuItem*)item
                   autoPauseMusic:(BGMAutoPauseMusic*)autoPause
                     musicPlayers:(BGMMusicPlayers*)players
                     userDefaults:(BGMUserDefaults*)defaults {
    if ((self = [super init])) {
        menuItem = item;
        autoPauseMusic = autoPause;
        musicPlayers = players;
        userDefaults = defaults;
        
        // Enable/disable auto-pause to match the user's preferences setting.
        if (userDefaults.autoPauseMusicEnabled) {
            menuItem.state = NSOnState;
            [autoPauseMusic enable];
        } else {
            menuItem.state = NSOffState;
            [autoPauseMusic disable];
        }
        
        // Toggle auto-pause when the menu item is clicked.
        menuItem.target = self;
        menuItem.action = @selector(toggleAutoPauseMusic);

        [self initMenuItemTitle];
    }
    
    return self;
}

- (void) initMenuItemTitle {
    // Set the initial text, tool-tip, state, etc.
    [self updateMenuItemTitle];

    // Avoid retain cycles in case we ever want to destroy instances of this class.
    BGMAutoPauseMenuItem* __weak weakSelf = self;

    // Add a callback that enables/disables the Auto-pause Music menu item when the music player
    // is launched/terminated.
    void (^callback)(void) = ^{
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, kMenuItemUpdateWaitTime * NSEC_PER_SEC),
                       dispatch_get_main_queue(),
                       ^{
                           BGMAutoPauseMenuItem* strongSelf = weakSelf;
                           [strongSelf updateMenuItemTitle];
                       });
    };

    appWatcher = [[BGMAppWatcher alloc] initWithAppLaunched:callback
                                              appTerminated:callback
                                         isMatchingBundleID:^BOOL(NSString* appBundleID) {
        BGMAutoPauseMenuItem* strongSelf = weakSelf;
        NSString* __nullable playerBundleID =
                strongSelf->musicPlayers.selectedMusicPlayer.bundleID;
        return playerBundleID && [appBundleID isEqualToString:(NSString*)playerBundleID];
    }];
}

- (void) toggleAutoPauseMusic {
    // The menu item was clicked.
    
    if (menuItem.state == NSOnState) {
        menuItem.state = NSOffState;
        [autoPauseMusic disable];
    } else {
        menuItem.state = NSOnState;
        [autoPauseMusic enable];
    }
    
    // Persist the change in the user's preferences.
    userDefaults.autoPauseMusicEnabled = (menuItem.state == NSOnState);
}

- (void) updateMenuItemTitle {
    [self updateMenuItemTitleWithHighlight:menuItem.isHighlighted];
}

- (void) updateMenuItemTitleWithHighlight:(BOOL)highlight {
    // Set the title of the Auto-pause Music menu item, including the name of the selected music player.
    NSString* musicPlayerName = musicPlayers.selectedMusicPlayer.name;
    menuItem.title = [NSString stringWithFormat:kMenuItemTitleFormat, musicPlayerName];
    
    // Make the Auto-pause Music menu item appear disabled if the application is not running.
    //
    // We don't actually disable it just in case the user decides to disable auto-pause and their music player isn't
    // running. E.g. someone who only recently installed Background Music and doesn't want to use auto-pause at all.
    if (musicPlayers.selectedMusicPlayer.running) {
        menuItem.attributedTitle = nil;
        menuItem.toolTip = nil;
    } else {
        // Hardcode the text colour grey to match disabled menu items (unless the menu item is highlighted, in which
        // case use white).
        //
        // I couldn't figure out a way to do this without hardcoding the colours. There's no colour constant for this,
        // except possibly disabledControlTextColor, which just leaves the text black for me. I also couldn't get the
        // colours from the built-in NSColorLists.
        //
        // TODO: Can we make the tick mark grey as well?
        NSString* __nullable appleInterfaceStyle =
            [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
        BOOL darkMode = [appleInterfaceStyle isEqualToString:@"Dark"];
        NSColor* textColor = [NSColor colorWithHue:0
                                        saturation:0
                                        brightness:(highlight ? 1 : (darkMode ? 0.25 : 0.75))
                                             alpha:1];
        
        NSDictionary* attributes = @{ NSFontAttributeName: [NSFont menuBarFontOfSize:0],  // Default font size
                                      NSForegroundColorAttributeName: textColor };
        NSAttributedString* pseudoDisabledTitle = [[NSAttributedString alloc] initWithString:menuItem.title
                                                                                  attributes:attributes];
        menuItem.attributedTitle = pseudoDisabledTitle;

        menuItem.toolTip = [NSString stringWithFormat:kMenuItemDisabledToolTipFormat, musicPlayerName];
    }
}

#pragma mark Parent menu events

- (void) parentMenuNeedsUpdate {
    [self updateMenuItemTitle];
}

- (void) parentMenuItemWillHighlight:(NSMenuItem* __nullable)item {
    // Used to make the auto-pause menu item's text white when it's highlighted and change it back after.
    //
    // TODO: If you click the auto-pause menu item while it's disabled, it will initially appear highlighted next time
    //       you open the main menu.
    
    // If item is nil or any other menu item, the auto-pause menu item will be unhighlighted.
    BOOL willHighlightMenuItem = [item isEqual:menuItem];
    
    // Only update the menu item if it's changing (from highlighted to unhighlighted or vice versa) to save a little
    // CPU.
    if (willHighlightMenuItem != menuItem.highlighted) {
        [self updateMenuItemTitleWithHighlight:willHighlightMenuItem];
    }
}

@end

#pragma clang assume_nonnull end


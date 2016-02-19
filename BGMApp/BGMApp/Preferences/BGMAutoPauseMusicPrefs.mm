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
//  Copyright © 2016 Kyle Neideck
//

// Self Includes
#import "BGMAutoPauseMusicPrefs.h"

// Local Includes
#include "BGM_Types.h"
#import "BGMMusicPlayer.h"


static NSString* const kToggleAutoPauseMusicMenuItemTitleFormat = @"Auto-pause %@";
static float const kMenuItemIconScalingFactor = 1.15f;
static NSInteger const kPrefsMenuAutoPauseHeaderTag = 1;

@implementation BGMAutoPauseMusicPrefs {
    BGMAudioDeviceManager* audioDevices;
    NSMenuItem* toggleAutoPauseMusicMenuItem;
    NSMenu* prefsMenu;
    NSArray<NSMenuItem*>* musicPlayerMenuItems;
}

- (id) initWithPreferencesMenu:(NSMenu*)inPrefsMenu
  toggleAutoPauseMusicMenuItem:(NSMenuItem*)inToggleAutoPauseMusicMenuItem
                  audioDevices:(BGMAudioDeviceManager*)inAudioDevices {
    if ((self = [super init])) {
        prefsMenu = inPrefsMenu;
        toggleAutoPauseMusicMenuItem = inToggleAutoPauseMusicMenuItem;
        audioDevices = inAudioDevices;
        musicPlayerMenuItems = @[];
        
        [self initSelectedMusicPlayer];
        [self initMenuSection];
        [self updateMenuItemTitle];
    }
    
    return self;
}

- (void) initSelectedMusicPlayer {
    // TODO: It would make more sense to either just save the music player setting in the User Defaults (the same way AppDelegate saves
    //       whether auto-pause is enabled) or to send a "musicPlayerID" to the driver, which would only be used by BGMApp. If the latter,
    //       we might as well save the auto-pause setting on the driver as well just so all the settings are saved in the same place.
    
    // Get the currently selected music player from the driver and update the global in BGMMusicPlayerBase
    
    // The bundle ID and PID set on the driver
    CFNumberRef selectedPID = static_cast<CFNumberRef>([audioDevices bgmDevice].GetPropertyData_CFType(kBGMMusicPlayerProcessIDAddress));
    CFStringRef selectedBundleID = [audioDevices bgmDevice].GetPropertyData_CFString(kBGMMusicPlayerBundleIDAddress);
    
    DebugMsg("BGMAutoPauseMusicPrefs::initSelectedMusicPlayer: Music player on BGMDriver: bundleID=%s PID=%s",
             selectedBundleID == NULL ? "null" : CFStringGetCStringPtr(selectedBundleID, kCFStringEncodingUTF8),
             selectedPID == NULL ? "null" : [[(__bridge NSNumber*)selectedPID stringValue] UTF8String]);
    
    // If no music player is set on the driver, set it to the one set in the app and return
    if ((selectedBundleID == NULL || CFEqual(selectedBundleID, CFSTR(""))) &&
        (selectedPID == NULL || [(__bridge NSNumber*)selectedPID intValue] < 1)) {
        [self updateBGMDevice];
        return;
    }
    
    // The IDs set in the app, which will be updated if they don't match the values from the driver
    CFNumberRef selectedPIDInBGMApp = [[BGMMusicPlayerBase selectedMusicPlayer] pid];
    CFStringRef selectedBundleIDInBGMApp = [[[BGMMusicPlayerBase selectedMusicPlayer] class] bundleID];
    
    // Return early if the music player selected in the app already matches the driver
    if ((selectedPID != NULL && selectedPIDInBGMApp != NULL && CFEqual(selectedPID, selectedPIDInBGMApp)) ||
        (selectedBundleID != NULL && selectedBundleIDInBGMApp != NULL && CFEqual(selectedBundleID, selectedBundleIDInBGMApp))) {
        return;
    }
    
    // Check each selectable music player
    for (Class mpClass in [BGMMusicPlayerBase musicPlayerClasses]) {
        // Look for a running instance of the music player by PID
        if (selectedPID != NULL &&
            [mpClass respondsToSelector:@selector(pidsOfRunningInstances)] &&
            [mpClass respondsToSelector:@selector(initWithPIDFromNSNumber:)]) {
            NSArray<NSNumber*>* mpPIDs = [mpClass pidsOfRunningInstances];
            for (NSNumber* mpPID in mpPIDs) {
                if (CFEqual((__bridge CFNumberRef)mpPID, selectedPID)) {
                    DebugMsg("BGMAutoPauseMusicPrefs::initSelectedMusicPlayer: Selected music player on driver was %s (found by pid)",
                             [[mpClass name] UTF8String]);
                    [BGMMusicPlayerBase setSelectedMusicPlayer:[[mpClass alloc] initWithPIDFromNSNumber:mpPID]];
                    return;
                }
            }
        }
        
        // Check by bundle ID
        CFStringRef mpBundleID = [mpClass bundleID];
        if (selectedBundleID != NULL &&
            mpBundleID != NULL &&
            CFEqual(mpBundleID, selectedBundleID)) {
            // Found the selected music player. Update the app to match the driver and return.
            DebugMsg("BGMAutoPauseMusicPrefs::initSelectedMusicPlayer: Selected music player on driver was %s",
                     [[mpClass name] UTF8String]);
            
            [BGMMusicPlayerBase setSelectedMusicPlayer:[mpClass new]];
            return;
        }
    }
}

- (void) initMenuSection {
    // Add the menu items related to auto-pausing music to the settings submenu
    
    // The index to start inserting music player menu items at
    NSInteger musicPlayerItemsIndex = [prefsMenu indexOfItemWithTag:kPrefsMenuAutoPauseHeaderTag] + 1;
    
    // Insert the options to change the music player app
    for (Class musicPlayerClass in [BGMMusicPlayerBase musicPlayerClasses]) {
        NSMenuItem* menuItem = [prefsMenu insertItemWithTitle:[musicPlayerClass name]
                                                       action:@selector(handleMusicPlayerChange:)
                                                keyEquivalent:@""
                                                       atIndex:musicPlayerItemsIndex];
        
        musicPlayerMenuItems = [musicPlayerMenuItems arrayByAddingObject:menuItem];
        
        // Create an instance for this music player and associate it with the menu item
        [menuItem setRepresentedObject:[musicPlayerClass new]];
        
        // Show the default music player as selected
        if (musicPlayerClass == [[BGMMusicPlayerBase selectedMusicPlayer] class]) {
            [menuItem setState:NSOnState];
        }
        
        // Set the item's icon
        NSImage* icon = [musicPlayerClass icon];
        if (icon == nil) {
            // Set a blank icon so the text lines up
            icon = [NSImage new];
        }
        // Size the icon relative to the size of the item's text
        CGFloat length = [[NSFont menuBarFontOfSize:0] pointSize] * kMenuItemIconScalingFactor;
        [icon setSize:NSMakeSize(length, length)];
        [menuItem setImage:icon];
        
        [menuItem setTarget:self];
        [menuItem setIndentationLevel:1];
    }
}

- (void) handleMusicPlayerChange:(NSMenuItem*)sender {
    // Set the new music player as the selected music player
    BGMMusicPlayer* musicPlayer = [sender representedObject];
    assert(musicPlayer != nil);
    [BGMMusicPlayerBase setSelectedMusicPlayer:musicPlayer];
    
    // Select/Deselect the menu items
    for (NSMenuItem* item in musicPlayerMenuItems) {
        BOOL isNewlySelectedMusicPlayer = item == sender;
        [item setState:(isNewlySelectedMusicPlayer ? NSOnState : NSOffState)];
    }
    
    [self updateMenuItemTitle];
    [self updateBGMDevice];
}

- (void) updateBGMDevice {
    // Send the music player's PID or bundle ID to the driver
    
    DebugMsg("BGMAutoPauseMusicPrefs::updateBGMDevice: Setting the music player to %s on the driver",
             [[[[BGMMusicPlayer selectedMusicPlayer] class] name] UTF8String]);
    
    CFNumberRef __nullable pid = [[BGMMusicPlayer selectedMusicPlayer] pid];
    
    if (pid != NULL) {
        [audioDevices bgmDevice].SetPropertyData_CFType(kBGMMusicPlayerProcessIDAddress, pid);
    } else {
        CFStringRef __nullable bundleID = [[[BGMMusicPlayer selectedMusicPlayer] class] bundleID];
        
        if (bundleID != NULL) {
            [audioDevices bgmDevice].SetPropertyData_CFString(kBGMMusicPlayerBundleIDAddress, bundleID);
        }
    }
}

- (void) updateMenuItemTitle {
    // Set the title of the Auto-pause Music menu item, including the name of the selected music player
    NSString* musicPlayerName = [[[BGMMusicPlayer selectedMusicPlayer] class] name];
    NSString* title = [NSString stringWithFormat:kToggleAutoPauseMusicMenuItemTitleFormat, musicPlayerName];
    [toggleAutoPauseMusicMenuItem setTitle:title];
}

@end


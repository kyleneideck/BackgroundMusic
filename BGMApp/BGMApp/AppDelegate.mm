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
//  AppDelegate.mm
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//

// Self Includes
#import "AppDelegate.h"

// Local Includes
#include "BGM_Types.h"
#import "BGMAudioDeviceManager.h"
#import "BGMAutoPauseMusic.h"
#import "BGMAppVolumes.h"
#import "BGMPreferencesMenu.h"
#import "BGMXPCListener.h"


static float const kStatusBarIconPadding = 0.25;

@implementation AppDelegate {
    // The button in the system status bar (the bar with volume, battery, clock, etc.) to show the main menu
    // for the app. These are called "menu bar extras" in the Human Interface Guidelines.
    NSStatusItem* statusBarItem;
    
    BGMAutoPauseMusic* autoPauseMusic;
    BGMAppVolumes* appVolumes;
    BGMAudioDeviceManager* audioDevices;
    BGMPreferencesMenu* prefsMenu;
    BGMXPCListener* xpcListener;
}

- (void) awakeFromNib {
    // Set up the status bar item
    statusBarItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
    
    // Set the icon
    NSImage* icon = [NSImage imageNamed:@"FermataIcon"];
    if (icon != nil) {
        // Make the icon a "template image" so it gets drawn colour-inverted when it's highlighted or the status
        // bar's in dark mode
        [icon setTemplate:YES];
        
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_9
        if ([statusBarItem respondsToSelector:@selector(button)]) {
            CGFloat lengthMinusPadding = statusBarItem.button.frame.size.height * (1 - kStatusBarIconPadding);
            [icon setSize:NSMakeSize(lengthMinusPadding, lengthMinusPadding)];
            
            statusBarItem.button.image = icon;
        } else {
            // OS X 10.9 fallback.
            // TODO: It would be better to set this size dynamically, like we do for 10.10+.
            icon.size = NSMakeSize(16, 16);
            statusBarItem.image = icon;
        }
#else
        icon.size = NSMakeSize(16, 16);
        statusBarItem.image = icon;
#endif
    } else {
        // If our icon is missing for some reason, fallback to a fermata character (1D110)
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_9
        if ([statusBarItem respondsToSelector:@selector(button)]) {
            statusBarItem.button.title = @"𝄐";
        } else
#endif
        {
            statusBarItem.title = @"𝄐";
        }
    }
    
    // Set the main menu
    statusBarItem.menu = self.bgmMenu;
}

- (void) applicationDidFinishLaunching:(NSNotification*)aNotification {
    #pragma unused (aNotification)
    
    // Set up the GUI and other external interfaces.

    // Coordinates the audio devices (BGMDevice and the output device): manages playthrough, volume/mute controls, etc.
    NSError* err;
    audioDevices = [[BGMAudioDeviceManager alloc] initWithError:&err];
    if (audioDevices == nil) {
        [self showDeviceNotFoundErrorMessageAndExit:err.code];
    }
    [audioDevices setBGMDeviceAsOSDefault];
    
    autoPauseMusic = [[BGMAutoPauseMusic alloc] initWithAudioDevices:audioDevices];
    
    xpcListener = [[BGMXPCListener alloc] initWithAudioDevices:audioDevices
                                  helperConnectionErrorHandler:^(NSError* error) {
                                      [self showXPCHelperErrorMessageAndExit:error];
                                  }];
    
    appVolumes = [[BGMAppVolumes alloc] initWithMenu:[self bgmMenu]
                                       appVolumeView:[self appVolumeView]
                                        audioDevices:audioDevices];
    
    prefsMenu = [[BGMPreferencesMenu alloc] initWithbgmMenu:[self bgmMenu]
                                               audioDevices:audioDevices
                                                 aboutPanel:[self aboutPanel]
                                      aboutPanelLicenseView:[self aboutPanelLicenseView]];
    
    [self loadUserDefaults];
}

- (void) loadUserDefaults {
    // Register the preference defaults. These are the preferences/state that only apply to BGMApp. The others are
    // persisted on BGMDriver.
    NSDictionary* appDefaults = [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:YES]
                                                            forKey:@"AutoPauseMusicEnabled"];
    [[NSUserDefaults standardUserDefaults] registerDefaults:appDefaults];
    
    // Enable auto-pausing music if it's enabled in the user's preferences (which it is by default).
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"AutoPauseMusicEnabled"]) {
        [self toggleAutoPauseMusic:self];
    }
}

- (void) showDeviceNotFoundErrorMessageAndExit:(NSInteger)code {
    // Show an error dialog and exit if either BGMDevice wasn't found on the system or we couldn't find any output devices
    
    // NSAlert should only be used on the main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert* alert = [NSAlert new];
        
        if (code == kBGMErrorCode_BGMDeviceNotFound) {
            // TODO: Check whether the driver files are in /Library/Audio/Plug-Ins/HAL and offer to install them if not. Also,
            //       it would be nice if we could restart coreaudiod automatically (using launchd).
            [alert setMessageText:@"Could not find the Background Music virtual audio device."];
            [alert setInformativeText:@"Make sure you've installed Background Music.driver to /Library/Audio/Plug-Ins/HAL and restarted coreaudiod (e.g. \"sudo killall coreaudiod\")."];
        } else if (code == kBGMErrorCode_OutputDeviceNotFound) {
            [alert setMessageText:@"Could not find an audio output device."];
            [alert setInformativeText:@"If you do have one installed, this is probably a bug. Sorry about that. Feel free to file an issue on GitHub."];
        }
        
        [alert runModal];
        [NSApp terminate:self];
    });
}

- (void) showXPCHelperErrorMessageAndExit:(NSError*)error {
    // NSAlert should only be used on the main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert* alert = [NSAlert new];
        
        // TODO: Offer to install BGMXPCHelper if it's missing.
        [alert setMessageText:@"Error connecting to BGMXPCHelper."];
        [alert setInformativeText:[NSString stringWithFormat:@"%s%s%@ (%lu)",
                                   "Make sure you have BGMXPCHelper installed. There are instructions in the README.md file.",
                                   "\n\nDetails:\n",
                                   [error localizedDescription],
                                   [error code]]];
        
        [alert runModal];
        [NSApp terminate:self];
    });
}

- (void) applicationWillTerminate:(NSNotification*)aNotification {
    #pragma unused (aNotification)
    [audioDevices unsetBGMDeviceAsOSDefault];
}

- (IBAction) toggleAutoPauseMusic:(id)sender {
    #pragma unused (sender)
    
    if (self.autoPauseMenuItem.state == NSOnState) {
        self.autoPauseMenuItem.state = NSOffState;
        [autoPauseMusic disable];
    } else {
        self.autoPauseMenuItem.state = NSOnState;
        [autoPauseMusic enable];
    }
    
    // Persist the change in the user's preferences
    [[NSUserDefaults standardUserDefaults] setBool:(self.autoPauseMenuItem.state == NSOnState)
                                            forKey:@"AutoPauseMusicEnabled"];
}

@end


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

// System Includes
#import <math.h>

//included the Cocoa framework so I could create a menu item and open System Settings !
#import <Cocoa/Cocoa.h>
//added



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
    
    // Delay preferences
    NSMenuItem* pauseDelayMenuItem;
    NSMenuItem* maxUnpauseDelayMenuItem;
    NSSlider* pauseDelaySlider;
    NSSlider* maxUnpauseDelaySlider;
    NSTextField* pauseDelayLabel;
    NSTextField* maxUnpauseDelayLabel;
    BGMUserDefaults* userDefaults;
}

- (id) initWithBGMMenu:(NSMenu*)inBGMMenu
          audioDevices:(BGMAudioDeviceManager*)inAudioDevices
          musicPlayers:(BGMMusicPlayers*)inMusicPlayers
         statusBarItem:(BGMStatusBarItem*)inStatusBarItem
            aboutPanel:(NSPanel*)inAboutPanel
 aboutPanelLicenseView:(NSTextView*)inAboutPanelLicenseView
          userDefaults:(BGMUserDefaults*)inUserDefaults {
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
        
        //created a new menu item in the Preferences menu !
        NSMenuItem* launchAtLoginItem =
            [[NSMenuItem alloc] initWithTitle:@"Launch at Login Settings"
                                action:@selector(openLoginItemsSettings:)
                                keyEquivalent:@""];
        launchAtLoginItem.target = self;
        [prefsMenu addItem:launchAtLoginItem];
        //
        
        // Set up delay preferences
        userDefaults = inUserDefaults;
        [self setupDelayPreferences:prefsMenu];
    
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

- (void) setupDelayPreferences:(NSMenu*)prefsMenu {
    // Create delay preference menu items dynamically
    
    // Add separator
    [prefsMenu addItem:[NSMenuItem separatorItem]];
    
    // Add "Auto-pause Delays" header
    NSMenuItem* delaysHeader = [[NSMenuItem alloc] initWithTitle:@"Auto-pause Delays" action:nil keyEquivalent:@""];
    delaysHeader.enabled = NO;
    [prefsMenu addItem:delaysHeader];
    
    // Create pause delay menu item with slider
    pauseDelayMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    NSView* pauseDelayView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 280, 25)];
    
    // Pause delay label
    NSTextField* pauseDelayTitleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 5, 100, 15)];
    pauseDelayTitleLabel.stringValue = @"Pause Delay:";
    pauseDelayTitleLabel.editable = NO;
    pauseDelayTitleLabel.bordered = NO;
    pauseDelayTitleLabel.backgroundColor = [NSColor clearColor];
    pauseDelayTitleLabel.font = [NSFont menuFontOfSize:13];
    [pauseDelayView addSubview:pauseDelayTitleLabel];
    
    // Pause delay slider
    pauseDelaySlider = [[NSSlider alloc] initWithFrame:NSMakeRect(115, 5, 100, 15)];
    [pauseDelayView addSubview:pauseDelaySlider];
    
    // Pause delay value label
    pauseDelayLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(220, 5, 55, 15)];
    pauseDelayLabel.editable = NO;
    pauseDelayLabel.bordered = NO;
    pauseDelayLabel.backgroundColor = [NSColor clearColor];
    pauseDelayLabel.font = [NSFont menuFontOfSize:11];
    [pauseDelayView addSubview:pauseDelayLabel];
    
    pauseDelayMenuItem.view = pauseDelayView;
    [prefsMenu addItem:pauseDelayMenuItem];
    
    // Create max unpause delay menu item with slider
    maxUnpauseDelayMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    NSView* maxUnpauseDelayView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 280, 25)];
    
    // Max unpause delay label
    NSTextField* maxUnpauseDelayTitleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 5, 100, 15)];
    maxUnpauseDelayTitleLabel.stringValue = @"Max Unpause:";
    maxUnpauseDelayTitleLabel.editable = NO;
    maxUnpauseDelayTitleLabel.bordered = NO;
    maxUnpauseDelayTitleLabel.backgroundColor = [NSColor clearColor];
    maxUnpauseDelayTitleLabel.font = [NSFont menuFontOfSize:13];
    [maxUnpauseDelayView addSubview:maxUnpauseDelayTitleLabel];
    
    // Max unpause delay slider
    maxUnpauseDelaySlider = [[NSSlider alloc] initWithFrame:NSMakeRect(115, 5, 100, 15)];
    [maxUnpauseDelayView addSubview:maxUnpauseDelaySlider];
    
    // Max unpause delay value label
    maxUnpauseDelayLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(220, 5, 55, 15)];
    maxUnpauseDelayLabel.editable = NO;
    maxUnpauseDelayLabel.bordered = NO;
    maxUnpauseDelayLabel.backgroundColor = [NSColor clearColor];
    maxUnpauseDelayLabel.font = [NSFont menuFontOfSize:11];
    [maxUnpauseDelayView addSubview:maxUnpauseDelayLabel];
    
    maxUnpauseDelayMenuItem.view = maxUnpauseDelayView;
    [prefsMenu addItem:maxUnpauseDelayMenuItem];
    
    // Initialize the delay sliders with current values and targets
    [self initDelaySliders];
}

- (void) initDelaySliders {
    // Configure pause delay slider with logarithmic scale (0ms to 10000ms)
    // Slider range 0-100 maps logarithmically to 0-10000ms
    pauseDelaySlider.minValue = 0;
    pauseDelaySlider.maxValue = 100;
    pauseDelaySlider.integerValue = [self msToSliderValue:userDefaults.pauseDelayMS];
    pauseDelaySlider.target = self;
    pauseDelaySlider.action = @selector(pauseDelaySliderChanged:);
    
    // Configure max unpause delay slider with logarithmic scale (0ms to 10000ms)
    // Slider range 0-100 maps logarithmically to 0-10000ms
    maxUnpauseDelaySlider.minValue = 0;
    maxUnpauseDelaySlider.maxValue = 100;
    maxUnpauseDelaySlider.integerValue = [self msToSliderValue:userDefaults.maxUnpauseDelayMS];
    maxUnpauseDelaySlider.target = self;
    maxUnpauseDelaySlider.action = @selector(maxUnpauseDelaySliderChanged:);
    
    // Update labels with current values
    [self updatePauseDelayLabel];
    [self updateMaxUnpauseDelayLabel];
}

- (void) pauseDelaySliderChanged:(NSSlider*)sender {
    NSUInteger delayMS = [self sliderValueToMS:sender.integerValue];
    userDefaults.pauseDelayMS = delayMS;
    [self updatePauseDelayLabel];
}

- (void) maxUnpauseDelaySliderChanged:(NSSlider*)sender {
    NSUInteger delayMS = [self sliderValueToMS:sender.integerValue];
    userDefaults.maxUnpauseDelayMS = delayMS;
    [self updateMaxUnpauseDelayLabel];
}


- (void) updatePauseDelayLabel {
    NSUInteger delayMS = userDefaults.pauseDelayMS;
    if (delayMS == 0) {
        pauseDelayLabel.stringValue = @"Off";
    } else {
        pauseDelayLabel.stringValue = [NSString stringWithFormat:@"%lums", (unsigned long)delayMS];
    }
}

- (void) updateMaxUnpauseDelayLabel {
    NSUInteger delayMS = userDefaults.maxUnpauseDelayMS;
    if (delayMS == 0) {
        maxUnpauseDelayLabel.stringValue = @"Off";
    } else {
        maxUnpauseDelayLabel.stringValue = [NSString stringWithFormat:@"%lums", (unsigned long)delayMS];
    }
}

// Logarithmic conversion methods for better control at small values
// Maps slider value (0-100) to milliseconds (0-10000) logarithmically
- (NSUInteger) sliderValueToMS:(NSInteger)sliderValue {
    if (sliderValue <= 0) {
        return 0;
    }
    
    // Logarithmic mapping: slider 0-100 -> ms 0-10000
    // Formula: ms = (10^(sliderValue/50) - 1) * (10000/99)
    // This gives finer control at small values and coarser control at large values
    double normalizedSlider = (double)sliderValue / 100.0; // 0-1
    double logValue = pow(10.0, normalizedSlider * 2.0) - 1.0; // 0-99
    NSUInteger ms = (NSUInteger)(logValue * (10000.0 / 99.0));
    
    return MIN(ms, 10000); // Cap at 10000ms
}

// Maps milliseconds (0-10000) to slider value (0-100) logarithmically
- (NSInteger) msToSliderValue:(NSUInteger)ms {
    if (ms == 0) {
        return 0;
    }
    
    // Inverse of the logarithmic mapping
    // Formula: sliderValue = 50 * log10((ms * 99/10000) + 1)
    double normalizedMS = (double)ms / 10000.0; // 0-1
    double logInput = (normalizedMS * 99.0) + 1.0; // 1-100
    double sliderValue = (log10(logInput) / 2.0) * 100.0; // 0-100
    
    return (NSInteger)MIN(MAX(sliderValue, 0), 100);
}

//created a function, this runs when the menu item is clicked !
- (IBAction)openLoginItemsSettings:(id)sender {
    (void)sender;
    NSURL* url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.LoginItems-Settings.extension"];
    if (url) {
        [[NSWorkspace sharedWorkspace] openURL:url];
    }
}
//


@end

NS_ASSUME_NONNULL_END


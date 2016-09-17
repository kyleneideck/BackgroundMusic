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
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#import "BGMPreferencesMenu.h"

// Local Includes
#import "BGMAutoPauseMusicPrefs.h"
#import "BGMOutputDevicePrefs.h"


NS_ASSUME_NONNULL_BEGIN

// Interface Builder tags
static NSInteger const kPreferencesMenuItemTag = 1;
static NSInteger const kAboutPanelMenuItemTag = 3;

static NSInteger const kAboutPanelVersionLabelTag = 1;
static NSInteger const kAboutPanelCopyrightLabelTag = 2;

@implementation BGMPreferencesMenu {
    // Menu sections
    BGMAutoPauseMusicPrefs* autoPauseMusicPrefs;
    BGMOutputDevicePrefs* outputDevicePrefs;
    
    // About Background Music window
    NSPanel* aboutPanel;
    NSTextField* aboutPanelVersionLabel;
    NSTextField* aboutPanelCopyrightLabel;
    NSTextView* aboutPanelLicenseView;
}

- (id) initWithBGMMenu:(NSMenu*)inBGMMenu
          audioDevices:(BGMAudioDeviceManager*)inAudioDevices
          musicPlayers:(BGMMusicPlayers*)inMusicPlayers
            aboutPanel:(NSPanel*)inAboutPanel
 aboutPanelLicenseView:(NSTextView*)inAboutPanelLicenseView {
    if ((self = [super init])) {
        aboutPanel = inAboutPanel;
        
        aboutPanelVersionLabel = [[aboutPanel contentView] viewWithTag:kAboutPanelVersionLabelTag];
        aboutPanelCopyrightLabel = [[aboutPanel contentView] viewWithTag:kAboutPanelCopyrightLabelTag];
        aboutPanelLicenseView = inAboutPanelLicenseView;

        NSMenu* prefsMenu = [[inBGMMenu itemWithTag:kPreferencesMenuItemTag] submenu];
        [prefsMenu setDelegate:self];
        
        autoPauseMusicPrefs = [[BGMAutoPauseMusicPrefs alloc] initWithPreferencesMenu:prefsMenu
                                                                         audioDevices:inAudioDevices
                                                                         musicPlayers:inMusicPlayers];
        
        outputDevicePrefs = [[BGMOutputDevicePrefs alloc] initWithAudioDevices:inAudioDevices];
        
        // Set up the "About Background Music" menu item
        NSMenuItem* aboutMenuItem = [prefsMenu itemWithTag:kAboutPanelMenuItemTag];
        [aboutMenuItem setTarget:self];
        [aboutMenuItem setAction:@selector(showAboutPanel)];
        
        [self initAboutPanel];
    }
    
    return self;
}

- (void) initAboutPanel {
    // Set up the About Background Music window
    
    NSBundle* bundle = [NSBundle mainBundle];
    
    if (bundle == nil) {
        LogWarning("Background Music: BGMPreferencesMenu::initAboutPanel: Could not find main bundle");
    } else {
        // Version number label
        NSString* version = [[bundle infoDictionary] objectForKey:@"CFBundleShortVersionString"];
        [aboutPanelVersionLabel setStringValue:[NSString stringWithFormat:@"Version %@", version]];
        
        // Copyright notice label
        NSString* copyrightNotice = [[bundle infoDictionary] objectForKey:@"NSHumanReadableCopyright"];
        [aboutPanelCopyrightLabel setStringValue:copyrightNotice];
        
        // Load the text of the license into the text view
        NSString* licensePath = [bundle pathForResource:@"LICENSE" ofType:nil];
        NSError* err;
        NSString* licenseStr = [NSString stringWithContentsOfFile:licensePath encoding:NSASCIIStringEncoding error:&err];
        
        if (err != nil || [licenseStr isEqualToString:@""]) {
            NSLog(@"Error loading license file: %@", err);
            licenseStr = @"Error: could not open license file.";
        }
        
        [aboutPanelLicenseView setString:licenseStr];
    }
}

- (void) showAboutPanel {
    DebugMsg("BGMPreferencesMenu::showAboutPanel: Opening \"About Background Music\" panel");
    [NSApp activateIgnoringOtherApps:YES];
    [aboutPanel setIsVisible:YES];
    [aboutPanel makeKeyAndOrderFront:self];
}

#pragma mark NSMenuDelegate

- (void) menuNeedsUpdate:(NSMenu*)menu {
    [outputDevicePrefs populatePreferencesMenu:menu];
}

@end

NS_ASSUME_NONNULL_END


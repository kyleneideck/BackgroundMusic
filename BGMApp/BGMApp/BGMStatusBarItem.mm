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
//  BGMStatusBarItem.m
//  BGMApp
//
//  Copyright © 2019, 2020 Kyle Neideck
//

// Self Include
#import "BGMStatusBarItem.h"

// Local Includes
#import "BGM_Utils.h"
#import "BGMUserDefaults.h"
#import "BGMVolumeChangeListener.h"


#pragma clang assume_nonnull begin

static CGFloat const kStatusBarIconPadding                = 0.25;
static CGFloat const kVolumeIconAdditionalVerticalPadding = 0.075;

@implementation BGMStatusBarItem
{
    BGMAudioDeviceManager* audioDevices;

    // User settings and data.
    BGMUserDefaults* userDefaults;

    NSImage* fermataIcon;
    NSImage* volumeIcon0SoundWaves;
    NSImage* volumeIcon1SoundWave;
    NSImage* volumeIcon2SoundWaves;
    NSImage* volumeIcon3SoundWaves;

    NSStatusItem* statusBarItem;
    BGMDebugLoggingMenuItem* debugLoggingMenuItem;

    BGMVolumeChangeListener* volumeChangeListener;
    id __nullable clickEventHandler;

    BGMStatusBarIcon _icon;
}

#pragma mark Initialisation

- (instancetype) initWithMenu:(NSMenu*)bgmMenu
                 audioDevices:(BGMAudioDeviceManager*)devices
                 userDefaults:(BGMUserDefaults*)defaults {
    if ((self = [super init])) {
        statusBarItem =
                [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];

        audioDevices = devices;
        userDefaults = defaults;

        // Initialise the icons.
        [self initIcons];

        // Set the initial icon.
        self.icon = userDefaults.statusBarIcon;

        // Set the menu item to open the main menu.
        statusBarItem.menu = bgmMenu;

        // Monitor click events so we can show extra options in the menu if the user was holding the
        // option key.
        clickEventHandler = [self addClickMonitor];

        // Set the accessibility label to "Background Music". (We intentionally don't set a title or
        // a tooltip.)
        if ([BGMStatusBarItem buttonAvailable]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
            statusBarItem.button.accessibilityLabel =
                    [NSRunningApplication currentApplication].localizedName;
#pragma clang diagnostic pop
        }

        // Update the icon when BGMDevice's volume changes.
        BGMStatusBarItem* __weak weakSelf = self;
        volumeChangeListener = new BGMVolumeChangeListener(audioDevices.bgmDevice, [=] {
            [weakSelf bgmDeviceVolumeDidChange];
        });
    }

    return self;
}

- (id __nullable) addClickMonitor {
    NSEvent* __nullable (^handlerBlock)(NSEvent*) =
        ^NSEvent* __nullable (NSEvent* event) {
            [self statusBarItemWasClicked:event];
            return event;
        };

    // TODO: I doubt this works well with VoiceOver.
    return [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown
                                                 handler:handlerBlock];
}

- (void) dealloc {
    delete volumeChangeListener;

    if (clickEventHandler) {
        [NSEvent removeMonitor:(id)clickEventHandler];
        clickEventHandler = nil;
    }
}

- (void) initIcons {
    // Load the icons.
    fermataIcon = [NSImage imageNamed:@"FermataIcon"];
    volumeIcon0SoundWaves = [NSImage imageNamed:@"Volume0"];
    volumeIcon1SoundWave = [NSImage imageNamed:@"Volume1"];
    volumeIcon2SoundWaves = [NSImage imageNamed:@"Volume2"];
    volumeIcon3SoundWaves = [NSImage imageNamed:@"Volume3"];

    // Set the icons' sizes.
    NSRect statusBarItemFrame;

    if ([BGMStatusBarItem buttonAvailable]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
        statusBarItemFrame = statusBarItem.button.frame;
#pragma clang diagnostic pop
    } else {
        // OS X 10.9 fallback. I haven't tested this (or anything else on 10.9).
        statusBarItemFrame = statusBarItem.view.frame;
    }

    CGFloat heightMinusPadding = statusBarItemFrame.size.height * (1 - kStatusBarIconPadding);

    // The fermata icon has equal width and height.
    [fermataIcon setSize:NSMakeSize(heightMinusPadding, heightMinusPadding)];

    // The volume icons are all the same width and height.
    CGFloat volumeIconWidthToHeightRatio =
            volumeIcon0SoundWaves.size.width / volumeIcon0SoundWaves.size.height;
    CGFloat volumeIconWidth = heightMinusPadding * volumeIconWidthToHeightRatio;
    CGFloat volumeIconHeight = heightMinusPadding * (1 - kVolumeIconAdditionalVerticalPadding);

    [volumeIcon0SoundWaves setSize:NSMakeSize(volumeIconWidth, volumeIconHeight)];
    [volumeIcon1SoundWave setSize:NSMakeSize(volumeIconWidth, volumeIconHeight)];
    [volumeIcon2SoundWaves setSize:NSMakeSize(volumeIconWidth, volumeIconHeight)];
    [volumeIcon3SoundWaves setSize:NSMakeSize(volumeIconWidth, volumeIconHeight)];

    // Make the icons "template images" so they get drawn colour-inverted when they're highlighted
    // or the system is in dark mode.
    [fermataIcon setTemplate:YES];
    [volumeIcon0SoundWaves setTemplate:YES];
    [volumeIcon1SoundWave setTemplate:YES];
    [volumeIcon2SoundWaves setTemplate:YES];
    [volumeIcon3SoundWaves setTemplate:YES];
}

#pragma mark Accessors

+ (BOOL) buttonAvailable {
    // NSStatusItem doesn't have the "button" property on OS X 10.9.
    return (floor(NSAppKitVersionNumber) >= NSAppKitVersionNumber10_10);
}

- (void) setImage:(NSImage*)image {
    if ([BGMStatusBarItem buttonAvailable]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
        statusBarItem.button.image = image;
#pragma clang diagnostic pop
    } else {
        statusBarItem.image = image;
    }
}

- (BGMStatusBarIcon) icon {
    return _icon;
}

- (void) setIcon:(BGMStatusBarIcon)icon {
    _icon = icon;

    // Save the setting.
    userDefaults.statusBarIcon = self.icon;

    // Change the icon (i.e. the image). Dispatch this to the main thread because it changes the UI.
    dispatch_async(dispatch_get_main_queue(), ^{
        if (_icon == BGMFermataStatusBarIcon) {
            [self setImage:fermataIcon];

            // If the icon was greyed out, change it back.
            if ([BGMStatusBarItem buttonAvailable]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
                statusBarItem.button.appearsDisabled = NO;
#pragma clang diagnostic pop
            }
        } else {
            BGMAssert((_icon == BGMVolumeStatusBarIcon), "Unknown icon in enum");

            [self updateVolumeStatusBarIcon];
        }
    });
}

#pragma mark Volume Icon

- (void) bgmDeviceVolumeDidChange {
    if (self.icon == BGMVolumeStatusBarIcon) {
        [self updateVolumeStatusBarIcon];
    }
}

// Should only be called on the main thread because it calls UI functions.
- (void) updateVolumeStatusBarIcon {
    BGMAssert([[NSThread currentThread] isMainThread],
              "updateVolumeStatusBarIcon called on non-main thread.");
    BGMAssert((self.icon == BGMVolumeStatusBarIcon), "Volume status bar icon not enabled");

    BGMAudioDevice bgmDevice = [audioDevices bgmDevice];

    // BGMDevice should never return an error for these calls, so we just swallow any exceptions and
    // give up.
    BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeOutput;
        AudioObjectPropertyScope element = kAudioObjectPropertyElementMaster;

        BOOL hasVolume = bgmDevice.HasVolumeControl(scope, element);

        // Show the button as greyed out if BGMDevice doesn't have a volume control (which means the
        // output device doesn't have one).
        if ([BGMStatusBarItem buttonAvailable]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
            statusBarItem.button.appearsDisabled = !hasVolume;
#pragma clang diagnostic pop
        }

        if (hasVolume) {
            if (bgmDevice.HasMuteControl(scope, element) &&
                    bgmDevice.GetMuteControlValue(scope, element)) {
                // The device is muted, so use the zero waves icon.
                [self setImage:volumeIcon0SoundWaves];
            } else {
                // Set the icon to reflect the device's volume.
                double volume = bgmDevice.GetVolumeControlScalarValue(scope, element);

                // These values match the macOS volume status bar item, except for the first one. I
                // don't know why, but at a very low volume macOS will show the zero waves icon even
                // though the sound is still audible.
                if (volume == 0.05) {
                    [self setImage:volumeIcon0SoundWaves];
                } else if (volume < 0.33) {
                    [self setImage:volumeIcon1SoundWave];
                } else if (volume < 0.66) {
                    [self setImage:volumeIcon2SoundWaves];
                } else {
                    [self setImage:volumeIcon3SoundWaves];
                }
            }
        } else {
            // Always use the full-volume icon when the device has no volume control.
            [self setImage:volumeIcon3SoundWaves];
        }
    });

    DebugMsg("BGMStatusBarItem::updateVolumeStatusBarIcon: Set icon to %s",
             statusBarItem.image.name.UTF8String);
}

#pragma mark Debug Logging Menu Item

- (void) statusBarItemWasClicked:(NSEvent* __nonnull)event {
    if ((event.modifierFlags & NSEventModifierFlagOption) != 0) {
        DebugMsg("BGMStatusBarItem::statusBarItemWasClicked: Option key held");
        [debugLoggingMenuItem setMenuShowingExtraOptions:YES];
    } else {
        [debugLoggingMenuItem setMenuShowingExtraOptions:NO];
    }
}

- (void) setDebugLoggingMenuItem:(BGMDebugLoggingMenuItem*)menuItem {
    debugLoggingMenuItem = menuItem;
}

@end

#pragma clang assume_nonnull end


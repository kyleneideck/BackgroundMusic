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
//  BGMSleepTimer.mm
//  BGMApp
//
//  Copyright © 2026 Background Music contributors
//

// Self Include
#import "BGMSleepTimer.h"

// Local Includes
#include "BGM_Types.h"
#import "BGM_Utils.h"
#import "BGMMusicPlayer.h"

// PublicUtility Includes
#import "CAException.h"

// STL Includes
#import <cmath>  // std::ceil


#pragma clang assume_nonnull begin

// The scope and channel used to mute BGMDevice's output, matching BGMOutputVolumeMenuItem.
static const AudioObjectPropertyScope   kOutputScope  = kAudioDevicePropertyScopeOutput;
static const AudioObjectPropertyElement kOutputChannel = kMainChannel;

static NSString* const kParentTitle             = @"Sleep Timer";
static NSString* const kParentTitleActiveFormat = @"Sleep Timer (%ld min left)";
static NSString* const kMuteMenuItemTitle       = @"Mute Output When Timer Ends";

// The duration options shown in the submenu, in minutes. Zero means "Off" (no timer).
static NSInteger const kDurationsMinutes[] = { 0, 15, 30, 45, 60, 90, 120 };

@implementation BGMSleepTimer {
    BGMAudioDeviceManager* audioDevices;
    BGMMusicPlayers* musicPlayers;
    BGMUserDefaults* userDefaults;

    // The menu items for the duration options, in the same order as kDurationsMinutes.
    NSArray<NSMenuItem*>* durationMenuItems;
    // The item that toggles whether the output is muted when the timer ends.
    NSMenuItem* muteMenuItem;

    // Fires when the timer ends. Nil while no timer is active.
    NSTimer* __nullable expiryTimer;
    // The time the active timer will end. Nil while no timer is active.
    NSDate* __nullable expiryDate;
    // The duration of the active timer, in minutes. Zero while no timer is active.
    NSInteger activeDurationMinutes;
}

@synthesize menuItem = menuItem;

- (instancetype) initWithAudioDevices:(BGMAudioDeviceManager*)inAudioDevices
                         musicPlayers:(BGMMusicPlayers*)inMusicPlayers
                         userDefaults:(BGMUserDefaults*)inUserDefaults {
    if ((self = [super init])) {
        audioDevices = inAudioDevices;
        musicPlayers = inMusicPlayers;
        userDefaults = inUserDefaults;

        activeDurationMinutes = 0;

        [self buildMenuItem];
    }

    return self;
}

- (void) dealloc {
    [self cancelTimer];
}

- (void) buildMenuItem {
    menuItem = [[NSMenuItem alloc] initWithTitle:kParentTitle action:nil keyEquivalent:@""];

    NSMenu* submenu = [[NSMenu alloc] initWithTitle:kParentTitle];
    // We update the duration items and title ourselves, so disable auto-enabling.
    submenu.autoenablesItems = NO;

    // Add the duration options.
    NSMutableArray<NSMenuItem*>* items = [NSMutableArray new];

    for (NSUInteger i = 0; i < (sizeof(kDurationsMinutes) / sizeof(kDurationsMinutes[0])); i++) {
        NSInteger minutes = kDurationsMinutes[i];
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[BGMSleepTimer titleForMinutes:minutes]
                                                      action:@selector(durationSelected:)
                                               keyEquivalent:@""];
        item.target = self;
        item.tag = minutes;
        // Start with "Off" selected.
        item.state = (minutes == 0) ? NSOnState : NSOffState;
        [submenu addItem:item];
        [items addObject:item];
    }

    durationMenuItems = items;

    [submenu addItem:[NSMenuItem separatorItem]];

    // Add the toggle for muting the output when the timer ends.
    muteMenuItem = [[NSMenuItem alloc] initWithTitle:kMuteMenuItemTitle
                                              action:@selector(toggleMuteWhenTimerEnds)
                                       keyEquivalent:@""];
    muteMenuItem.target = self;
    muteMenuItem.state = userDefaults.sleepTimerMutesOutput ? NSOnState : NSOffState;
    [submenu addItem:muteMenuItem];

    menuItem.submenu = submenu;
}

// Returns the menu item title for a duration in minutes. Zero is the "Off" option.
+ (NSString*) titleForMinutes:(NSInteger)minutes {
    switch (minutes) {
        case 0:   return @"Off";
        case 15:  return @"15 Minutes";
        case 30:  return @"30 Minutes";
        case 45:  return @"45 Minutes";
        case 60:  return @"1 Hour";
        case 90:  return @"1.5 Hours";
        case 120: return @"2 Hours";
        default:  return [NSString stringWithFormat:@"%ld Minutes", (long)minutes];
    }
}

#pragma mark Menu Actions

- (void) durationSelected:(NSMenuItem*)item {
    NSInteger minutes = item.tag;

    if (minutes <= 0) {
        // "Off" cancels the timer.
        [self cancelTimer];
    } else {
        [self startTimerWithMinutes:minutes];
    }

    [self updateDurationCheckmarks];
    [self updateParentTitle];
}

- (void) toggleMuteWhenTimerEnds {
    BOOL enabled = (muteMenuItem.state != NSOnState);
    muteMenuItem.state = enabled ? NSOnState : NSOffState;
    userDefaults.sleepTimerMutesOutput = enabled;
}

#pragma mark Timer

- (void) startTimerWithMinutes:(NSInteger)minutes {
    [self cancelTimer];

    activeDurationMinutes = minutes;

    NSTimeInterval seconds = static_cast<NSTimeInterval>(minutes) * 60.0;
    expiryDate = [NSDate dateWithTimeIntervalSinceNow:seconds];

    // NSTimer keeps the main run loop scheduled and fires on the main thread, so the expiry handler
    // can safely mutate the UI. This isn't realtime audio code, so a main-thread timer is fine.
    expiryTimer = [NSTimer scheduledTimerWithTimeInterval:seconds
                                                   target:self
                                                 selector:@selector(timerDidExpire)
                                                 userInfo:nil
                                                  repeats:NO];

    DebugMsg("BGMSleepTimer::startTimerWithMinutes: Started a %ld minute sleep timer.",
             (long)minutes);
}

- (void) cancelTimer {
    [expiryTimer invalidate];
    expiryTimer = nil;
    expiryDate = nil;
    activeDurationMinutes = 0;
}

- (void) timerDidExpire {
    DebugMsg("BGMSleepTimer::timerDidExpire: Sleep timer expired.");

    // Pause the selected music player. -pause returns whether it actually paused anything.
    BOOL didPause = [musicPlayers.selectedMusicPlayer pause];
    DebugMsg("BGMSleepTimer::timerDidExpire: Paused the music player: %s",
             didPause ? "YES" : "NO");

    // Optionally mute BGMDevice's output so any other audio goes silent too.
    if (userDefaults.sleepTimerMutesOutput) {
        [self muteOutput];
    }

    // Reset the menu back to the "Off" state.
    [self cancelTimer];
    [self updateDurationCheckmarks];
    [self updateParentTitle];
}

- (void) muteOutput {
    // Mute BGMDevice's main output the same way the output volume slider does when set to zero. The
    // volume slider's BGMVolumeChangeListener listens for this mute change, so its UI updates to
    // match automatically.
    BGMLogAndSwallowExceptions("BGMSleepTimer::muteOutput", ([&] {
        if (audioDevices.bgmDevice.HasMuteControl(kOutputScope, kOutputChannel)) {
            audioDevices.bgmDevice.SetMuteControlValue(kOutputScope, kOutputChannel, true);
            DebugMsg("BGMSleepTimer::muteOutput: Muted the output.");
        }
    }));
}

#pragma mark UI Updates

- (void) parentMenuNeedsUpdate {
    [self updateParentTitle];
}

- (void) updateParentTitle {
    menuItem.title = [self parentTitle];
}

- (NSString*) parentTitle {
    if (!expiryDate) {
        return kParentTitle;
    }

    NSTimeInterval remaining = expiryDate.timeIntervalSinceNow;

    if (remaining <= 0) {
        return kParentTitle;
    }

    // Round up so the last minute still reads "1 min left" rather than "0 min left".
    NSInteger minutesLeft = (NSInteger)std::ceil(remaining / 60.0);
    return [NSString stringWithFormat:kParentTitleActiveFormat, (long)minutesLeft];
}

- (void) updateDurationCheckmarks {
    // Check the item matching the active duration (or "Off" when no timer is active) and uncheck the
    // rest.
    for (NSMenuItem* item in durationMenuItems) {
        item.state = (item.tag == activeDurationMinutes) ? NSOnState : NSOffState;
    }
}

@end

#pragma clang assume_nonnull end

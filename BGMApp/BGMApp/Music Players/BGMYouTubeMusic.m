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
//  BGMYouTubeMusic.m
//  BGMApp
//
//  Copyright © 2026 Background Music contributors
//

// Self Include
#import "BGMYouTubeMusic.h"

// Auto-generated Scripting Bridge header
#import "Chromium.h"

// Local Includes
#import "BGMScriptingBridge.h"

// PublicUtility Includes
#import "CADebugMacros.h"


#pragma clang assume_nonnull begin

// The prefix a tab's URL must have for us to consider it a YouTube Music tab.
static NSString* const kYouTubeMusicURLPrefix = @"https://music.youtube.com";

// JavaScript we run in the YouTube Music tab. We control the page's <video> element directly, which
// is more robust than trying to click buttons in the UI. YouTube Music's UI keeps itself in sync
// with the <video> element, so pausing/playing it updates the rest of the page as well.

// Returns "playing", "paused" or "none" (no <video> element on the page).
static NSString* const kGetStateJavaScript =
    @"(function(){var v=document.querySelector('video');if(!v)return 'none';"
     "return (!v.paused&&!v.ended)?'playing':'paused';})()";

// Pauses the video if it's playing.
static NSString* const kPauseJavaScript =
    @"var v=document.querySelector('video'); if(v&&!v.paused){v.pause()}";

// Plays the video if it's paused.
static NSString* const kUnpauseJavaScript =
    @"var v=document.querySelector('video'); if(v&&v.paused){v.play()}";

@implementation BGMYouTubeMusic {
    BGMScriptingBridge* scriptingBridge;

    // The human-readable name of the browser, e.g. "Brave", for use in messages.
    NSString* browserName;

    // True once we've shown the dialog explaining that the user needs to enable
    // "Allow JavaScript from Apple Events". We only ever show it once per launch so we don't spam
    // the user.
    BOOL didShowAppleEventsJavaScriptDialog;
}

+ (NSArray<id<BGMMusicPlayer>>*) createInstancesWithDefaults:(BGMUserDefaults*)userDefaults {
    #pragma unused (userDefaults)

    // We return one instance per browser that's actually installed. Each browser has its own fixed
    // music player ID (generated with uuidgen). The IDs must be stable across launches and unique
    // among all music players, so if you copy this class, generate new ones.
    NSMutableArray<id<BGMMusicPlayer>>* instances = [NSMutableArray new];

    [self addInstanceForBrowserName:@"Brave"
                           bundleID:@"com.brave.Browser"
                      musicPlayerID:@"C8BDAEC5-B597-402C-AB34-42549D766D3A"
                                 to:instances];
    [self addInstanceForBrowserName:@"Chrome"
                           bundleID:@"com.google.Chrome"
                      musicPlayerID:@"BF177F55-15E0-425B-B236-791DFDD51592"
                                 to:instances];
    [self addInstanceForBrowserName:@"Edge"
                           bundleID:@"com.microsoft.edgemac"
                      musicPlayerID:@"004E86D9-8277-4513-91E3-CD5F8EA1D3DF"
                                 to:instances];

    return instances;
}

// Adds a BGMYouTubeMusic instance for the given browser to instances, but only if that browser is
// installed. The bundle ID both tells BGMDriver which process's audio to control and gives us the
// right icon in the UI.
+ (void) addInstanceForBrowserName:(NSString*)name
                          bundleID:(NSString*)bundleID
                     musicPlayerID:(NSString*)musicPlayerID
                                to:(NSMutableArray<id<BGMMusicPlayer>>*)instances {
    // Only offer browsers the user actually has installed. absolutePathForAppBundleWithIdentifier
    // is available on 10.13, unlike URLForApplicationWithBundleIdentifier (10.15+). It's deprecated
    // in newer SDKs, but we still support 10.13 so we keep using it here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSString* __nullable path =
        [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:bundleID];
#pragma clang diagnostic pop

    if (path) {
        [instances addObject:[[self alloc] initWithBrowserName:name
                                                      bundleID:bundleID
                                                 musicPlayerID:musicPlayerID]];
    } else {
        DebugMsg("BGMYouTubeMusic::addInstanceForBrowserName: %s not installed. Skipping.",
                 bundleID.UTF8String);
    }
}

- (instancetype) initWithBrowserName:(NSString*)name
                            bundleID:(NSString*)bundleID
                       musicPlayerID:(NSString*)musicPlayerID {
    NSString* displayName = [NSString stringWithFormat:@"YouTube Music (%@)", name];

    if ((self = [super initWithMusicPlayerID:[BGMMusicPlayerBase makeID:musicPlayerID]
                                        name:displayName
                                    bundleID:bundleID])) {
        scriptingBridge = [[BGMScriptingBridge alloc] initWithMusicPlayer:self];
        browserName = name;
        didShowAppleEventsJavaScriptDialog = NO;
    }

    return self;
}

- (ChromiumApplication* __nullable) browser {
    return (ChromiumApplication*)scriptingBridge.application;
}

- (void) wasSelected {
    [super wasSelected];
    [scriptingBridge ensurePermission];
}

// Returns the first tab (across all of the browser's windows) that has YouTube Music open, or nil
// if there isn't one (or the browser isn't running). Scripting Bridge calls are wrapped in
// @try/@catch because they can raise if the browser closes or misbehaves mid-iteration.
- (ChromiumTab* __nullable) youTubeMusicTab {
    ChromiumApplication* __nullable app = self.browser;

    if (!app) {
        return nil;
    }

    @try {
        for (ChromiumWindow* window in [app windows]) {
            for (ChromiumTab* tab in [window tabs]) {
                NSString* __nullable url = tab.URL;

                if (url && [url hasPrefix:kYouTubeMusicURLPrefix]) {
                    return tab;
                }
            }
        }
    } @catch (NSException* e) {
        DebugMsg("BGMYouTubeMusic::youTubeMusicTab: Caught exception looking for the tab: %s",
                 e.description.UTF8String);
    }

    return nil;
}

// Runs kGetStateJavaScript in the YouTube Music tab and returns "playing", "paused" or "none".
// Returns "none" if there's no YouTube Music tab or if we can't run JavaScript in it.
- (NSString*) playbackState {
    ChromiumTab* __nullable tab = self.youTubeMusicTab;

    if (!tab) {
        return @"none";
    }

    id __nullable result = nil;

    @try {
        result = [tab executeJavascript:kGetStateJavaScript];
    } @catch (NSException* e) {
        DebugMsg("BGMYouTubeMusic::playbackState: Caught exception running JavaScript: %s",
                 e.description.UTF8String);
        result = nil;
    }

    if ([result isKindOfClass:[NSString class]]) {
        return (NSString*)result;
    }

    // There's a YouTube Music tab but we couldn't run JavaScript in it. Almost always this is
    // because the user hasn't enabled "Allow JavaScript from Apple Events" in their browser. Explain
    // how to fix it (once) and behave as if nothing is playing.
    [self showAppleEventsJavaScriptDialogIfNeeded];

    return @"none";
}

// Shows a one-time dialog explaining how to enable "Allow JavaScript from Apple Events". Only shown
// when this is the selected music player, and at most once per launch.
- (void) showAppleEventsJavaScriptDialogIfNeeded {
    if (didShowAppleEventsJavaScriptDialog || !self.selected) {
        return;
    }

    // Set this before we (asynchronously) show the dialog so we don't queue up more than one.
    didShowAppleEventsJavaScriptDialog = YES;

    NSString* name = browserName;

    // NSAlert must run on the main thread, and runModal would block whichever thread calls it, so
    // dispatch it. (playbackState is often called from a background thread.)
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert* alert = [NSAlert new];
        alert.messageText =
            [NSString stringWithFormat:
             @"Background Music needs permission to control YouTube Music in %@.", name];
        alert.informativeText =
            [NSString stringWithFormat:
             @"In %@, open the View menu, then Developer, and turn on "
             "\"Allow JavaScript from Apple Events\". You only need to do this once.\n\n"
             "If you don't see the Developer menu, it will appear once the setting has been "
             "enabled from Settings, or you can control YouTube Music without it by selecting a "
             "different music player in Background Music.",
             name];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
    });
}

- (BOOL) isRunning {
    // We consider ourselves "running" only when the browser is open and actually has a YouTube
    // Music tab. That way BGMApp won't try to auto-pause a browser that isn't playing music.
    return self.browser != nil && self.youTubeMusicTab != nil;
}

- (BOOL) isPlaying {
    return [self.playbackState isEqualToString:@"playing"];
}

- (BOOL) isPaused {
    return [self.playbackState isEqualToString:@"paused"];
}

- (BOOL) pause {
    BOOL wasPlaying = self.playing;

    if (wasPlaying) {
        DebugMsg("BGMYouTubeMusic::pause: Pausing YouTube Music in %s", browserName.UTF8String);
        [self runJavaScript:kPauseJavaScript];
    }

    return wasPlaying;
}

- (BOOL) unpause {
    BOOL wasPaused = self.paused;

    if (wasPaused) {
        DebugMsg("BGMYouTubeMusic::unpause: Unpausing YouTube Music in %s", browserName.UTF8String);
        [self runJavaScript:kUnpauseJavaScript];
    }

    return wasPaused;
}

// Runs JavaScript in the YouTube Music tab, ignoring the result. Used for the pause/unpause
// commands, which don't return anything useful.
- (void) runJavaScript:(NSString*)javaScript {
    ChromiumTab* __nullable tab = self.youTubeMusicTab;

    if (!tab) {
        return;
    }

    @try {
        [tab executeJavascript:javaScript];
    } @catch (NSException* e) {
        DebugMsg("BGMYouTubeMusic::runJavaScript: Caught exception running JavaScript: %s",
                 e.description.UTF8String);
    }
}

@end

#pragma clang assume_nonnull end

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
//  BGMVLC.m
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//  Portions copyright (C) 2012 Peter Ljunglöf. All rights reserved.
//

// Self Include
#import "BGMVLC.h"

// Auto-generated Scripting Bridge header
#import "VLC.h"

// PublicUtility Includes
#undef CoreAudio_ThreadStampMessages
#define CoreAudio_ThreadStampMessages 0  // Requires C++
#include "CADebugMacros.h"


@implementation BGMVLC {
    VLCApplication* vlc;
}

BGM_MUSIC_PLAYER_DEFAULT_LOAD_METHOD

+ (NSString*) name {
    return @"VLC";
}

- (id) init {
    if ((self = [super init])) {
        vlc = [SBApplication applicationWithBundleIdentifier:(__bridge NSString*)[[self class] bundleID]];
    }
    
    return self;
}

- (CFNumberRef) pid {
    return NULL;
}

+ (CFStringRef) bundleID {
    return CFSTR("org.videolan.vlc");
}

- (BOOL) isRunning {
    return [vlc isRunning];
}

- (BOOL) pause {
    // isPlaying checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPlaying = [self isPlaying];
    
    if (wasPlaying) {
        DebugMsg("BGMVLC::pause: Pausing VLC");
        [self togglePlay];
    }
    
    return wasPlaying;
}

- (BOOL) unpause {
    // isPaused checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPaused = [self isPaused];
    
    if (wasPaused) {
        DebugMsg("BGMVLC::unpause: Unpausing VLC");
        [self togglePlay];
    }
    
    return wasPaused;
}

- (BOOL) isPlaying {
    return [self isRunning] && [vlc playing];
}

- (BOOL) isPaused {
    // VLC is paused if it has a file open but isn't playing it
    return [self isRunning] && [vlc nameOfCurrentItem] != nil && ![vlc playing];
}

// This is from SubTTS's STVLCPlayer class:
// https://github.com/heatherleaf/subtts-mac/blob/master/SubTTS/STVLCPlayer.m
//
// VLC's Scripting Bridge interface doesn't seem to have a cleaner way to do this.
- (void) togglePlay {
    NSString* src = @"tell application \"VLC\" to play";
    NSAppleScript* script = [[NSAppleScript alloc] initWithSource:src];
    [script executeAndReturnError:nil];
}

@end


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
#if __has_feature(objc_generics)
    // According to "man sdp", generics is required by headers generated for 10.11+ but not for headers generated for 10.10.
    #import "VLC.h"
#else
    #import "VLC_10.9.h"
#endif

// PublicUtility Includes
#undef CoreAudio_ThreadStampMessages
#define CoreAudio_ThreadStampMessages 0  // Requires C++
#include "CADebugMacros.h"


@implementation BGMVLC

BGM_MUSIC_PLAYER_DEFAULT_LOAD_METHOD

+ (NSString*) name {
    return @"VLC";
}

- (CFNumberRef) pid {
    return NULL;
}

+ (CFStringRef) bundleID {
    return CFSTR("org.videolan.vlc");
}

- (VLCApplication* __nullable) vlc {
    return (VLCApplication*) self.sbApplication;
}

- (BOOL) isRunning {
    return self.vlc && [self.vlc isRunning];
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
    return [self isRunning] && [self.vlc playing];
}

- (BOOL) isPaused {
    // VLC is paused if it has a file open but isn't playing it
    return [self isRunning] && [self.vlc nameOfCurrentItem] != nil && ![self.vlc playing];
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


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
//  Copyright © 2016-2018 Kyle Neideck
//  Portions copyright (C) 2012 Peter Ljunglöf. All rights reserved.
//

// Self Include
#import "BGMVLC.h"

// Auto-generated Scripting Bridge header
#import "VLC.h"

// Local Includes
#import "BGMScriptingBridge.h"

// PublicUtility Includes
#import "CADebugMacros.h"


#pragma clang assume_nonnull begin

@implementation BGMVLC {
    BGMScriptingBridge* scriptingBridge;
}

- (instancetype) init {
    if ((self = [super initWithMusicPlayerID:[BGMMusicPlayerBase makeID:@"5226F4B9-C740-4045-A273-4B8EABC0E8FC"]
                                        name:@"VLC"
                                    bundleID:@"org.videolan.vlc"])) {
        scriptingBridge = [[BGMScriptingBridge alloc] initWithMusicPlayer:self];
    }
    
    return self;
}

- (VLCApplication* __nullable) vlc {
    return (VLCApplication*)scriptingBridge.application;
}

- (void) onSelect {
    [super onSelect];
    [scriptingBridge ensurePermission];
}

- (BOOL) isRunning {
    return self.vlc.running;
}

// isPlaying and isPaused check self.running first just in case VLC is closed but self.vlc hasn't become
// nil yet. In that case, reading other properties of self.vlc could make Scripting Bridge open VLC.

- (BOOL) isPlaying {
    return self.running && self.vlc.playing;
}

- (BOOL) isPaused {
    // VLC is paused if it has a file open but isn't playing it
    return self.running && (self.vlc.nameOfCurrentItem != nil) && !self.vlc.playing;
}

- (BOOL) pause {
    // isPlaying checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPlaying = self.playing;
    
    if (wasPlaying) {
        DebugMsg("BGMVLC::pause: Pausing VLC");
        [BGMVLC togglePlay];
    }
    
    return wasPlaying;
}

- (BOOL) unpause {
    // isPaused checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPaused = self.paused;
    
    if (wasPaused) {
        DebugMsg("BGMVLC::unpause: Unpausing VLC");
        [BGMVLC togglePlay];
    }
    
    return wasPaused;
}

// This is from SubTTS's STVLCPlayer class:
// https://github.com/heatherleaf/subtts-mac/blob/master/SubTTS/STVLCPlayer.m
//
// VLC's Scripting Bridge interface doesn't seem to have a cleaner way to do this.
+ (void) togglePlay {
    NSString* src = @"tell application id \"org.videolan.vlc\" to play";
    NSAppleScript* script = [[NSAppleScript alloc] initWithSource:src];
    [script executeAndReturnError:nil];
}

@end

#pragma clang assume_nonnull end


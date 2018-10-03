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
//  BGMiTunes.m
//  BGMApp
//
//  Copyright © 2016-2018 Kyle Neideck
//

// Self Include
#import "BGMiTunes.h"

// Auto-generated Scripting Bridge header
#import "iTunes.h"

// Local Includes
#import "BGMScriptingBridge.h"

// PublicUtility Includes
#import "CADebugMacros.h"


#pragma clang assume_nonnull begin

@implementation BGMiTunes {
    BGMScriptingBridge* scriptingBridge;
}

+ (NSUUID*) sharedMusicPlayerID {
    NSUUID* __nullable musicPlayerID = [[NSUUID alloc] initWithUUIDString:@"7B62B5BF-CF90-4938-84E3-F16DEDC3F608"];
    NSAssert(musicPlayerID, @"BGMiTunes::sharedMusicPlayerID: !musicPlayerID");
    return (NSUUID*)musicPlayerID;
}

- (instancetype) init {
    if ((self = [super initWithMusicPlayerID:[BGMiTunes sharedMusicPlayerID]
                                        name:@"iTunes"
                                    bundleID:@"com.apple.iTunes"])) {
        scriptingBridge = [[BGMScriptingBridge alloc] initWithMusicPlayer:self];
    }
    
    return self;
}

- (iTunesApplication* __nullable) iTunes {
    return (iTunesApplication*)scriptingBridge.application;
}

- (void) onSelect {
    [super onSelect];
    [scriptingBridge ensurePermission];
}

- (BOOL) isRunning {
    return self.iTunes.running;
}

// isPlaying and isPaused check self.running first just in case iTunes is closed but self.iTunes hasn't become
// nil yet. In that case, reading self.iTunes.playerState could make Scripting Bridge open iTunes.

- (BOOL) isPlaying {
    return self.running && (self.iTunes.playerState == iTunesEPlSPlaying);
}

- (BOOL) isPaused {
    return self.running && (self.iTunes.playerState == iTunesEPlSPaused);
}

- (BOOL) pause {
    // isPlaying checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPlaying = self.playing;
    
    if (wasPlaying) {
        DebugMsg("BGMiTunes::pause: Pausing iTunes");
        [self.iTunes pause];
    }
    
    return wasPlaying;
}

- (BOOL) unpause {
    // isPaused checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPaused = self.paused;
    
    if (wasPaused) {
        DebugMsg("BGMiTunes::unpause: Unpausing iTunes");
        [self.iTunes playpause];
    }
    
    return wasPaused;
}

@end

#pragma clang assume_nonnull end


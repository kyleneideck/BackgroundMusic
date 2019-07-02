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
//  BGMMusic.m
//  BGMApp
//
//  Copyright © 2016-2019 Kyle Neideck, theLMGN
//

// Self Include
#import "BGMMusic.h"

// Auto-generated Scripting Bridge header
#import "Music.h"

// Local Includes
#import "BGMScriptingBridge.h"

// PublicUtility Includes
#import "CADebugMacros.h"


#pragma clang assume_nonnull begin

@implementation BGMMusic {
    BGMScriptingBridge* scriptingBridge;
}

+ (NSUUID*) sharedMusicPlayerID {
    NSUUID* __nullable musicPlayerID =
            [[NSUUID alloc] initWithUUIDString:@"829B8069-8BD2-481D-BD40-54AB8CDAE228"];
    NSAssert(musicPlayerID, @"BGMMusic::sharedMusicPlayerID: !musicPlayerID");
    return (NSUUID*)musicPlayerID;
}

- (instancetype) init {
    if ((self = [super initWithMusicPlayerID:[BGMMusic sharedMusicPlayerID]
                                        name:@"Music"
                                    bundleID:@"com.apple.Music"])) {
        scriptingBridge = [[BGMScriptingBridge alloc] initWithMusicPlayer:self];
    }
    
    return self;
}

- (MusicApplication* __nullable) music {
    return (MusicApplication*)scriptingBridge.application;
}

- (void) wasSelected {
    [super wasSelected];
    [scriptingBridge ensurePermission];
}

- (BOOL) isRunning {
    return self.music.running;
}

// isPlaying and isPaused check self.running first just in case Music is closed but self.music
// hasn't become nil yet. In that case, reading self.music.playerState could make Scripting Bridge
// open Music.

- (BOOL) isPlaying {
    return self.running && (self.music.playerState == MusicEPlSPlaying);
}

- (BOOL) isPaused {
    return self.running && (self.music.playerState == MusicEPlSPaused);
}

- (BOOL) pause {
    // isPlaying checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPlaying = self.playing;
    
    if (wasPlaying) {
        DebugMsg("BGMMusic::pause: Pausing Music");
        [self.music pause];
    }
    
    return wasPlaying;
}

- (BOOL) unpause {
    // isPaused checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPaused = self.paused;
    
    if (wasPaused) {
        DebugMsg("BGMMusic::unpause: Unpausing Music");
        [self.music playpause];
    }
    
    return wasPaused;
}

@end

#pragma clang assume_nonnull end


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
//  BGMVOX.m
//  BGMApp
//
//  Copyright © 2016-2018 Kyle Neideck
//

// Self Include
#import "BGMVOX.h"

// Auto-generated Scripting Bridge header
#import "VOX.h"

// Local Includes
#import "BGMScriptingBridge.h"

// PublicUtility Includes
#import "CADebugMacros.h"


#pragma clang assume_nonnull begin

@implementation BGMVOX {
    BGMScriptingBridge* scriptingBridge;
}

- (instancetype) init {
    if ((self = [super initWithMusicPlayerID:[BGMMusicPlayerBase makeID:@"26498C5D-C18B-4689-8B41-9DA91A78FFAD"]
                                        name:@"VOX"
                                    bundleID:@"com.coppertino.Vox"])) {
        scriptingBridge = [[BGMScriptingBridge alloc] initWithMusicPlayer:self];
    }
    
    return self;
}

- (VoxApplication* __nullable) vox {
    return (VoxApplication*)scriptingBridge.application;
}

- (void) onSelect {
    [super onSelect];
    [scriptingBridge ensurePermission];
}

- (BOOL) isRunning {
    return self.vox.running;
}

// isPlaying and isPaused check self.running first just in case VOX is closed but self.vox hasn't become
// nil yet. In that case, reading self.vox.playerState could make Scripting Bridge open VOX.
//
// VOX's comment for its playerState property says "playing = 1, paused = 0".

- (BOOL) isPlaying {
    return self.running && (self.vox.playerState == 1);
}

- (BOOL) isPaused {
    return self.running && (self.vox.playerState == 0);
}

- (BOOL) pause {
    // isPlaying checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPlaying = self.playing;
    
    if (wasPlaying) {
        DebugMsg("BGMVOX::pause: Pausing VOX");
        [self.vox pause];
    }
    
    return wasPlaying;
}

- (BOOL) unpause {
    // isPaused checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPaused = self.paused;
    
    if (wasPaused) {
        DebugMsg("BGMVOX::unpause: Unpausing VOX");
        [self.vox playpause];
    }
    
    return wasPaused;
}

@end

#pragma clang assume_nonnull end


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
//  BGMVox.m
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#import "BGMVox.h"

// Auto-generated Scripting Bridge header
#import "Vox.h"

// PublicUtility Includes
#undef CoreAudio_ThreadStampMessages
#define CoreAudio_ThreadStampMessages 0  // Requires C++
#include "CADebugMacros.h"


@implementation BGMVox

BGM_MUSIC_PLAYER_DEFAULT_LOAD_METHOD

+ (NSString*) name {
    return @"Vox";
}

- (CFNumberRef) pid {
    return NULL;
}

+ (CFStringRef) bundleID {
    return CFSTR("com.coppertino.Vox");
}

- (VoxApplication* __nullable) vox {
    return (VoxApplication*) self.sbApplication;
}

- (BOOL) isRunning {
    return self.vox && [self.vox isRunning];
}

- (BOOL) pause {
    // isPlaying checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPlaying = [self isPlaying];
    
    if (wasPlaying) {
        DebugMsg("BGMVox::pause: Pausing Vox");
        [self.vox pause];
    }
    
    return wasPlaying;
}

- (BOOL) unpause {
    // isPaused checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPaused = [self isPaused];
    
    if (wasPaused) {
        DebugMsg("BGMVox::unpause: Unpausing Vox");
        [self.vox playpause];
    }
    
    return wasPaused;
}

// Vox's comment for playerState says "playing = 1, paused = 0"

- (BOOL) isPlaying {
    return [self isRunning] && [self.vox playerState] == 1;
}

- (BOOL) isPaused {
    return [self isRunning] && [self.vox playerState] == 0;
}

@end


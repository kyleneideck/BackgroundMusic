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
//  BGMDecibel.m
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//  Copyright © 2016 Tanner Hoke
//

// Self Include
#import "BGMDecibel.h"

// Auto-generated Scripting Bridge header
#import "Decibel.h"

// PublicUtility Includes
#undef CoreAudio_ThreadStampMessages
#define CoreAudio_ThreadStampMessages 0  // Requires C++
#include "CADebugMacros.h"


@implementation BGMDecibel

BGM_MUSIC_PLAYER_DEFAULT_LOAD_METHOD

+ (NSString*) name {
    return @"Decibel";
}

- (CFNumberRef) pid {
    return NULL;
}

+ (CFStringRef) bundleID {
    return CFSTR("org.sbooth.Decibel");
}

- (DecibelApplication* __nullable) decibel {
    return (DecibelApplication*) self.sbApplication;
}

- (BOOL) isRunning {
    return self.decibel && [self.decibel isRunning];
}

- (BOOL) pause {
    // isPlaying checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPlaying = [self isPlaying];
    
    if (wasPlaying) {
        DebugMsg("BGMDecibel::pause: Pausing Decibel");
        [self.decibel pause];
    }
    
    return wasPlaying;
}

- (BOOL) unpause {
    // isPaused checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPaused = [self isPaused];
    
    if (wasPaused) {
        DebugMsg("BGMDecibel::unpause: Unpausing Decibel");
        [self.decibel playPause];
    }
    
    return wasPaused;
}

- (BOOL) isPlaying {
    return [self isRunning] && [self.decibel playing];
}

- (BOOL) isPaused {
    return [self isRunning] && ![self.decibel playing];
}

@end


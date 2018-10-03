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
//  Copyright © 2016-2018 Kyle Neideck
//  Copyright © 2016 Tanner Hoke
//

// Self Include
#import "BGMDecibel.h"

// Auto-generated Scripting Bridge header
#import "Decibel.h"

// Local Includes
#import "BGMScriptingBridge.h"

// PublicUtility Includes
#import "CADebugMacros.h"


#pragma clang assume_nonnull begin

@implementation BGMDecibel {
    BGMScriptingBridge* scriptingBridge;
}

- (instancetype) init {
    if ((self = [super initWithMusicPlayerID:[BGMMusicPlayerBase makeID:@"A9790CD5-4886-47C7-9FFC-DD70743CF2BF"]
                                        name:@"Decibel"
                                    bundleID:@"org.sbooth.Decibel"])) {
        scriptingBridge = [[BGMScriptingBridge alloc] initWithMusicPlayer:self];
    }
    
    return self;
}

- (DecibelApplication* __nullable) decibel {
    return (DecibelApplication* __nullable)scriptingBridge.application;
}

- (void) onSelect {
    [super onSelect];
    [scriptingBridge ensurePermission];
}

- (BOOL) isRunning {
    return self.decibel.running;
}

- (BOOL) isPlaying {
    return self.running && self.decibel.playing;
}

- (BOOL) isPaused {
    // We don't want to return true when Decibel is stopped, rather than paused. At least for me, Decibel
    // returns -1 for playbackTime and playbackPosition when it's neither playing nor paused.
    BOOL probablyNotStopped =
        self.decibel.playbackTime >= 0 || self.decibel.playbackPosition >= 0;
    
    return self.running && !self.decibel.playing && probablyNotStopped;
}

- (BOOL) pause {
    // isPlaying checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPlaying = self.playing;
    
    if (wasPlaying) {
        DebugMsg("BGMDecibel::pause: Pausing Decibel");
        [self.decibel pause];
    }
    
    return wasPlaying;
}

- (BOOL) unpause {
    // isPaused checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPaused = self.paused;
    
    if (wasPaused) {
        DebugMsg("BGMDecibel::unpause: Unpausing Decibel");
        [self.decibel play];
    }
    
    return wasPaused;
}

@end

#pragma clang assume_nonnull end


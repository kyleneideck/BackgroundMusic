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
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#import "BGMiTunes.h"

// Auto-generated Scripting Bridge header
#import "iTunes.h"

// PublicUtility Includes
#undef CoreAudio_ThreadStampMessages
#define CoreAudio_ThreadStampMessages 0  // Requires C++
#include "CADebugMacros.h"


@implementation BGMiTunes {
    iTunesApplication* iTunes;
}

+ (void) load {
    BGM_MUSIC_PLAYER_ADD_SELF_TO_CLASSES_LIST
    
    // iTunes is selected as the music player when the user hasn't changed the setting yet
    [self setSelectedMusicPlayer:[BGMiTunes new]];
}

+ (NSString*) name {
    return @"iTunes";
}

- (id) init {
    if ((self = [super init])) {
        iTunes = [SBApplication applicationWithBundleIdentifier:(__bridge NSString*)[[self class] bundleID]];
    }
    
    return self;
}

- (CFNumberRef) pid {
    return NULL;
}

+ (CFStringRef) bundleID {
    return CFSTR("com.apple.iTunes");
}

- (BOOL) isRunning {
    return [iTunes isRunning];
}

- (BOOL) pause {
    // isPlaying checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPlaying = [self isPlaying];
    
    if (wasPlaying) {
        DebugMsg("BGMiTunes::pause: Pausing iTunes");
        [iTunes pause];
    }
    
    return wasPlaying;
}

- (BOOL) unpause {
    // isPaused checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPaused = [self isPaused];
    
    if (wasPaused) {
        DebugMsg("BGMiTunes::unpause: Unpausing iTunes");
        [iTunes playpause];
    }
    
    return wasPaused;
}

- (BOOL) isPlaying {
    return [self isRunning] && [iTunes playerState] == iTunesEPlSPlaying;
}

- (BOOL) isPaused {
    return [self isRunning] && [iTunes playerState] == iTunesEPlSPaused;
}

@end


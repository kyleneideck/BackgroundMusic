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
//  BGMSpotify.m
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//
//  Spotify's AppleScript API looks to have been designed to match iTunes', so this file is basically
//  just s/iTunes/Spotify/ on BGMiTunes.m
//

// Self Include
#import "BGMSpotify.h"

// Auto-generated Scripting Bridge header
#import "Spotify.h"

// PublicUtility Includes
#undef CoreAudio_ThreadStampMessages
#define CoreAudio_ThreadStampMessages 0  // Requires C++
#include "CADebugMacros.h"


@implementation BGMSpotify

BGM_MUSIC_PLAYER_DEFAULT_LOAD_METHOD

+ (NSString*) name {
    return @"Spotify";
}

- (CFNumberRef) pid {
    return NULL;
}

+ (CFStringRef) bundleID {
    return CFSTR("com.spotify.client");
}

- (SpotifyApplication* __nullable) spotify {
    return (SpotifyApplication*) self.sbApplication;
}

- (BOOL) isRunning {
    return self.spotify && [self.spotify isRunning];
}

- (BOOL) pause {
    // isPlaying checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPlaying = [self isPlaying];
    
    if (wasPlaying) {
        DebugMsg("BGMSpotify::pause: Pausing Spotify");
        [self.spotify pause];
    }
    
    return wasPlaying;
}

- (BOOL) unpause {
    // isPaused checks isRunning, so we don't need to check it here and waste an Apple event
    BOOL wasPaused = [self isPaused];
    
    if (wasPaused) {
        DebugMsg("BGMSpotify::unpause: Unpausing Spotify");
        [self.spotify playpause];
    }
    
    return wasPaused;
}

- (BOOL) isPlaying {
    return [self isRunning] && [self.spotify playerState] == SpotifyEPlSPlaying;
}

- (BOOL) isPaused {
    return [self isRunning] && [self.spotify playerState] == SpotifyEPlSPaused;
}

@end


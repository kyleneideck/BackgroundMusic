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
//  BGMMusicPlayer.m
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//

#import "BGMMusicPlayer.h"

#import <Cocoa/Cocoa.h>


@implementation BGMMusicPlayerBase

// A array of the subclasses of BGMMusicPlayer
static NSArray* sMusicPlayerClasses;

// The user-selected music player. One of BGMMusicPlayer's subclasses declares itself the default music player by
// setting this to an instance of itself in its load method.
static BGMMusicPlayer* sSelectedMusicPlayer;

// Load-time static initializer
+ (void) load {
    sMusicPlayerClasses = @[];
}

+ (void) addToMusicPlayerClasses:(Class)musicPlayerClass {
    sMusicPlayerClasses = [sMusicPlayerClasses arrayByAddingObject:musicPlayerClass];
}

+ (NSArray*) musicPlayerClasses {
    return sMusicPlayerClasses;
}

+ (BGMMusicPlayer*) selectedMusicPlayer {
    NSAssert(sSelectedMusicPlayer != nil, @"One of BGMMusicPlayer's subclasses should set itself as the default \
                                            music player (i.e. set sSelectedMusicPlayer) in its initialize method");
    return sSelectedMusicPlayer;
}

+ (void) setSelectedMusicPlayer:(BGMMusicPlayer*)musicPlayer {
    sSelectedMusicPlayer = musicPlayer;
}

+ (NSImage*) icon {
    NSString* bundleID = (__bridge NSString*)[(id<BGMMusicPlayerProtocol>)self bundleID];
    NSString* bundlePath = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:bundleID];
    return bundlePath == nil ? nil : [[NSWorkspace sharedWorkspace] iconForFile:bundlePath];
}

@end


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

// Self Include
#import "BGMMusicPlayer.h"

// PublicUtility Includes
#undef CoreAudio_ThreadStampMessages
#define CoreAudio_ThreadStampMessages 0  // Requires C++
#include "CADebugMacros.h"

// System Includes
#import <Cocoa/Cocoa.h>


#pragma clang assume_nonnull begin

@implementation BGMMusicPlayerBase {
    // Tokens for the notification observers. We need these to remove the observers in dealloc.
    id didLaunchToken;
    id didTerminateToken;
}

@synthesize sbApplication = sbApplication;

// A array of the subclasses of BGMMusicPlayer
static NSArray* sMusicPlayerClasses;

// The user-selected music player. One of BGMMusicPlayer's subclasses declares itself the default music player by
// setting this to an instance of itself in its load method.
static BGMMusicPlayer* sSelectedMusicPlayer;

// Load-time static initializer
+ (void) load {
    sMusicPlayerClasses = @[];
}

- (id) init {
    if ((self = [super init])) {
        NSString* bundleID = (__bridge NSString*)[[self class] bundleID];
        
        void (^createSBApplication)(void) = ^{
            sbApplication = [SBApplication applicationWithBundleIdentifier:bundleID];
            sbApplication.delegate = self;
        };
        
        BOOL (^isAboutThisMusicPlayer)(NSNotification*) = ^(NSNotification* note){
            return [[note.userInfo[NSWorkspaceApplicationKey] bundleIdentifier] isEqualToString:bundleID];
        };
        
        // Add observers that create/destroy the SBApplication when the music player is launched/terminated. We
        // only create the SBApplication when the music player is open because, if it isn't, creating the
        // SBApplication, or sending it events, could launch the music player. Whether it does or not depends on
        // the music player, and possibly the version of the music player, so to be safe we assume they all do.
        //
        // From the docs for SBApplication's applicationWithBundleIdentifier method:
        //     "For applications that declare themselves to have a dynamic scripting interface, this method will
        //     launch the application if it is not already running."
        NSNotificationCenter* center = [[NSWorkspace sharedWorkspace] notificationCenter];
#if DEBUG
        const char* mpName = [[[self class] name] UTF8String];
#endif
        didLaunchToken = [center addObserverForName:NSWorkspaceDidLaunchApplicationNotification
                                             object:nil
                                              queue:nil
                                         usingBlock:^(NSNotification* note) {
                                             if (isAboutThisMusicPlayer(note)) {
                                                 DebugMsg("BGMMusicPlayer::init: %s launched", mpName);
                                                 createSBApplication();
                                             }
                                         }];
        didTerminateToken = [center addObserverForName:NSWorkspaceDidTerminateApplicationNotification
                                                object:nil
                                                 queue:nil
                                            usingBlock:^(NSNotification* note) {
                                             if (isAboutThisMusicPlayer(note)) {
                                                 DebugMsg("BGMMusicPlayer::init: %s terminated", mpName);
                                                 sbApplication = nil;
                                                }
                                            }];
        
        // Create the SBApplication if the music player is already running.
        if ([[NSRunningApplication runningApplicationsWithBundleIdentifier:bundleID] count] > 0) {
            createSBApplication();
        }
    }
    
    return self;
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101200
- (__nullable id) eventDidFail:(const AppleEvent*)event withError:(NSError*)error {
#else
- (id) eventDidFail:(const AppleEvent*)event withError:(NSError*)error {
#endif
    // SBApplicationDelegate method. So far, this just logs the error.
    
#if DEBUG
    NSString* vars = [NSString stringWithFormat:@"event=%@ error=%@ sbApplication=%@", event, error, sbApplication];
    DebugMsg("BGMMusicPlayer::eventDidFail: Apple event sent to %s failed. %s",
             [[[self class] name] UTF8String],
             [vars UTF8String]);
#else
    #pragma unused (event, error)
#endif
    
    return nil;
}

- (void) dealloc {
    // Remove the application launch/termination observers.
    NSNotificationCenter* center = [[NSWorkspace sharedWorkspace] notificationCenter];
    
    if (didLaunchToken) {
        [center removeObserver:didLaunchToken];
    }
    
    if (didTerminateToken) {
        [center removeObserver:didTerminateToken];
    }
}

+ (void) addToMusicPlayerClasses:(Class)musicPlayerClass {
    sMusicPlayerClasses = [sMusicPlayerClasses arrayByAddingObject:musicPlayerClass];
}

+ (NSArray*) musicPlayerClasses {
    return sMusicPlayerClasses;
}

+ (BGMMusicPlayer*) selectedMusicPlayer {
    NSAssert(sSelectedMusicPlayer != nil, @"One of BGMMusicPlayer's subclasses should set itself as the default "
                                           "music player (i.e. set sSelectedMusicPlayer) in its initialize method");
    return sSelectedMusicPlayer;
}

+ (void) setSelectedMusicPlayer:(BGMMusicPlayer*)musicPlayer {
    sSelectedMusicPlayer = musicPlayer;
}

+ (NSImage* __nullable) icon {
    NSString* bundleID = (__bridge NSString*)[(id<BGMMusicPlayerProtocol>)self bundleID];
    NSString* bundlePath = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:bundleID];
    return bundlePath == nil ? nil : [[NSWorkspace sharedWorkspace] iconForFile:bundlePath];
}

@end

#pragma clang assume_nonnull end


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
//  BGMScriptingBridge.m
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#import "BGMScriptingBridge.h"

// PublicUtility Includes
#undef CoreAudio_ThreadStampMessages
#define CoreAudio_ThreadStampMessages 0  // Requires C++
#include "CADebugMacros.h"


#pragma clang assume_nonnull begin

@implementation BGMScriptingBridge {
    NSString* bundleID;
    // Tokens for the notification observers. We need these to remove the observers in dealloc.
    id didLaunchToken, didTerminateToken;
}

@synthesize application = _application;

- (instancetype) initWithBundleID:(NSString*)inBundleID {
    if ((self = [super init])) {
        bundleID = inBundleID;
        
        [self initApplication];
    }
    
    return self;
}

- (void) initApplication {
    void (^createSBApplication)(void) = ^{
        _application = [SBApplication applicationWithBundleIdentifier:bundleID];
        _application.delegate = self;
    };
    
    BOOL (^isAboutThisMusicPlayer)(NSNotification*) = ^(NSNotification* note) {
        return [[note.userInfo[NSWorkspaceApplicationKey] bundleIdentifier] isEqualToString:bundleID];
    };
    
    // Add observers that create/destroy the SBApplication when the music player is launched/terminated. We
    // only create the SBApplication when the music player is open. If it isn't open, creating the
    // SBApplication or sending it events could launch the music player. Whether or not it does depends on
    // the music player, and possibly the version of the music player, so to be safe we assume they all do.
    //
    // From the docs for SBApplication's applicationWithBundleIdentifier method:
    //     "For applications that declare themselves to have a dynamic scripting interface, this method will
    //     launch the application if it is not already running."
    NSNotificationCenter* center = [[NSWorkspace sharedWorkspace] notificationCenter];
    didLaunchToken = [center addObserverForName:NSWorkspaceDidLaunchApplicationNotification
                                         object:nil
                                          queue:nil
                                     usingBlock:^(NSNotification* note) {
                                         if (isAboutThisMusicPlayer(note)) {
                                             DebugMsg("BGMScriptingBridge::initApplication: %s launched",
                                                      bundleID.UTF8String);
                                             createSBApplication();
                                         }
                                     }];
    didTerminateToken = [center addObserverForName:NSWorkspaceDidTerminateApplicationNotification
                                            object:nil
                                             queue:nil
                                        usingBlock:^(NSNotification* note) {
                                            if (isAboutThisMusicPlayer(note)) {
                                                DebugMsg("BGMScriptingBridge::initApplication: %s terminated",
                                                         bundleID.UTF8String);
                                                _application = nil;
                                            }
                                        }];
    
    // Create the SBApplication if the music player is already running.
    if ([NSRunningApplication runningApplicationsWithBundleIdentifier:bundleID].count > 0) {
        createSBApplication();
    }
}

- (void) dealloc {
    // Remove the application launch/termination observers.
    NSNotificationCenter* center = [NSWorkspace sharedWorkspace].notificationCenter;
    
    if (didLaunchToken) {
        [center removeObserver:didLaunchToken];
    }
    
    if (didTerminateToken) {
        [center removeObserver:didTerminateToken];
    }
}

#pragma mark SBApplicationDelegate

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability"  // See explanation in the header file.
- (id __nullable) eventDidFail:(const AppleEvent*)event withError:(NSError*)error {
#pragma clang diagnostic pop
    // So far, this just logs the error.
    
#if DEBUG
    NSString* vars = [NSString stringWithFormat:@"event='%4.4s' error=%@ application=%@",
                         (char*)&(event->descriptorType), error, self.application];
    DebugMsg("BGMScriptingBridge::eventDidFail: Apple event sent to %s failed. %s",
             bundleID.UTF8String,
             vars.UTF8String);
#else
    #pragma unused (event, error)
#endif

    return nil;
}

@end

#pragma clang assume_nonnull end


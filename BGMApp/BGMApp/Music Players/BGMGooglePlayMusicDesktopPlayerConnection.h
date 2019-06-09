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
//  BGMGooglePlayMusicDesktopPlayerConnection.h
//  BGMApp
//
//  Copyright © 2019 Kyle Neideck
//

// Local Includes
#import "BGMUserDefaults.h"

// System Includes
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>


#pragma clang assume_nonnull begin

API_AVAILABLE(macos(10.10))
@interface BGMGooglePlayMusicDesktopPlayerConnection : NSObject<WKScriptMessageHandler>

// authRequiredHandler: A UI callback that asks the user for the auth code GPMDP will display.
//                      Returns the auth code they entered, or nil.
// connectionErrorHandler: A UI callback that shows a connection error message.
// apiVersionMismatchHandler: A UI callback that shows a warning dialog explaining that GPMDP
//                            reported an API version that we don't support yet.
- (instancetype) initWithUserDefaults:(BGMUserDefaults*)defaults
                  authRequiredHandler:(NSString* __nullable (^)(void))authHandler
               connectionErrorHandler:(void (^)(void))errorHandler
            apiVersionMismatchHandler:(void (^)(NSString* reportedAPIVersion))apiVersionHandler;

// Returns before the connection has been fully established. The playing and paused properties will
// remain false until the connection is complete, but playPause can be called at any time after
// calling this method.
//
// If the connection fails, it will be retried after a one second delay, up to the number of times
// given.
- (void) connectWithRetries:(int)retries;
- (void) disconnect;

// Tell GPMDP to play if it's paused or pause if it's playing.
- (void) playPause;

@property (readonly) BOOL playing;
@property (readonly) BOOL paused;

@end

#pragma clang assume_nonnull end


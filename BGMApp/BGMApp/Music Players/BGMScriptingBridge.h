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
//  BGMScriptingBridge.h
//  BGMApp
//
//  Copyright © 2016, 2018 Kyle Neideck
//
//  A wrapper around Scripting Bridge's SBApplication that tries to avoid ever launching the application.
//
//  We use Scripting Bridge to communicate with music player apps, which we never want to launch
//  ourselves. But creating an SBApplication for an app, or sending messages/events to an existing one,
//  can launch the app.
//
//  As a workaround, this class has an SBApplication property, application (see below), which is nil
//  unless the music player app is running. That way messages sent while the app is closed are ignored.
//

// Local Includes
#import "BGMMusicPlayer.h"

// System Includes
#import <Cocoa/Cocoa.h>
#import <ScriptingBridge/ScriptingBridge.h>


#pragma clang assume_nonnull begin

@interface BGMScriptingBridge : NSObject <SBApplicationDelegate>

// Only keeps a weak ref to musicPlayer.
- (instancetype) initWithMusicPlayer:(id<BGMMusicPlayer>)musicPlayer;

// If the music player application is running, this property is the Scripting Bridge object representing
// it. If not, it's set to nil. Used to send Apple events to the music player app.
@property (readonly) __kindof SBApplication* __nullable application;

// macOS 10.14 requires the user's permission to send Apple Events. If the music player that owns
// this object (i.e. the one passed to initWithMusicPlayer) is currently the selected music player
// and the user hasn't already given us permission to send it Apple Events, this method asks the
// user for permission.
- (void) ensurePermission;

// SBApplicationDelegate

// On 10.11, SBApplicationDelegate.h declares eventDidFail with a non-null return type, but the docs
// specifically say that returning nil is allowed.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability"
- (id __nullable) eventDidFail:(const AppleEvent*)event withError:(NSError*)error;
#pragma clang diagnostic pop

@end

#pragma clang assume_nonnull end


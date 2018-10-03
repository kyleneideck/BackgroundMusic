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
//  BGMMusicPlayer.h
//  BGMApp
//
//  Copyright © 2016, 2018 Kyle Neideck
//
//  The base classes and protocol for objects that represent a music player app.
//
//  To add support for a music player, create a class that implements the BGMMusicPlayer protocol
//  and add it to initWithAudioDevices in BGMMusicPlayers.mm.
//
//  You'll probably want to subclass BGMMusicPlayerBase and, if the music player supports
//  AppleScript, use BGMScriptingBridge. Your class might need to override the icon method if the
//  default implementation from BGMMusicPlayerBase doesn't work.
//
//  BGMSpotify will probably be the most useful example to follow, but they're all pretty
//  similar. The music player classes written so far all use Scripting Bridge to communicate with
//  the music player apps (see iTunes.h/Spotify.h) but any other way is fine too.
//
//  BGMDriver will use either the music player's bundle ID or PID to match it to the audio it
//  plays. (Though using PIDs hasn't been tested yet.)
//
//  If you're not sure what bundle ID the music player uses, install a debug build of BGMDriver
//  and play something in the music player. The easiest way is to do
//      build_and_install.sh -d
//  BGMDriver will log the bundle ID to system.log when it becomes aware of the music player.
//

// System Includes
#import <Cocoa/Cocoa.h>


#pragma clang assume_nonnull begin

@protocol BGMMusicPlayer <NSObject>

// Classes return an instance of themselves for each music player app they make available in
// BGMApp. So far that's always been a single instance, and classes haven't needed to override
// the default implementation of createInstances from BGMMusicPlayerBase. But that will probably
// change eventually.
//
// For example, a class for custom music players would probably return an instance for each
// custom player the user has created. (Also note that it could return an empty array.) In that
// case the class would probably restore some state from user defaults in its createInstances.
//
// TODO: I think the return type should actually be NSArray<instancetype>*, but that doesn't seem
//       to work. There's a Clang bug about this: https://llvm.org/bugs/show_bug.cgi?id=27323
//       (though it hasn't been confirmed yet).
+ (NSArray<id<BGMMusicPlayer>>*) createInstances;

// We need a unique ID for each music player to store in user defaults. In the most common case,
// classes that provide a static (or at least bounded) number of music players, you can generate
// IDs with uuidgen (the command line tool) and include them in your class as constants. Otherwise,
// you'll probably want to store them in user defaults and retrieve them in your createInstances.
@property (readonly) NSUUID* musicPlayerID;

// The name and icon of the music player, to be used in the UI.
@property (readonly) NSString* name;
@property (readonly) NSImage* __nullable icon;

@property (readonly) NSString* __nullable bundleID;

// Classes will usually ignore this property and leave it nil unless the music player has no
// bundle ID.
//
// TODO: If we ever add a music player class that uses this property, it'll need a way to inform
//       BGMDevice of changes. It might be easiest to have BGMMusicPlayers to observe this property,
//       on the selected music player, with KVO and update BGMDevice when it changes. Or
//       BGMMusicPlayers could pass a pointer to itself to createInstances.
@property NSNumber* __nullable pid;

// True if this is currently the selected music player.
@property (readonly) BOOL selected;

// The state of the music player.
//
// True if the music player app is open.
@property (readonly, getter=isRunning) BOOL running;
// True if the music player is playing a song or some other user-selected audio file. Note that
// the music player playing audio for UI, notifications, etc. won't make this true (which is why we
// need this property and can't just ask BGMDriver if the music player is playing audio).
@property (readonly, getter=isPlaying) BOOL playing;
// True if the music player has a current/open song (or whatever) and will continue playing it if
// BGMMusicPlayer::unpause is called. Normally because the user was playing a song and they or
// BGMApp paused it.
@property (readonly, getter=isPaused) BOOL paused;

// Called when the user selects this music player.
- (void) onSelect;
// Called when this is the selected music player and the user selects a different one.
- (void) onDeselect;

// Pause the music player. Does nothing if the music player is already paused or isn't running.
// Returns YES if the music player is paused now but wasn't before, returns NO otherwise.
- (BOOL) pause;
// Unpause the music player. Does nothing if the music player is already playing or isn't running.
// Returns YES if the music player is playing now but wasn't before, returns NO otherwise.
- (BOOL) unpause;

@end


@interface BGMMusicPlayerBase : NSObject

- (instancetype) initWithMusicPlayerID:(NSUUID*)musicPlayerID
                                  name:(NSString*)name
                              bundleID:(NSString* __nullable)bundleID;

- (instancetype) initWithMusicPlayerID:(NSUUID*)musicPlayerID
                                  name:(NSString*)name
                              bundleID:(NSString* __nullable)bundleID
                                   pid:(NSNumber* __nullable)pid;

// Convenience wrapper around NSUUID's initWithUUIDString. musicPlayerIDString must be a string
// generated by uuidgen (command line tool), e.g. "60BA9739-B6DD-4E6A-8134-51410A45BB84".
+ (NSUUID*) makeID:(NSString*)musicPlayerIDString;

// BGMMusicPlayer default implementations
+ (NSArray<id<BGMMusicPlayer>>*) createInstances;
@property (readonly) NSImage* __nullable icon;
@property (readonly) NSUUID* musicPlayerID;
@property (readonly) NSString* name;
@property (readonly) NSString* __nullable bundleID;
@property NSNumber* __nullable pid;
@property (readonly) BOOL selected;
- (void) onSelect;
- (void) onDeselect;

@end

#pragma clang assume_nonnull end


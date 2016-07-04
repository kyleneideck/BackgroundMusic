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
//  Copyright © 2016 Kyle Neideck
//
//  The base class and protocol for music player apps. Also holds the state of the currently
//  selected music player.
//
//  To add support for a music player, create a subclass of BGMMusicPlayerBase that implements
//  BGMMusicPlayerProtocol. BGMSpotify will probably be the most useful example.
//
//  Include the BGM_MUSIC_PLAYER_DEFAULT_LOAD_METHOD macro somewhere in the @implementation block.
//  You might also want to override the icon method if the default implementation from
//  BGMMusicPlayerBase doesn't work.
//
//  The music player classes written so far use Scripting Bridge to communicate with the music
//  player apps (see iTunes.h/Spotify.h) but any other way is fine too.
//
//  If you're not sure what bundle ID the music player uses, install a debug build of BGMDriver
//  and play something in the music player. The easiest way is to do
//      build_and_install.sh -d
//  BGMDriver will log the bundle ID to system.log when it becomes aware of the music player.
//

// System Includes
#import <Foundation/Foundation.h>
#import <ScriptingBridge/ScriptingBridge.h>


#pragma clang assume_nonnull begin

#define BGM_MUSIC_PLAYER_ADD_SELF_TO_CLASSES_LIST \
    [BGMMusicPlayerBase addToMusicPlayerClasses:[self class]];

#define BGM_MUSIC_PLAYER_DEFAULT_LOAD_METHOD \
    + (void) load { \
        BGM_MUSIC_PLAYER_ADD_SELF_TO_CLASSES_LIST \
    }

// Forward declarations (just for the typedef)
@class BGMMusicPlayerBase;
@protocol BGMMusicPlayerProtocol;

typedef BGMMusicPlayerBase<BGMMusicPlayerProtocol> BGMMusicPlayer;

@protocol BGMMusicPlayerProtocol

@optional
// Subclasses usually won't need to implement these unless the music player has no bundle ID.
+ (id) initWithPID:(pid_t)pid;
+ (id) initWithPIDFromNSNumber:(NSNumber*)pid;
+ (id) initWithPIDFromCFNumber:(CFNumberRef)pid;
// The pid of each instance of the music player app currently running
+ (NSArray<NSNumber*>*) pidsOfRunningInstances;

@required
// The name of the music player, to be used in the UI
+ (NSString*) name;

// The refs returned by the bundleID and pid methods don't need to be released by users, but may be
// released by the class/instance at some point (get rule applies).
+ (CFStringRef __nullable) bundleID;
// Subclasses will usually always return NULL unless they implement the optional methods above.
- (CFNumberRef __nullable) pid;

- (BOOL) isRunning;

// Pause the music player. Does nothing if the music player is already paused or isn't running.
// Returns YES if the music player is paused now but wasn't before, returns NO otherwise.
- (BOOL) pause;

// Unpause the music player. Does nothing if the music player is already playing or isn't running.
// Returns YES if the music player is playing now but wasn't before, returns NO otherwise.
- (BOOL) unpause;

- (BOOL) isPlaying;

- (BOOL) isPaused;

@end

@interface BGMMusicPlayerBase : NSObject <SBApplicationDelegate>

+ (NSArray*) musicPlayerClasses;
+ (void) addToMusicPlayerClasses:(Class)musicPlayerClass;

// The music player currently selected in the preferences menu. (There's no real reason for this to be
// global or in this class. I was just trying it out of curiosity.)
+ (BGMMusicPlayer*) selectedMusicPlayer;
+ (void) setSelectedMusicPlayer:(BGMMusicPlayer*)musicPlayer;

+ (NSImage* __nullable) icon;

// If the music player application is running, the scripting bridge object representing it. Otherwise
// nil.
@property (readonly) __kindof SBApplication* __nullable sbApplication;

@end

#pragma clang assume_nonnull end


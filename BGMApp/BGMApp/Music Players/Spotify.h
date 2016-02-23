/*
 * Spotify.h
 *
 * Generated with
 * sdef /Applications/Spotify.app | sdp -fh --basename Spotify
 */

#import <AppKit/AppKit.h>
#import <ScriptingBridge/ScriptingBridge.h>


@class SpotifyApplication, SpotifyTrack, SpotifyApplication;

enum SpotifyEPlS {
	SpotifyEPlSStopped = 'kPSS',
	SpotifyEPlSPlaying = 'kPSP',
	SpotifyEPlSPaused = 'kPSp'
};
typedef enum SpotifyEPlS SpotifyEPlS;



/*
 * Spotify Suite
 */

// The Spotify application.
@interface SpotifyApplication : SBApplication

@property (copy, readonly) SpotifyTrack *currentTrack;  // The current playing track.
@property NSInteger soundVolume;  // The sound output volume (0 = minimum, 100 = maximum)
@property (readonly) SpotifyEPlS playerState;  // Is Spotify stopped, paused, or playing?
@property double playerPosition;  // The player’s position within the currently playing track in seconds.
@property (readonly) BOOL repeatingEnabled;  // Is repeating enabled in the current playback context?
@property BOOL repeating;  // Is repeating on or off?
@property (readonly) BOOL shufflingEnabled;  // Is shuffling enabled in the current playback context?
@property BOOL shuffling;  // Is shuffling on or off?

- (void) nextTrack;  // Skip to the next track.
- (void) previousTrack;  // Skip to the previous track.
- (void) playpause;  // Toggle play/pause.
- (void) pause;  // Pause playback.
- (void) play;  // Resume playback.
- (void) playTrack:(NSString *)x inContext:(NSString *)inContext;  // Start playback of a track in the given context.

@end

// A Spotify track.
@interface SpotifyTrack : SBObject

@property (copy, readonly) NSString *artist;  // The artist of the track.
@property (copy, readonly) NSString *album;  // The album of the track.
@property (readonly) NSInteger discNumber;  // The disc number of the track.
@property (readonly) NSInteger duration;  // The length of the track in seconds.
@property (readonly) NSInteger playedCount;  // The number of times this track has been played.
@property (readonly) NSInteger trackNumber;  // The index of the track in its album.
@property (readonly) BOOL starred;  // Is the track starred?
@property (readonly) NSInteger popularity;  // How popular is this track? 0-100
- (NSString *) id;  // The ID of the item.
@property (copy, readonly) NSString *name;  // The name of the track.
@property (copy, readonly) NSImage *artwork;  // The track's album cover.
@property (copy, readonly) NSString *albumArtist;  // That album artist of the track.
@property (copy) NSString *spotifyUrl;  // The URL of the track.


@end



/*
 * Standard Suite
 */

// The application's top level scripting object.
@interface SpotifyApplication (StandardSuite)

@property (copy, readonly) NSString *name;  // The name of the application.
@property (readonly) BOOL frontmost;  // Is this the frontmost (active) application?
@property (copy, readonly) NSString *version;  // The version of the application.

@end


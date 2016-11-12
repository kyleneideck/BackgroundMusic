/*
 * Hermes.h
 *
 * Generated with
 * sdef /Applications/Hermes.app | sdp -fh --basename Hermes
 */

#import <AppKit/AppKit.h>
#import <ScriptingBridge/ScriptingBridge.h>


@class HermesApplication, HermesSong, HermesStation;

// Legal player states
enum HermesPlayerStates {
	HermesPlayerStatesStopped = 'stop' /* Player is stopped */,
	HermesPlayerStatesPlaying = 'play' /* Player is playing */,
	HermesPlayerStatesPaused = 'paus' /* Player is paused */
};
typedef enum HermesPlayerStates HermesPlayerStates;



/*
 * Hermes Suite
 */

// The Pandora player.
@interface HermesApplication : SBApplication

- (SBElementArray<HermesStation *> *) stations;

@property NSInteger playbackVolume;  // The current playback volume (0–100).
@property HermesPlayerStates playbackState;  // The current playback state.
@property (readonly) double playbackPosition;  // The current song’s playback position, in seconds.
@property (readonly) double currentSongDuration;  // The duration (length) of the current song, in seconds.
@property (copy) HermesStation *currentStation;  // The currently selected Pandora station.
@property (copy, readonly) HermesSong *currentSong;  // The currently playing (or paused) Pandora song (WARNING: This is an invalid reference in current versions of Hermes; you must access the current song’s properties individually or as a group directly instead.)

- (void) playpause;  // Play the current song if it is paused; pause the current song if it is playing.
- (void) pause;  // Pause the currently playing song.
- (void) play;  // Resume playing the current song.
- (void) nextSong;  // Skip to the next song on the current station.
- (void) thumbsUp;  // Tell Pandora you like the current song.
- (void) thumbsDown;  // Tell Pandora you don’t like the current song.
- (void) tiredOfSong;  // Tell Pandora you’re tired of the current song.
- (void) increaseVolume;  // Increase the playback volume.
- (void) decreaseVolume;  // Decrease the playback volume.
- (void) maximizeVolume;  // Set the playback volume to its maximum level.
- (void) mute;  // Mutes playback, saving the current volume level.
- (void) unmute;  // Restores the volume to the level prior to muting.

@end

// A Pandora song (track).
@interface HermesSong : SBObject

@property (copy, readonly) NSString *title;  // The song’s title.
@property (copy, readonly) NSString *artist;  // The song’s artist.
@property (copy, readonly) NSString *album;  // The song’s album.
@property (copy, readonly) NSString *artworkURL;  // An image URL for the album’s cover artwork.
@property (readonly) NSInteger rating;  // The song’s numeric rating.
@property (copy, readonly) NSString *albumURL;  // A Pandora URL for more information on the album.
@property (copy, readonly) NSString *artistURL;  // A Pandora URL for more information on the artist.
@property (copy, readonly) NSString *trackURL;  // A Pandora URL for more information on the track.


@end

// A Pandora station.
@interface HermesStation : SBObject

@property (copy, readonly) NSString *name;  // The station’s name.
@property (copy, readonly) NSString *stationID;  // The station’s ID.


@end


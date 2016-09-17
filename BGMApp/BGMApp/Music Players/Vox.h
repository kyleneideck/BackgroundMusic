/*
 * VOX.h
 *
 * Generated with
 * sdef /Applications/VOX.app | sdp -fh --basename VOX
 */

#import <AppKit/AppKit.h>
#import <ScriptingBridge/ScriptingBridge.h>


@class VoxApplication, VoxApplication;



/*
 * Standard Suite
 */

// The application's top level scripting object.
@interface VoxApplication : SBApplication

@property (copy, readonly) NSString *name;  // The name of the application.
@property (readonly) BOOL frontmost;  // Is this the frontmost (active) application?
@property (copy, readonly) NSString *version;  // The version of the application.

- (void) quit;  // Quit an application.
- (void) pause;  // Pause playback.
- (void) play;  // Begin playback.
- (void) playpause;  // Toggle playback between playing and paused.
- (void) next;  // Skip to the next track in the playlist.
- (void) previous;  // Skip to the previous track in the playlist.
- (void) shuffle;  // Shuffle the tracks in the playlist.
- (void) playUrl:(NSString *)x;  // Play specified URL.
- (void) addUrl:(NSString *)x;  // Add specified URL to playlist
- (void) rewindForward;  // Rewind current track forward.
- (void) rewindForwardFast;  // Rewind current track forward fast.
- (void) rewindBackward;  // Rewind current track backward.
- (void) rewindBackwardFast;  // Rewind current track backward fast.
- (void) increasVolume;  // Increas volume.
- (void) decreaseVolume;  // Decrease volume.
- (void) showHidePlaylist;  // Show/Hide playlist.

@end



/*
 * Vox Suite
 */

// The application's top-level scripting object.
@interface VoxApplication (VoxSuite)

@property (copy, readonly) NSData *tiffArtworkData;  // Current track artwork data in TIFF format.
@property (copy, readonly) NSImage *artworkImage;  // Current track artwork as an image.
@property (readonly) NSInteger playerState;  // Player state (playing = 1, paused = 0)
@property (copy, readonly) NSString *track;  // Current track title.
@property (copy, readonly) NSString *trackUrl;  // Current track URL.
@property (copy, readonly) NSString *artist;  // Current track artist.
@property (copy, readonly) NSString *albumArtist;  // Current track album artist.
@property (copy, readonly) NSString *album;  // Current track album.
@property (copy, readonly) NSString *uniqueID;  // Unique identifier for the current track.
@property double currentTime;  // The current playback position.
@property (readonly) double totalTime;  // The total time of the currenty playing track.
@property double playerVolume;  // Player volume (0.0 to 1.0)
@property NSInteger repeatState;  // Player repeat state (none = 0, repeat one = 1, repeat all = 2)

@end


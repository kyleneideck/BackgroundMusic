/*
 * Swinsian.h
 *
 * Generated with
 * sdef /Applications/Swinsian.app | sdp -fh --basename Swinsian
 */

#import <AppKit/AppKit.h>
#import <ScriptingBridge/ScriptingBridge.h>


@class SwinsianItem, SwinsianColor, SwinsianWindow, SwinsianApplication, SwinsianPlaylist, SwinsianLibrary, SwinsianTrack, SwinsianLibraryTrack, SwinsianIPodTrack, SwinsianQueue, SwinsianSmartPlaylist, SwinsianNormalPlaylist, SwinsianPlaylistFolder, SwinsianAudioDevice;

enum SwinsianSaveOptions {
	SwinsianSaveOptionsYes = 'yes ' /* Save the file. */,
	SwinsianSaveOptionsNo = 'no  ' /* Do not save the file. */,
	SwinsianSaveOptionsAsk = 'ask ' /* Ask the user whether or not to save the file. */
};
typedef enum SwinsianSaveOptions SwinsianSaveOptions;

enum SwinsianPlayerState {
	SwinsianPlayerStateStopped = 'kPSS',
	SwinsianPlayerStatePlaying = 'kPSP',
	SwinsianPlayerStatePaused = 'kPSp'
};
typedef enum SwinsianPlayerState SwinsianPlayerState;

@protocol SwinsianGenericMethods

- (void) closeSaving:(SwinsianSaveOptions)saving savingIn:(NSURL *)savingIn;  // Close an object.
- (void) delete;  // Delete an object.
- (void) duplicateTo:(SBObject *)to withProperties:(NSDictionary *)withProperties;  // Copy object(s) and put the copies at a new location.
- (BOOL) exists;  // Verify if an object exists.
- (void) moveTo:(SBObject *)to;  // Move object(s) to a new location.
- (void) saveIn:(NSURL *)in_ as:(NSString *)as;  // Save an object.

@end



/*
 * Standard Suite
 */

// A scriptable object.
@interface SwinsianItem : SBObject <SwinsianGenericMethods>

@property (copy) NSDictionary *properties;  // All of the object's properties.


@end

// A color.
@interface SwinsianColor : SBObject <SwinsianGenericMethods>


@end

// A window.
@interface SwinsianWindow : SBObject <SwinsianGenericMethods>

@property (copy) NSString *name;  // The full title of the window.
- (NSNumber *) id;  // The unique identifier of the window.
@property NSRect bounds;  // The bounding rectangle of the window.
@property (readonly) BOOL closeable;  // Whether the window has a close box.
@property (readonly) BOOL titled;  // Whether the window has a title bar.
@property (copy) NSNumber *index;  // The index of the window in the back-to-front window ordering.
@property (readonly) BOOL floating;  // Whether the window floats.
@property (readonly) BOOL miniaturizable;  // Whether the window can be miniaturized.
@property BOOL miniaturized;  // Whether the window is currently miniaturized.
@property (readonly) BOOL modal;  // Whether the window is the application's current modal window.
@property (readonly) BOOL resizable;  // Whether the window can be resized.
@property BOOL visible;  // Whether the window is currently visible.
@property (readonly) BOOL zoomable;  // Whether the window can be zoomed.
@property BOOL zoomed;  // Whether the window is currently zoomed.
@property (copy, readonly) NSArray<SwinsianTrack *> *selection;  // Currently seleted tracks


@end



/*
 * Swinsian Suite
 */

// The application
@interface SwinsianApplication : SBApplication

- (SBElementArray<SwinsianWindow *> *) windows;
- (SBElementArray<SwinsianPlaylist *> *) playlists;
- (SBElementArray<SwinsianSmartPlaylist *> *) smartPlaylists;
- (SBElementArray<SwinsianNormalPlaylist *> *) normalPlaylists;
- (SBElementArray<SwinsianLibrary *> *) libraries;
- (SBElementArray<SwinsianTrack *> *) tracks;
- (SBElementArray<SwinsianAudioDevice *> *) audioDevices;

@property (copy, readonly) NSString *name;  // The name of the application.
@property (readonly) BOOL frontmost;  // Is this the frontmost (active) application?
@property (copy, readonly) NSString *version;  // The version of the application.
@property NSInteger playerPosition;  // the player’s position within the currently playing track in seconds.
@property (copy, readonly) SwinsianTrack *currentTrack;  // the currently playing track
@property (copy) NSNumber *soundVolume;  // the volume. (0 minimum, 100 maximum)
@property (readonly) SwinsianPlayerState playerState;  // are we stopped, paused or still playing?
@property (copy, readonly) SwinsianQueue *playbackQueue;  // the currently queued tracks
@property (copy) SwinsianAudioDevice *outputDevice;  // current audio output device

- (void) open:(NSURL *)x;  // Open an object.
- (void) print:(NSURL *)x;  // Print an object.
- (void) quitSaving:(SwinsianSaveOptions)saving;  // Quit an application.
- (void) play;  // begin playing the current playlist
- (void) pause;  // pause playback
- (void) nextTrack;  // skip to the next track in the current playlist
- (void) stop;  // stop playback
- (NSArray<SwinsianTrack *> *) searchPlaylist:(SwinsianPlaylist *)playlist for:(NSString *)for_;  // search a playlist for tracks matching a string
- (void) previousTrack;  // skip back to the previous track
- (void) playpause;  // toggle play/pause
- (void) addTracks:(NSArray<SwinsianTrack *> *)tracks to:(SwinsianNormalPlaylist *)to;  // add a track to a playlist
- (void) notify;  // show currently playing track notification
- (void) rescanTags:(NSArray<SwinsianTrack *> *)x;  // rescan tags on tracks
- (NSArray<SwinsianTrack *> *) findTrack:(NSString *)x;  // Finds tracks for the given path
- (void) removeTracks:(NSArray<SwinsianTrack *> *)tracks from:(SwinsianNormalPlaylist *)from;  // remove tracks from a playlist

@end

// generic playlist type, subcasses include smart playlist and normal playlist
@interface SwinsianPlaylist : SwinsianItem

- (SBElementArray<SwinsianTrack *> *) tracks;

@property (copy) NSString *name;  // the name of the playlist
@property (readonly) BOOL smart;  // is this a smart playlist


@end

@interface SwinsianLibrary : SwinsianItem

- (SBElementArray<SwinsianTrack *> *) tracks;



@end

// a music track
@interface SwinsianTrack : SwinsianItem

@property (copy) NSString *album;  // the album of the track
@property (copy) NSString *artist;  // the artist
@property (copy) NSString *composer;  // the composer
@property (copy) NSString *genre;  // the genre
@property (copy, readonly) NSString *time;  // the length of the track in text format as MM:SS
@property NSInteger year;  // the year the track was recorded
@property (copy, readonly) NSDate *dateAdded;  // the date the track was added to the library
@property (readonly) double duration;  // the length of the track in seconds
@property (copy, readonly) NSString *location;  // location on disk
@property (readonly) BOOL iPodTrack;  // TRUE if the track is on an iPod
@property (copy) NSString *name;  // the title of the track (same as title)
@property (readonly) NSInteger bitRate;  // the bitrate of the track
@property (copy, readonly) NSString *kind;  // a text description of the type of file the track is
@property (copy) NSNumber *rating;  // Track rating. 0-5
@property NSInteger trackNumber;  // the Track number
@property (readonly) NSInteger fileSize;  // file size in bytes
@property (copy, readonly) NSImage *albumArt;  // the album artwork
@property (copy, readonly) NSString *artFormat;  // the data format for this piece of artwork. text that will be "PNG" or "JPEG". getting the album art property first will mean this information has been retrieved already, otherwise the tags for the file will have to be re-read
@property (copy) NSNumber *discNumber;  // the disc number
@property (copy) NSNumber *discCount;  // the total number of discs in the album
- (NSString *) id;  // uuid
@property (copy) NSString *albumArtist;  // the album artist
@property (copy, readonly) NSString *albumArtistOrArtist;  // the album artist of the track, or is none is set, the artist
@property BOOL compilation;  // compilation flag
@property (copy) NSString *title;  // track title (the same as name)
@property (copy) NSString *comment;  // the comment
@property (copy, readonly) NSDate *dateCreated;  // the date created
@property (readonly) NSInteger channels;  // audio channel count
@property (readonly) NSInteger sampleRate;  // audio sample rate
@property (readonly) NSInteger bitDepth;  // the audio bit depth
@property (copy) NSDate *lastPlayed;  // date track was last played
@property (copy) NSString *lyrics;  // track lyrics
@property (copy, readonly) NSString *path;  // POSIX style path
@property (copy) NSString *grouping;  // grouping
@property (copy) NSString *publisher;  // the publisher
@property (copy) NSString *conductor;  // the conductor
@property (copy) NSString *objectDescription;  // the description
@property (copy, readonly) NSString *encoder;  // the encoder
@property (copy, readonly) NSString *copyright;  // the copyright
@property (copy) NSString *catalogNumber;  // the catalog number
@property (copy, readonly) NSDate *dateModified;  // the date modified
@property NSInteger playCount;  // the play count
@property (copy) NSNumber *trackCount;  // the total number of tracks in the album


@end

@interface SwinsianLibraryTrack : SwinsianTrack



@end

@interface SwinsianIPodTrack : SwinsianTrack

@property (copy, readonly) NSString *iPodName;  // the name of the iPod this track is on


@end

// The playback queue
@interface SwinsianQueue : SwinsianItem

- (SBElementArray<SwinsianTrack *> *) tracks;



@end

// a smart playlist
@interface SwinsianSmartPlaylist : SwinsianPlaylist


@end

// a normal, non-smart, playlist
@interface SwinsianNormalPlaylist : SwinsianPlaylist

- (SBElementArray<SwinsianTrack *> *) tracks;

- (NSString *) id;  // uuid


@end

// folder of playlists
@interface SwinsianPlaylistFolder : SwinsianPlaylist

- (SBElementArray<SwinsianPlaylist *> *) playlists;

- (NSString *) id;  // uuid


@end

// an audio output device
@interface SwinsianAudioDevice : SBObject <SwinsianGenericMethods>

@property (copy, readonly) NSString *name;  // device name
- (NSString *) id;  // uuid
- (void) setId: (NSString *) id;


@end


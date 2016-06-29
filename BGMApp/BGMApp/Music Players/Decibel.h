/*
 * Decibel.h
 *
 * Generated with
 * sdef /Applications/Decibel.app | sdp -fh --basename Decibel
 */

#import <AppKit/AppKit.h>
#import <ScriptingBridge/ScriptingBridge.h>


@class DecibelApplication, DecibelDocument, DecibelWindow, DecibelApplication, DecibelTrack;

enum DecibelSaveOptions {
	DecibelSaveOptionsYes = 'yes ' /* Save the file. */,
	DecibelSaveOptionsNo = 'no  ' /* Do not save the file. */,
	DecibelSaveOptionsAsk = 'ask ' /* Ask the user whether or not to save the file. */
};
typedef enum DecibelSaveOptions DecibelSaveOptions;

enum DecibelPrintingErrorHandling {
	DecibelPrintingErrorHandlingStandard = 'lwst' /* Standard PostScript error handling */,
	DecibelPrintingErrorHandlingDetailed = 'lwdt' /* print a detailed report of PostScript errors */
};
typedef enum DecibelPrintingErrorHandling DecibelPrintingErrorHandling;

enum DecibelShuffleMode {
	DecibelShuffleModeOff = 'off ' /* Off */,
	DecibelShuffleModeTrack = 'trck' /* Track */,
	DecibelShuffleModeAlbum = 'albm' /* Album */,
	DecibelShuffleModeArtist = 'arts' /* Artist */
};
typedef enum DecibelShuffleMode DecibelShuffleMode;

enum DecibelRepeatMode {
	DecibelRepeatModeOff = 'off ' /* Off */,
	DecibelRepeatModeTrack = 'trck' /* Track */,
	DecibelRepeatModeAlbum = 'albm' /* Album */,
	DecibelRepeatModeArtist = 'arts' /* Artist */,
	DecibelRepeatModeAll = 'all ' /* All */
};
typedef enum DecibelRepeatMode DecibelRepeatMode;

@protocol DecibelGenericMethods

- (void) closeSaving:(DecibelSaveOptions)saving savingIn:(NSURL *)savingIn;  // Close a document.
- (void) saveIn:(NSURL *)in_ as:(id)as;  // Save a document.
- (void) printWithProperties:(NSDictionary *)withProperties printDialog:(BOOL)printDialog;  // Print a document.
- (void) delete;  // Delete an object.
- (void) duplicateTo:(SBObject *)to withProperties:(NSDictionary *)withProperties;  // Copy an object.
- (void) moveTo:(SBObject *)to;  // Move an object to a new location.

@end



/*
 * Standard Suite
 */

// The application's top-level scripting object.
@interface DecibelApplication : SBApplication

- (SBElementArray<DecibelDocument *> *) documents;
- (SBElementArray<DecibelWindow *> *) windows;

@property (copy, readonly) NSString *name;  // The name of the application.
@property (readonly) BOOL frontmost;  // Is this the active application?
@property (copy, readonly) NSString *version;  // The version number of the application.

- (id) open:(id)x;  // Open a document.
- (void) print:(id)x withProperties:(NSDictionary *)withProperties printDialog:(BOOL)printDialog;  // Print a document.
- (void) quitSaving:(DecibelSaveOptions)saving;  // Quit the application.
- (BOOL) exists:(id)x;  // Verify that an object exists.
- (void) play;  // Begin audio playback
- (void) pause;  // Suspend audio playback
- (void) stop;  // Stop audio playback
- (void) playPause;  // Begin or suspend audio playback
- (void) seekForward;  // Seek forward three seconds
- (void) seekBackward;  // Seek backward three seconds
- (void) playSelection;  // Play the selected track, or the first track if more than one are selected
- (void) playPreviousTrack;  // Play the previous logical track in the playlist
- (void) playNextTrack;  // Play the next logical track in the playlist
- (void) addFile:(NSURL *)x;  // Add a file to the playlist
- (void) playFile:(NSURL *)x;  // Add a file to the playlist and play it
- (void) playTrackAtIndex:(NSInteger)x;  // Play a track in the playlist
- (void) increaseDeviceVolume;  // Increase the device volume
- (void) decreaseDeviceVolume;  // Decrease the device volume
- (void) increaseDigitalVolume;  // Increase the digital volume
- (void) decreaseDigitalVolume;  // Decrease the digital volume
- (void) clearPlaylist;  // Clear the playlist
- (void) scramblePlaylist;  // Scramble the playlist

@end

// A document.
@interface DecibelDocument : SBObject <DecibelGenericMethods>

@property (copy, readonly) NSString *name;  // Its name.
@property (readonly) BOOL modified;  // Has it been modified since the last save?
@property (copy, readonly) NSURL *file;  // Its location on disk, if it has one.


@end

// A window.
@interface DecibelWindow : SBObject <DecibelGenericMethods>

@property (copy, readonly) NSString *name;  // The title of the window.
- (NSInteger) id;  // The unique identifier of the window.
@property NSInteger index;  // The index of the window, ordered front to back.
@property NSRect bounds;  // The bounding rectangle of the window.
@property (readonly) BOOL closeable;  // Does the window have a close button?
@property (readonly) BOOL miniaturizable;  // Does the window have a minimize button?
@property BOOL miniaturized;  // Is the window minimized right now?
@property (readonly) BOOL resizable;  // Can the window be resized?
@property BOOL visible;  // Is the window visible right now?
@property (readonly) BOOL zoomable;  // Does the window have a zoom button?
@property BOOL zoomed;  // Is the window zoomed right now?
@property (copy, readonly) DecibelDocument *document;  // The document whose contents are displayed in the window.


@end



/*
 * Decibel Scripting Suite
 */

// The Decibel application class.
@interface DecibelApplication (DecibelScriptingSuite)

- (SBElementArray<DecibelTrack *> *) tracks;

@property (readonly) BOOL playing;  // Is the player currently playing?
@property (readonly) BOOL shuffling;  // Is the player currently shuffling?
@property (readonly) BOOL repeating;  // Is the player currently repeating?
@property (copy, readonly) DecibelTrack *nowPlaying;  // The track that is currently playing?
@property double deviceVolume;  // The current device volume
@property double digitalVolume;  // The current digital volume
@property double playbackPosition;  // The current playback position [0, 1]
@property double playbackTime;  // The current playback time in seconds
@property (readonly) BOOL canPlay;  // Is the player currently playing?
@property (readonly) BOOL canPlayPreviousTrack;  // Is the player currently playing?
@property (readonly) BOOL canPlayNextTrack;  // Is the player currently playing?
@property (readonly) BOOL canAdjustDeviceVolume;  // Can the device volume be adjusted?
@property DecibelShuffleMode shuffleMode;  // Player shuffle mode
@property DecibelRepeatMode repeatMode;  // Player repeat mode
@property (copy, readonly) SBObject *currentPlaylist;  // The current playlist

@end

// A track in the playlist
@interface DecibelTrack : SBObject <DecibelGenericMethods>

- (NSString *) id;  // The track's ID
@property (copy, readonly) NSURL *file;  // The track's location
@property (readonly) double duration;  // The track's duration in seconds
@property (readonly) double sampleRate;  // The track's sample rate in Hz
@property (readonly) NSInteger bitDepth;  // The bit depth
@property (readonly) NSInteger channels;  // The track's channels
@property (copy) NSString *title;  // The track's title
@property (copy) NSString *artist;  // The track's artist
@property (copy) NSString *albumTitle;  // The track's album title
@property (copy) NSString *albumArtist;  // The track's album artist
@property NSInteger trackNumber;  // The track's track number
@property NSInteger trackTotal;  // The total number of tracks on the album
@property NSInteger discNumber;  // The disc number containing the track
@property NSInteger discTotal;  // The total number of discs (for multidisc albums)
@property BOOL partOfACompilation;  // Is the track part of a compilation?
@property (copy) NSString *genre;  // The track's genre
@property (copy) NSString *composer;  // The track's composer
@property (copy) NSString *releaseDate;  // The track's release date
@property (copy) NSString *ISRC;  // The track's ISRC
@property (copy) id MCN;  // The track's MCN

- (void) playTrack;  // Play a track in the playlist

@end


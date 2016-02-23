/*
 * VLC.h
 *
 * Generated with
 * sdef /Applications/VLC.app | sdp -fh --basename VLC
 */

#import <AppKit/AppKit.h>
#import <ScriptingBridge/ScriptingBridge.h>


@class VLCItem, VLCApplication, VLCColor, VLCDocument, VLCWindow, VLCAttributeRun, VLCCharacter, VLCParagraph, VLCText, VLCAttachment, VLCWord, VLCPrintSettings;

enum VLCSavo {
	VLCSavoAsk = 'ask ' /* Ask the user whether or not to save the file. */,
	VLCSavoNo = 'no  ' /* Do not save the file. */,
	VLCSavoYes = 'yes ' /* Save the file. */
};
typedef enum VLCSavo VLCSavo;

enum VLCEnum {
	VLCEnumStandard = 'lwst' /* Standard PostScript error handling */,
	VLCEnumDetailed = 'lwdt' /* print a detailed report of PostScript errors */
};
typedef enum VLCEnum VLCEnum;

@protocol VLCGenericMethods

- (void) closeSaving:(VLCSavo)saving savingIn:(NSURL *)savingIn;  // Close an object.
- (void) delete;  // Delete an object.
- (void) duplicateTo:(SBObject *)to withProperties:(NSDictionary *)withProperties;  // Copy object(s) and put the copies at a new location.
- (BOOL) exists;  // Verify if an object exists.
- (void) moveTo:(SBObject *)to;  // Move object(s) to a new location.
- (void) saveAs:(NSString *)as in:(NSURL *)in_;  // Save an object.
- (void) fullscreen;  // Toggle between fullscreen and windowed mode.
- (void) GetURL;  // Get a URL
- (void) mute;  // Mute the audio
- (void) next;  // Go to the next item in the playlist or the next chapter in the DVD/VCD.
- (void) OpenURL;  // Open a URL
- (void) play;  // Start playing the current playlistitem or pause it when it is already playing.
- (void) previous;  // Go to the previous item in the playlist or the previous chapter in the DVD/VCD.
- (void) stepBackward;  // Step the current playlist item backward the specified step width (default is 2) (1=extraShort, 2=short, 3=medium, 4=long).
- (void) stepForward;  // Step the current playlist item forward the specified step width (default is 2) (1=extraShort, 2=short, 3=medium, 4=long).
- (void) stop;  // Stop playing the current playlistitem
- (void) volumeDown;  // Bring the volume down by one step. There are 32 steps from 0 to 400% volume.
- (void) volumeUp;  // Bring the volume up by one step. There are 32 steps from 0 to 400% volume.

@end



/*
 * Standard Suite
 */

// A scriptable object.
@interface VLCItem : SBObject <VLCGenericMethods>

@property (copy) NSDictionary *properties;  // All of the object's properties.


@end

// An application's top level scripting object.
@interface VLCApplication : SBApplication

- (SBElementArray<VLCDocument *> *) documents;
- (SBElementArray<VLCWindow *> *) windows;

@property (readonly) BOOL frontmost;  // Is this the frontmost (active) application?
@property (copy, readonly) NSString *name;  // The name of the application.
@property (copy, readonly) NSString *version;  // The version of the application.

- (VLCDocument *) open:(NSURL *)x;  // Open an object.
- (void) print:(NSURL *)x printDialog:(BOOL)printDialog withProperties:(VLCPrintSettings *)withProperties;  // Print an object.
- (void) quitSaving:(VLCSavo)saving;  // Quit an application.

@end

// A color.
@interface VLCColor : VLCItem


@end

// A document.
@interface VLCDocument : VLCItem

@property (readonly) BOOL modified;  // Has the document been modified since the last save?
@property (copy) NSString *name;  // The document's name.
@property (copy) NSString *path;  // The document's path.


@end

// A window.
@interface VLCWindow : VLCItem

@property NSRect bounds;  // The bounding rectangle of the window.
@property (readonly) BOOL closeable;  // Whether the window has a close box.
@property (copy, readonly) VLCDocument *document;  // The document whose contents are being displayed in the window.
@property (readonly) BOOL floating;  // Whether the window floats.
- (NSInteger) id;  // The unique identifier of the window.
@property NSInteger index;  // The index of the window, ordered front to back.
@property (readonly) BOOL miniaturizable;  // Whether the window can be miniaturized.
@property BOOL miniaturized;  // Whether the window is currently miniaturized.
@property (readonly) BOOL modal;  // Whether the window is the application's current modal window.
@property (copy) NSString *name;  // The full title of the window.
@property (readonly) BOOL resizable;  // Whether the window can be resized.
@property (readonly) BOOL titled;  // Whether the window has a title bar.
@property BOOL visible;  // Whether the window is currently visible.
@property (readonly) BOOL zoomable;  // Whether the window can be zoomed.
@property BOOL zoomed;  // Whether the window is currently zoomed.


@end



/*
 * Text Suite
 */

// This subdivides the text into chunks that all have the same attributes.
@interface VLCAttributeRun : VLCItem

- (SBElementArray<VLCAttachment *> *) attachments;
- (SBElementArray<VLCAttributeRun *> *) attributeRuns;
- (SBElementArray<VLCCharacter *> *) characters;
- (SBElementArray<VLCParagraph *> *) paragraphs;
- (SBElementArray<VLCWord *> *) words;

@property (copy) NSColor *color;  // The color of the first character.
@property (copy) NSString *font;  // The name of the font of the first character.
@property NSInteger size;  // The size in points of the first character.


@end

// This subdivides the text into characters.
@interface VLCCharacter : VLCItem

- (SBElementArray<VLCAttachment *> *) attachments;
- (SBElementArray<VLCAttributeRun *> *) attributeRuns;
- (SBElementArray<VLCCharacter *> *) characters;
- (SBElementArray<VLCParagraph *> *) paragraphs;
- (SBElementArray<VLCWord *> *) words;

@property (copy) NSColor *color;  // The color of the first character.
@property (copy) NSString *font;  // The name of the font of the first character.
@property NSInteger size;  // The size in points of the first character.


@end

// This subdivides the text into paragraphs.
@interface VLCParagraph : VLCItem

- (SBElementArray<VLCAttachment *> *) attachments;
- (SBElementArray<VLCAttributeRun *> *) attributeRuns;
- (SBElementArray<VLCCharacter *> *) characters;
- (SBElementArray<VLCParagraph *> *) paragraphs;
- (SBElementArray<VLCWord *> *) words;

@property (copy) NSColor *color;  // The color of the first character.
@property (copy) NSString *font;  // The name of the font of the first character.
@property NSInteger size;  // The size in points of the first character.


@end

// Rich (styled) text
@interface VLCText : VLCItem

- (SBElementArray<VLCAttachment *> *) attachments;
- (SBElementArray<VLCAttributeRun *> *) attributeRuns;
- (SBElementArray<VLCCharacter *> *) characters;
- (SBElementArray<VLCParagraph *> *) paragraphs;
- (SBElementArray<VLCWord *> *) words;

@property (copy) NSColor *color;  // The color of the first character.
@property (copy) NSString *font;  // The name of the font of the first character.
@property NSInteger size;  // The size in points of the first character.


@end

// Represents an inline text attachment.  This class is used mainly for make commands.
@interface VLCAttachment : VLCText

@property (copy) NSString *fileName;  // The path to the file for the attachment


@end

// This subdivides the text into words.
@interface VLCWord : VLCItem

- (SBElementArray<VLCAttachment *> *) attachments;
- (SBElementArray<VLCAttributeRun *> *) attributeRuns;
- (SBElementArray<VLCCharacter *> *) characters;
- (SBElementArray<VLCParagraph *> *) paragraphs;
- (SBElementArray<VLCWord *> *) words;

@property (copy) NSColor *color;  // The color of the first character.
@property (copy) NSString *font;  // The name of the font of the first character.
@property NSInteger size;  // The size in points of the first character.


@end



/*
 * VLC suite
 */

// VLC's top level scripting object
@interface VLCApplication (VLCSuite)

@property NSInteger audioVolume;  // The volume of the current playlist item from 0 to 4, where 4 is 400%
@property NSInteger currentTime;  // The current time of the current playlist item in seconds.
@property (readonly) NSInteger durationOfCurrentItem;  // The duration of the current playlist item in seconds.
@property BOOL fullscreenMode;  // indicates wheter fullscreen is enabled or not
@property (readonly) BOOL muted;  // Is VLC currently muted?
@property (copy, readonly) NSString *nameOfCurrentItem;  // Name of the current playlist item.
@property (copy, readonly) NSString *pathOfCurrentItem;  // Path to the current playlist item.
@property (readonly) BOOL playing;  // Is VLC playing an item?

@end



/*
 * Type Definitions
 */

@interface VLCPrintSettings : SBObject <VLCGenericMethods>

@property NSInteger copies;  // the number of copies of a document to be printed
@property BOOL collating;  // Should printed copies be collated?
@property NSInteger startingPage;  // the first page of the document to be printed
@property NSInteger endingPage;  // the last page of the document to be printed
@property NSInteger pagesAcross;  // number of logical pages laid across a physical page
@property NSInteger pagesDown;  // number of logical pages laid out down a physical page
@property (copy) NSDate *requestedPrintTime;  // the time at which the desktop printer should print the document
@property VLCEnum errorHandling;  // how errors are handled
@property (copy) NSString *faxNumber;  // for fax number
@property (copy) NSString *targetPrinter;  // for target printer


@end


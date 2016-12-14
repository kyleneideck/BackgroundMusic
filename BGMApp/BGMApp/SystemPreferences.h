/*
 * SystemPreferences.h
 */

#import <AppKit/AppKit.h>
#import <ScriptingBridge/ScriptingBridge.h>


@class SystemPreferencesItem, SystemPreferencesApplication, SystemPreferencesColor, SystemPreferencesDocument, SystemPreferencesWindow, SystemPreferencesAttributeRun, SystemPreferencesCharacter, SystemPreferencesParagraph, SystemPreferencesText, SystemPreferencesAttachment, SystemPreferencesWord, SystemPreferencesAnchor, SystemPreferencesPane, SystemPreferencesPrintSettings;

enum SystemPreferencesSavo {
	SystemPreferencesSavoAsk = 'ask ' /* Ask the user whether or not to save the file. */,
	SystemPreferencesSavoNo = 'no  ' /* Do not save the file. */,
	SystemPreferencesSavoYes = 'yes ' /* Save the file. */
};
typedef enum SystemPreferencesSavo SystemPreferencesSavo;

enum SystemPreferencesEnum {
	SystemPreferencesEnumStandard = 'lwst' /* Standard PostScript error handling */,
	SystemPreferencesEnumDetailed = 'lwdt' /* print a detailed report of PostScript errors */
};
typedef enum SystemPreferencesEnum SystemPreferencesEnum;



/*
 * Standard Suite
 */

// A scriptable object.
@interface SystemPreferencesItem : SBObject

@property (copy) NSDictionary *properties;  // All of the object's properties.

- (void)closeSaving:(SystemPreferencesSavo) saving savingIn:(NSURL *)savingIn;  // Close an object.
- (void)delete;   // Delete an object.
- (void)duplicateTo:(SBObject *)to withProperties:(NSDictionary *)withProperties;   // Copy object(s) and put the copies at a new location.
- (BOOL)exists;   // Verify if an object exists.
- (void)moveTo:(SBObject *)to;   // Move object(s) to a new location.
- (void)saveAs:(NSString *)as in:(NSURL *)in_;   // Save an object.

@end

// An application's top level scripting object.
@interface SystemPreferencesApplication : SBApplication

- (SBElementArray *)documents;
- (SBElementArray *)windows;

@property (readonly) BOOL frontmost;  // Is this the frontmost (active) application?
@property (copy, readonly) NSString *name;  // The name of the application.
@property (copy, readonly) NSString *version;  // The version of the application.

- (SystemPreferencesDocument *)open:(NSURL *)x;   // Open an object.
- (void)print:(NSURL *)x printDialog:(BOOL) printDialog withProperties:(SystemPreferencesPrintSettings *)withProperties;  // Print an object.
- (void)quitSaving:(SystemPreferencesSavo)saving;   // Quit an application.

@end

// A color.
@interface SystemPreferencesColor : SystemPreferencesItem


@end

// A document.
@interface SystemPreferencesDocument : SystemPreferencesItem

@property (readonly) BOOL modified;  // Has the document been modified since the last save?
@property (copy) NSString *name;  // The document's name.
@property (copy) NSString *path;  // The document's path.


@end

// A window.
@interface SystemPreferencesWindow : SystemPreferencesItem

@property NSRect bounds;  // The bounding rectangle of the window.
@property (readonly) BOOL closeable;  // Whether the window has a close box.
@property (copy, readonly) SystemPreferencesDocument *document;  // The document whose contents are being displayed in the window.
@property (readonly) BOOL floating;  // Whether the window floats.
- (NSInteger)id;   // The unique identifier of the window.
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
@interface SystemPreferencesAttributeRun : SystemPreferencesItem

- (SBElementArray *)attachments;
- (SBElementArray *)attributeRuns;
- (SBElementArray *)characters;
- (SBElementArray *)paragraphs;
- (SBElementArray *)words;

@property (copy) NSColor *color;  // The color of the first character.
@property (copy) NSString *font;  // The name of the font of the first character.
@property NSInteger size;  // The size in points of the first character.


@end

// This subdivides the text into characters.
@interface SystemPreferencesCharacter : SystemPreferencesItem

- (SBElementArray *)attachments;
- (SBElementArray *)attributeRuns;
- (SBElementArray *)characters;
- (SBElementArray *)paragraphs;
- (SBElementArray *)words;

@property (copy) NSColor *color;  // The color of the first character.
@property (copy) NSString *font;  // The name of the font of the first character.
@property NSInteger size;  // The size in points of the first character.


@end

// This subdivides the text into paragraphs.
@interface SystemPreferencesParagraph : SystemPreferencesItem

- (SBElementArray *)attachments;
- (SBElementArray *)attributeRuns;
- (SBElementArray *)characters;
- (SBElementArray *)paragraphs;
- (SBElementArray *)words;

@property (copy) NSColor *color;  // The color of the first character.
@property (copy) NSString *font;  // The name of the font of the first character.
@property NSInteger size;  // The size in points of the first character.


@end

// Rich (styled) text
@interface SystemPreferencesText : SystemPreferencesItem

- (SBElementArray *)attachments;
- (SBElementArray *)attributeRuns;
- (SBElementArray *)characters;
- (SBElementArray *)paragraphs;
- (SBElementArray *)words;

@property (copy) NSColor *color;  // The color of the first character.
@property (copy) NSString *font;  // The name of the font of the first character.
@property NSInteger size;  // The size in points of the first character.


@end

// Represents an inline text attachment.  This class is used mainly for make commands.
@interface SystemPreferencesAttachment : SystemPreferencesText

@property (copy) NSString *fileName;  // The path to the file for the attachment


@end

// This subdivides the text into words.
@interface SystemPreferencesWord : SystemPreferencesItem

- (SBElementArray *)attachments;
- (SBElementArray *)attributeRuns;
- (SBElementArray *)characters;
- (SBElementArray *)paragraphs;
- (SBElementArray *)words;

@property (copy) NSColor *color;  // The color of the first character.
@property (copy) NSString *font;  // The name of the font of the first character.
@property NSInteger size;  // The size in points of the first character.


@end



/*
 * System Preferences
 */

// an anchor within a preference pane
@interface SystemPreferencesAnchor : SystemPreferencesItem

@property (copy, readonly) NSString *name;  // name of the anchor within a preference pane

- (SystemPreferencesAnchor *)reveal;   // Reveals an anchor within a preference pane or preference pane itself

@end

// System Preferences top level scripting object
@interface SystemPreferencesApplication (SystemPreferences)

- (SBElementArray *)panes;

@property (copy) SystemPreferencesPane *currentPane;  // the currently selected pane
@property (copy, readonly) SystemPreferencesWindow *preferencesWindow;  // the main preferences window
@property BOOL showAll;  // Is SystemPrefs in show all view. (Setting to false will do nothing)

@end

// a preference pane
@interface SystemPreferencesPane : SystemPreferencesItem

- (SBElementArray *)anchors;

- (NSString *)id;   // locale independent name of the preference pane; can refer to a pane using the expression: pane id "<name>"
@property (copy, readonly) NSString *localizedName;  // localized name of the preference pane
@property (copy, readonly) NSString *name;  // name of the preference pane as it appears in the title bar; can refer to a pane using the expression: pane "<name>"

- (NSInteger)timedLoad;   // This command does xxxx.

@end



/*
 * Type Definitions
 */

@interface SystemPreferencesPrintSettings : SBObject

@property NSInteger copies;  // the number of copies of a document to be printed
@property BOOL collating;  // Should printed copies be collated?
@property NSInteger startingPage;  // the first page of the document to be printed
@property NSInteger endingPage;  // the last page of the document to be printed
@property NSInteger pagesAcross;  // number of logical pages laid across a physical page
@property NSInteger pagesDown;  // number of logical pages laid out down a physical page
@property (copy) NSDate *requestedPrintTime;  // the time at which the desktop printer should print the document
@property SystemPreferencesEnum errorHandling;  // how errors are handled
@property (copy) NSString *faxNumber;  // for fax number
@property (copy) NSString *targetPrinter;  // for target printer

- (void)closeSaving:(SystemPreferencesSavo) saving savingIn:(NSURL *)savingIn;  // Close an object.
- (void)delete;   // Delete an object.
- (void)duplicateTo:(SBObject *)to withProperties:(NSDictionary *)withProperties;   // Copy object(s) and put the copies at a new location.
- (BOOL)exists;   // Verify if an object exists.
- (void)moveTo:(SBObject *)to;   // Move object(s) to a new location.
- (void)saveAs:(NSString *)as in:(NSURL *)in_;   // Save an object.

@end


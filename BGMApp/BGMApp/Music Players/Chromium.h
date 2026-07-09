/*
 * Chromium.h
 *
 * Generated with
 * sdef "/Applications/Brave Browser.app" | sdp -fh --basename Chromium
 *
 * Chromium-based browsers (Brave, Chrome, Edge) all ship the same AppleScript
 * dictionary, so this single header drives all of them. The Scripting Bridge
 * classes bind at runtime to whichever browser bundle ID the SBApplication is
 * created for.
 */

#import <AppKit/AppKit.h>
#import <ScriptingBridge/ScriptingBridge.h>


@class ChromiumApplication, ChromiumWindow, ChromiumTab, ChromiumBookmarkFolder, ChromiumBookmarkItem;

@protocol ChromiumGenericMethods

- (void) saveIn:(NSURL *)in_ as:(NSString *)as;  // Save an object.
- (void) close;  // Close a window.
- (void) delete;  // Delete an object.
- (SBObject *) duplicateTo:(SBObject *)to withProperties:(NSDictionary *)withProperties;  // Copy object(s) and put the copies at a new location.
- (SBObject *) moveTo:(SBObject *)to;  // Move object(s) to a new location.
- (void) print;  // Print an object.
- (void) reload;  // Reload a tab.
- (void) goBack;  // Go Back (If Possible).
- (void) goForward;  // Go Forward (If Possible).
- (void) selectAll;  // Select all.
- (void) cutSelection;  // Cut selected text (If Possible).
// sdp emits "NS_RETURNS_NOT_RETAINED" here, but that attribute only applies to methods that return
// an Objective-C object, so it's been removed to keep the header warning-clean.
- (void) copySelection;  // Copy text.
- (void) pasteSelection;  // Paste text (If Possible).
- (void) undo;  // Undo the last change.
- (void) redo;  // Redo the last change.
- (void) stop;  // Stop the current tab from loading.
- (void) viewSource;  // View the HTML source of the tab.
- (id) executeJavascript:(NSString *)javascript;  // Execute a piece of javascript.

@end



/*
 * Standard Suite
 */

// The application's top-level scripting object.
@interface ChromiumApplication : SBApplication

- (SBElementArray<ChromiumWindow *> *) windows;

@property (copy, readonly) NSString *name;  // The name of the application.
@property (readonly) BOOL frontmost;  // Is this the frontmost (active) application?
@property (copy, readonly) NSString *version;  // The version of the application.

- (void) open:(NSArray<NSURL *> *)x;  // Open a document.
- (void) quit;  // Quit the application.
- (BOOL) exists:(id)x;  // Verify if an object exists.

@end

// A window.
@interface ChromiumWindow : SBObject <ChromiumGenericMethods>

- (SBElementArray<ChromiumTab *> *) tabs;

@property (copy) NSString *givenName;  // The given name of the window.
@property (copy, readonly) NSString *name;  // The full title of the window.
- (NSString *) id;  // The unique identifier of the window.
@property NSInteger index;  // The index of the window, ordered front to back.
@property NSRect bounds;  // The bounding rectangle of the window.
@property (readonly) BOOL closeable;  // Whether the window has a close box.
@property (readonly) BOOL minimizable;  // Whether the window can be minimized.
@property BOOL minimized;  // Whether the window is currently minimized.
@property (readonly) BOOL resizable;  // Whether the window can be resized.
@property BOOL visible;  // Whether the window is currently visible.
@property (readonly) BOOL zoomable;  // Whether the window can be zoomed.
@property BOOL zoomed;  // Whether the window is currently zoomed.
@property (copy, readonly) ChromiumTab *activeTab;  // Returns the currently selected tab
@property (copy) NSString *mode;  // Represents the mode of the window which can be 'normal' or 'incognito', can be set only once during creation of the window.
@property NSInteger activeTabIndex;  // The index of the active tab.


@end



/*
 * Chromium Suite
 */

// The application's top-level scripting object.
@interface ChromiumApplication (ChromiumSuite)

- (SBElementArray<ChromiumBookmarkFolder *> *) bookmarkFolders;

@property (copy, readonly) ChromiumBookmarkFolder *bookmarksBar;  // The bookmarks bar bookmark folder.
@property (copy, readonly) ChromiumBookmarkFolder *otherBookmarks;  // The other bookmarks bookmark folder.

@end

// A tab.
@interface ChromiumTab : SBObject <ChromiumGenericMethods>

- (NSString *) id;  // Unique ID of the tab.
@property (copy, readonly) NSString *title;  // The title of the tab.
@property (copy) NSString *URL;  // The url visible to the user.
@property (readonly) BOOL loading;  // Is loading?


@end

// A bookmarks folder that contains other bookmarks folder and bookmark items.
@interface ChromiumBookmarkFolder : SBObject <ChromiumGenericMethods>

- (SBElementArray<ChromiumBookmarkFolder *> *) bookmarkFolders;
- (SBElementArray<ChromiumBookmarkItem *> *) bookmarkItems;

- (NSString *) id;  // Unique ID of the bookmark folder.
@property (copy) NSString *title;  // The title of the folder.
@property (copy, readonly) NSNumber *index;  // Returns the index with respect to its parent bookmark folder.


@end

// An item consists of an URL and the title of a bookmark
@interface ChromiumBookmarkItem : SBObject <ChromiumGenericMethods>

- (NSString *) id;  // Unique ID of the bookmark item.
@property (copy) NSString *title;  // The title of the bookmark item.
@property (copy) NSString *URL;  // The URL of the bookmark.
@property (copy, readonly) NSNumber *index;  // Returns the index with respect to its parent bookmark folder.


@end


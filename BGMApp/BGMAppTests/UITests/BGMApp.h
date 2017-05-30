/*
 * BGMApp.h
 */

#import <AppKit/AppKit.h>
#import <ScriptingBridge/ScriptingBridge.h>


@class BGMAppOutputDevice, BGMAppApplication;



/*
 * Background Music
 */

// an output audio device
@interface BGMAppOutputDevice : SBObject

@property (copy, readonly) NSString *name;
@property BOOL selected;  // is this the device to be used for audio output?

@end

// The application program
@interface BGMAppApplication : SBApplication

- (SBElementArray<BGMAppOutputDevice *> *) outputDevices;

@end


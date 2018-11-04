/*
 * BGMApp.h
 *
 * Generated with
 * sdef "/Applications/Background Music.app" | sdp -fh --basename BGMApp
 */

#import <AppKit/AppKit.h>
#import <ScriptingBridge/ScriptingBridge.h>


@class BGMAppOutputDevice, BGMAppApplication;



/*
 * Background Music
 */

// A hardware device that can play audio
@interface BGMAppOutputDevice : SBObject

@property (copy, readonly) NSString *name;  // The name of the output device.
@property BOOL selected;  // Is this the device to be used for audio output?

@end

// The application program
@interface BGMAppApplication : SBApplication

- (SBElementArray<BGMAppOutputDevice *> *) outputDevices;

@property (copy) BGMAppOutputDevice *selectedOutputDevice;  // The device to be used for audio output

@end


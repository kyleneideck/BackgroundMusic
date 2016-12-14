// This file is part of Background Music.
//
// Background Music is free software: you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation, either version 2 of the
// License, or (at your option) any later version.
//
// Background Music is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Background Music. If not, see <http://www.gnu.org/licenses/>.

//
//  BGMOutputDevicePrefs.mm
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#import "BGMOutputDevicePrefs.h"

// Local Includes
#import "BGM_Utils.h"

// PublicUtility Includes
#include "CAHALAudioSystemObject.h"
#include "CAHALAudioDevice.h"
#include "CAAutoDisposer.h"


#pragma clang assume_nonnull begin

static NSInteger const kOutputDeviceMenuItemTag = 2;

@implementation BGMOutputDevicePrefs {
    BGMAudioDeviceManager* audioDevices;
    NSMutableArray<NSMenuItem*>* outputDeviceMenuItems;
}

- (id) initWithAudioDevices:(BGMAudioDeviceManager*)inAudioDevices {
    if ((self = [super init])) {
        audioDevices = inAudioDevices;
        outputDeviceMenuItems = [NSMutableArray new];
    }
    
    return self;
}

- (void) populatePreferencesMenu:(NSMenu*)prefsMenu {
    // Remove existing menu items
    for (NSMenuItem* item in outputDeviceMenuItems) {
        [prefsMenu removeItem:item];
    }
    [outputDeviceMenuItems removeAllObjects];
    
    // Insert menu items after the item for the "Output Device" heading
    const NSInteger menuItemsIdx = [prefsMenu indexOfItemWithTag:kOutputDeviceMenuItemTag] + 1;
    
    // Add a menu item for each output device
    CAHALAudioSystemObject audioSystem;
    UInt32 numDevices = audioSystem.GetNumberAudioDevices();
    if (numDevices > 0) {
        CAAutoArrayDelete<AudioObjectID> devices(numDevices);
        audioSystem.GetAudioDevices(numDevices, devices);
        
        for (UInt32 i = 0; i < numDevices; i++) {
            CAHALAudioDevice device(devices[i]);
            
            // TODO: Handle C++ exceptions. (And the ones above that we don't swallow.)
            BGMLogAndSwallowExceptions("BGMOutputDevicePrefs::populatePreferencesMenu", [&]() {
                BOOL hasOutputChannels = device.GetTotalNumberChannels(/* inIsInput = */ false) > 0;
                
                if (device.GetObjectID() != [audioDevices bgmDevice].GetObjectID() && hasOutputChannels) {
                    NSString* deviceName = CFBridgingRelease(device.CopyName());
                    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:deviceName
                                                                  action:@selector(outputDeviceWasChanged:)
                                                           keyEquivalent:@""];
                    
                    BOOL selected = [audioDevices isOutputDevice:device.GetObjectID()];
                    item.state = (selected ? NSOnState : NSOffState);
                    item.target = self;
                    item.indentationLevel = 1;
                    item.representedObject = [NSNumber numberWithUnsignedInt:device.GetObjectID()];
                    
                    [prefsMenu insertItem:item atIndex:menuItemsIdx];
                    
                    [outputDeviceMenuItems addObject:item];
                }
            });
        }
    }
}

- (void) outputDeviceWasChanged:(NSMenuItem*)menuItem {
    DebugMsg("BGMOutputDevicePrefs::outputDeviceWasChanged: '%s' menu item selected",
             [menuItem.title UTF8String]);
    
    // Make sure the menu item is actually for an output device.
    if (![outputDeviceMenuItems containsObject:menuItem]) {
        return;
    }
    
    // Change to the new output device.
    AudioDeviceID newDeviceID = [[menuItem representedObject] unsignedIntValue];
    NSError* __nullable error = [audioDevices setOutputDeviceWithID:newDeviceID revertOnFailure:YES];
    
    if (error) {
        // Couldn't change the output device, so show a warning. (No need to change the menu selection back
        // because it's repopulated every time it's opened.)
        NSLog(@"Failed to set output device: %@", menuItem);
        
        NSAlert* alert = [NSAlert new];
        NSString* deviceName = [menuItem title];
        
        alert.messageText =
            [NSString stringWithFormat:@"Failed to set %@ as the output device", deviceName];
        alert.informativeText = @"This is probably a bug. Feel free to report it.";
        
        [alert runModal];
    }
}

@end

#pragma clang assume_nonnull end


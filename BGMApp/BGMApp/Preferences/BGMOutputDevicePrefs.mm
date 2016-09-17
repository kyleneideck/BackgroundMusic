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

// PublicUtility Includes
#include "CAHALAudioSystemObject.h"
#include "CAHALAudioDevice.h"
#include "CAAutoDisposer.h"


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
            BOOL hasOutputChannels = device.GetTotalNumberChannels(/* inIsInput = */ false) > 0;
            
            if (device.GetObjectID() != [audioDevices bgmDevice].GetObjectID() && hasOutputChannels) {
                NSString* deviceName = CFBridgingRelease(device.CopyName());
                NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:deviceName
                                                              action:@selector(outputDeviceWasChanged:)
                                                       keyEquivalent:@""];
                
                BOOL isSelected = [audioDevices isOutputDevice:device.GetObjectID()];
                [item setState:(isSelected ? NSOnState : NSOffState)];
                [item setTarget:self];
                [item setIndentationLevel:1];
                [item setRepresentedObject:[NSNumber numberWithUnsignedInt:device.GetObjectID()]];
                
                [prefsMenu insertItem:item atIndex:menuItemsIdx];
                
                [outputDeviceMenuItems addObject:item];
            }
        }
    }
}

- (void) outputDeviceWasChanged:(NSMenuItem*)menuItem {
    DebugMsg("BGMOutputDevicePrefs::outputDeviceWasChanged: '%s' menu item selected", [menuItem.title UTF8String]);
    
    // Make sure the menu item is actually for an output device.
    if (![outputDeviceMenuItems containsObject:menuItem]) {
        return;
    }
    
    // Change to the new output device.
    BOOL success = [audioDevices setOutputDeviceWithID:[[menuItem representedObject] unsignedIntValue] revertOnFailure:YES];
    
    if (!success) {
        // Couldn't change the output device, so show a warning and change the menu selection back
        NSAlert* alert = [NSAlert new];
        NSString* deviceName = [menuItem title];
        [alert setMessageText:[NSString stringWithFormat:@"Failed to set %@ as the output device", deviceName]];
        [alert setInformativeText:@"This is probably a bug. Feel free to report it."];
        [alert runModal];
        
        [menuItem setState:NSOffState];
        
        // TODO: Reselect previous menu item
    }
}

@end


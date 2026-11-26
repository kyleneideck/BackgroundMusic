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
//  BGMOutputDeviceMenuSection.mm
//  BGMApp
//
//  Copyright © 2016-2018, 2026 Kyle Neideck
//

// Self Include
#import "BGMOutputDeviceMenuSection.h"

// Local Includes
#import "BGM_Utils.h"
#import "BGM_Types.h"
#import "BGMAudioDevice.h"

// PublicUtility Includes
#import "CAAutoDisposer.h"
#import "CAHALAudioDevice.h"
#import "CAHALAudioSystemObject.h"
#import "CAPropertyAddress.h"

// STL Includes
#import <set>
#import <vector>


#pragma clang assume_nonnull begin

static NSInteger const kOutputDeviceMenuItemTag = 5;

// The size of the custom view in each output device's menu item. (The width matches the master
// volume row's view, which comes from MainMenu.xib.)
static const CGFloat kRowWidth  = 269.0;
static const CGFloat kRowHeight = 24.0;

@implementation BGMOutputDeviceMenuSection {
    NSMenu* bgmMenu;
    BGMAudioDeviceManager* audioDevices;
    BGMPreferredOutputDevices* preferredDevices;
    NSMutableArray<NSMenuItem*>* outputDeviceMenuItems;
    // Called when a CoreAudio property has changed and we might need to update the menu. For
    // example, when a device is connected or disconnected.
    AudioObjectPropertyListenerBlock refreshNeededListener;
    // The devices we've added refreshNeededListener to. Used to avoid adding it to a device twice
    // for the same property and to remove it from all devices in dealloc.
    std::set<BGMAudioDevice> listenedDevices_kAudioDevicePropertyDataSources;
    std::set<BGMAudioDevice> listenedDevices_kAudioDevicePropertyDataSource;
}

- (instancetype) initWithBGMMenu:(NSMenu*)inBGMMenu
                    audioDevices:(BGMAudioDeviceManager*)inAudioDevices
                preferredDevices:(BGMPreferredOutputDevices*)inPreferredDevices {
    if ((self = [super init])) {
        bgmMenu = inBGMMenu;
        audioDevices = inAudioDevices;
        preferredDevices = inPreferredDevices;
        outputDeviceMenuItems = [NSMutableArray new];

        [self listenForDevicesAddedOrRemoved];
        [self populateBGMMenu];
    }
    
    return self;
}

- (void) dealloc {
    // Tell CoreAudio not to call the listener block anymore. This probably isn't necessary.
    //
    // I think it's safe to do this without dispatching to the main queue because dealloc and
    // refreshNeededListener should be essentially mutually exclusive. If refreshNeededListener is
    // invoked and gets a value for weakSelf, it holds the strong ref until it returns, so dealloc
    // won't be called. If refreshNeededListener is invoked and deallocation has started, it will
    // get nil for weakSelf and just return.
    auto removeListener = [&] (CAHALAudioObject audioObject, AudioObjectPropertySelector prop) {
        BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
            // Check the object still exists first to reduce unnecessary error logs.
            if (CAHALAudioObject::ObjectExists(audioObject.GetObjectID())) {
                audioObject.RemovePropertyListenerBlock(CAPropertyAddress(prop),
                                                        dispatch_get_main_queue(),
                                                        refreshNeededListener);
            }
        });
    };

    // Remove the listener from each audio object we added it to.
    removeListener(CAHALAudioSystemObject(), kAudioHardwarePropertyDevices);

    for (auto device : listenedDevices_kAudioDevicePropertyDataSources) {
        removeListener(device, kAudioDevicePropertyDataSources);
    }

    for (auto device : listenedDevices_kAudioDevicePropertyDataSource) {
        removeListener(device, kAudioDevicePropertyDataSource);
    }
}

- (void) listenForDevicesAddedOrRemoved {
    // Create the block that will run when a device is added or removed.
    BGMOutputDeviceMenuSection* __weak weakSelf = self;

    refreshNeededListener = ^(UInt32 inNumberAddresses,
                              const AudioObjectPropertyAddress* inAddresses) {
        #pragma unused (inNumberAddresses, inAddresses)

        BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
            [weakSelf populateBGMMenu];
        });
    };

    // Register the listener block to be called when devices are connected or disconnected.
    BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
        CAHALAudioSystemObject().AddPropertyListenerBlock(
            CAPropertyAddress(kAudioHardwarePropertyDevices),
            dispatch_get_main_queue(),
            refreshNeededListener);
    });
}

- (void) populateBGMMenu {
    // TODO: Technically, we should assert we're on the main queue rather than just the main thread.
    BGMAssert([NSThread isMainThread],
              "BGMOutputDeviceMenuSection::populateBGMMenu called on non-main thread");

    // Remove existing menu items
    for (NSMenuItem* item in outputDeviceMenuItems) {
        DebugMsg("BGMOutputDeviceMenuSection::populateBGMMenu: Removing %s",
                 item.description.UTF8String);
        [bgmMenu removeItem:item];
    }
    
    [outputDeviceMenuItems removeAllObjects];
    
    // Add a menu item for each output device
    CAHALAudioSystemObject audioSystem;
    UInt32 numDevices = audioSystem.GetNumberAudioDevices();
    
    if (numDevices > 0) {
        CAAutoArrayDelete<AudioObjectID> devices(numDevices);
        audioSystem.GetAudioDevices(numDevices, devices);
        
        for (UInt32 i = 0; i < numDevices; i++) {
            [self insertMenuItemsForDevice:devices[i]];
        }
    }
}

- (void) insertMenuItemsForDevice:(BGMAudioDevice)device {
    // Insert menu items after the item for the "Output Device" heading.
    const NSInteger menuItemsIdx = [bgmMenu indexOfItemWithTag:kOutputDeviceMenuItemTag] + 1;

    BOOL canBeOutputDevice = YES;
    BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
        canBeOutputDevice = device.CanBeOutputDeviceInBGMApp();
    });

    if (canBeOutputDevice) {
        for (NSMenuItem* item : [self createMenuItemsForDevice:device]) {
            DebugMsg("BGMOutputDeviceMenuSection::insertMenuItemsForDevice: Inserting %s",
                     item.description.UTF8String);
            [bgmMenu insertItem:item atIndex:menuItemsIdx];
            [outputDeviceMenuItems addObject:item];
        }

        // Add listeners to update the menu when the device's data source changes or it changes its
        // list of data sources. We do this so that, for example, when you plug headphones into the
        // built-in jack, the menu item for the built-in device will change from "Internal Speakers"
        // to "Headphones".
        if (listenedDevices_kAudioDevicePropertyDataSources.count(device) == 0) {
            BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
                device.AddPropertyListenerBlock(CAPropertyAddress(kAudioDevicePropertyDataSources,
                                                                  kAudioDevicePropertyScopeOutput),
                                                dispatch_get_main_queue(),
                                                refreshNeededListener);
                listenedDevices_kAudioDevicePropertyDataSources.insert(device);
            });
        };

        if (listenedDevices_kAudioDevicePropertyDataSource.count(device) == 0) {
            BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
                device.AddPropertyListenerBlock(CAPropertyAddress(kAudioDevicePropertyDataSource,
                                                                  kAudioDevicePropertyScopeOutput),
                                                dispatch_get_main_queue(),
                                                refreshNeededListener);
                listenedDevices_kAudioDevicePropertyDataSource.insert(device);
            });
        };
    }
}

- (NSArray<NSMenuItem*>*) createMenuItemsForDevice:(CAHALAudioDevice)device {
    // We fill this array with a menu item for each output device (or each data source for each device) on
    // the system.
    NSMutableArray<NSMenuItem*>* items = [NSMutableArray new];

    AudioObjectPropertyScope scope = kAudioObjectPropertyScopeOutput;
    UInt32 channel = kAudioObjectPropertyElementMain;
    
    // If the device has data sources, create a menu item for each. Otherwise, create a single menu item
    // for the device. This way the menu items' titles will be, for example, "Internal Speakers" rather
    // than "Built-in Output".
    //
    // TODO: Handle data destinations as well? I don't have (or know of) any hardware with them.
    // TODO: Use the current data source's name when the control isn't settable, but only add one menu item.
    UInt32 numDataSources = 0;

    BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
        if (device.HasDataSourceControl(scope, channel) &&
                device.DataSourceControlIsSettable(scope, channel)) {
            numDataSources = device.GetNumberAvailableDataSources(scope, channel);
        }
    });
    
    if (numDataSources > 0) {
        std::vector<UInt32> dataSourceIDs(numDataSources);
        // This call updates numDataSources to the real number of IDs it added to our array.
        device.GetAvailableDataSources(scope, channel, numDataSources, dataSourceIDs.data());
        
        for (UInt32 i = 0; i < numDataSources; i++) {
            DebugMsg("BGMOutputDeviceMenuSection::createMenuItemsForDevice: "
                     "Creating item. %s%u %s%u",
                     "Device ID:", device.GetObjectID(),
                     ", Data source ID:", dataSourceIDs[i]);

            BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, "(DS)", [&] {
                NSString* dataSourceName =
                    CFBridgingRelease(device.CopyDataSourceNameForID(scope, channel, dataSourceIDs[i]));
                NSString* deviceName = CFBridgingRelease(device.CopyName());
                
                [items addObject:[self createRowMenuItemForDevice:device
                                                    dataSourceID:@(dataSourceIDs[i])
                                                           title:dataSourceName
                                                         toolTip:deviceName]];
            });
        }
    } else {
        DebugMsg("BGMOutputDeviceMenuSection::createMenuItemsForDevice: Creating item. %s%u",
                 "Device ID:", device.GetObjectID());

        BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
            [items addObject:[self createRowMenuItemForDevice:device
                                                  dataSourceID:nil
                                                         title:CFBridgingRelease(device.CopyName())
                                                       toolTip:nil]];
        });
    }
    
    return items;
}

// Creates a menu item with a custom view containing a check box (to enable/disable the device as
// an output) and a slider (to set the device's volume relative to the other output devices).
- (NSMenuItem*) createRowMenuItemForDevice:(CAHALAudioDevice)device
                              dataSourceID:(NSNumber* __nullable)dataSourceID
                                     title:(NSString* __nullable)title
                                   toolTip:(NSString* __nullable)toolTip {
    // If we don't have a title, use the tool-tip text instead.
    if (!title) {
        title = (toolTip ? toolTip : @"");
    }
    
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    
    NSView* rowView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kRowWidth, kRowHeight)];
    
    CGFloat checkboxX = 0;
    
    // Add the AirPlay icon before the check box for AirPlay devices.
    //
    // TODO: Test this with real hardware that supports AirPlay. (I don't have any.)
    BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
        if (device.GetTransportType() == kAudioDeviceTransportTypeAirPlay) {
            NSImageView* iconView = [[NSImageView alloc] initWithFrame:NSMakeRect(4, 2, 18, 18)];
            iconView.image = [NSImage imageNamed:@"AirPlayIcon"];
            
            // Make the icon a "template image" so it gets drawn colour-inverted when it's
            // highlighted or OS X is in dark mode.
            [iconView.image setTemplate:YES];
            
            [rowView addSubview:iconView];
            
            checkboxX = 26;
        }
    });
    
    // The check box (with the device or data source's name) enables/disables the device as an
    // output device.
    NSButton* checkbox = [[NSButton alloc]
                          initWithFrame:NSMakeRect(checkboxX, 2, 148 - checkboxX, 20)];
    [checkbox setButtonType:NSButtonTypeSwitch];
    [checkbox setTitle:BGMNN(title)];
    [checkbox setTarget:self];
    [checkbox setAction:@selector(outputDeviceCheckboxClicked:)];
    [checkbox setToolTip:toolTip];
    // Used to find the menu item in the action methods. (The same technique is used by
    // BGMAppVolumes.)
    checkbox.cell.representedObject = item;
    
    // The slider sets the device's volume, from 0 to 1. (The device's level is the master volume
    // times this gain.)
    NSSlider* slider = [[NSSlider alloc] initWithFrame:NSMakeRect(154, 2, 109, 20)];
    slider.minValue = 0.0;
    slider.maxValue = 1.0;
    slider.target = self;
    slider.action = @selector(outputDeviceSliderChanged:);
    slider.cell.representedObject = item;
    [slider setToolTip:@"Sets the volume of this output device."];
    
    [rowView addSubview:checkbox];
    [rowView addSubview:slider];
    
    item.view = rowView;
    item.toolTip = toolTip;
    item.representedObject = @{ @"deviceID": @(device.GetObjectID()),
                                @"dataSourceID": dataSourceID ? BGMNN(dataSourceID) : [NSNull null] };
    
#if __clang_major__ >= 9
    if (@available(macOS 10.10, *)) {
        // Used for UI tests.
        item.accessibilityIdentifier = @"output-device";
    }
#endif
    
    // Set the check box and slider to the current settings.
    [self updateRowItem:item];
    
    return item;
}

// Updates the check box and slider in a row's menu item to match the current settings for the
// row's device.
//
// If the device has data sources, only the row for the device's current data source is checked.
- (void) updateRowItem:(NSMenuItem*)item {
    AudioObjectID deviceID = [[item representedObject][@"deviceID"] unsignedIntValue];
    id dataSourceID = [item representedObject][@"dataSourceID"];

    BOOL deviceEnabled = [audioDevices isDeviceEnabled:deviceID];

    BOOL isChecked = NO;
    if (deviceEnabled) {
        if (dataSourceID == [NSNull null]) {
            isChecked = YES;
        } else {
            // The device has data sources. This row is checked only if it's for the device's
            // current data source.
            BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
                BGMAudioDevice dev(deviceID);
                AudioObjectPropertyScope scope = kAudioDevicePropertyScopeOutput;
                UInt32 channel = kAudioObjectPropertyElementMain;

                if (dev.HasDataSourceControl(scope, channel)) {
                    isChecked =
                        (dev.GetCurrentDataSourceID(scope, channel) == [dataSourceID unsignedIntValue]);
                } else {
                    isChecked = YES;
                }
            });
        }
    }

    float gain = [audioDevices gainForDevice:deviceID];

    for (NSView* subview in item.view.subviews) {
        if ([subview isKindOfClass:[NSButton class]]) {
            ((NSButton*)subview).state = (isChecked ? NSControlStateValueOn : NSControlStateValueOff);
        } else if ([subview isKindOfClass:[NSSlider class]]) {
            ((NSSlider*)subview).doubleValue = gain;
        }
    }
}

// Called when the user clicks a row's check box to enable or disable the device.
- (void) outputDeviceCheckboxClicked:(NSButton*)checkbox {
    NSMenuItem* menuItem = (NSMenuItem*)checkbox.cell.representedObject;

    // Make sure the menu item is actually for an output device.
    if (![outputDeviceMenuItems containsObject:menuItem]) {
        return;
    }

    AudioDeviceID deviceID = [[menuItem representedObject][@"deviceID"] unsignedIntValue];
    id dataSourceID = [menuItem representedObject][@"dataSourceID"];
    BOOL hasDataSource = (dataSourceID != [NSNull null]);

    BOOL isChecked = (checkbox.state == NSControlStateValueOn);
    BOOL deviceWasEnabled = [audioDevices isDeviceEnabled:deviceID];

    DebugMsg("BGMOutputDeviceMenuSection::outputDeviceCheckboxClicked: Device %u (%s data source) "
             "%schecked",
             deviceID,
             hasDataSource ? [dataSourceID stringValue].UTF8String : "no",
             isChecked ? "" : "un");

    // Dispatched because changing which devices are enabled usually blocks. (Note that we're using
    // DISPATCH_QUEUE_PRIORITY_HIGH, which is the second highest priority.)
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
        BOOL ok = YES;

        if (isChecked) {
            // Enable the device (if it isn't already).
            if (!deviceWasEnabled) {
                // All enabled output devices must share a sample rate, so check before enabling.
                NSError* __nullable conflict = [audioDevices sampleRateConflictErrorForDevice:deviceID];
                if (conflict) {
                    // The device's sample rate differs from the enabled output devices', so it
                    // would play the audio at the wrong speed. Revert the check box and explain.
                    dispatch_async(dispatch_get_main_queue(), ^{
                        checkbox.state = NSControlStateValueOff;

                        NSAlert* alert = [NSAlert new];
                        alert.messageText = @"Can't enable this output device.";
                        alert.informativeText = conflict.localizedDescription ?: @"";
                        [alert runModal];
                    });
                    return;
                }

                // Add the device to the list of preferred devices.
                [preferredDevices userChangedOutputDeviceTo:deviceID];

                ok = [audioDevices setDevice:deviceID enabled:YES];
            }

            // If this row has a data source, make it the device's current data source.
            if (ok && hasDataSource) {
                BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
                    BGMAudioDevice dev(deviceID);
                    AudioObjectPropertyScope scope = kAudioDevicePropertyScopeOutput;
                    UInt32 channel = kAudioObjectPropertyElementMain;

                    if (dev.DataSourceControlIsSettable(scope, channel)) {
                        dev.SetCurrentDataSourceByID(scope, channel, [dataSourceID unsignedIntValue]);
                    }
                });
            }
        } else if (deviceWasEnabled) {
            // Disable the device.
            ok = [audioDevices setDevice:deviceID enabled:NO];
        }

        if (!ok) {
            // Put the check box back the way it was and show an error message. (NSAlerts should
            // only be shown on the main thread.)
            dispatch_async(dispatch_get_main_queue(), ^{
                checkbox.state = (isChecked ? NSControlStateValueOff : NSControlStateValueOn);

                BOOL isLastEnabledDevice = ([audioDevices enabledOutputDeviceIDs].count <= 1);

                NSLog(@"Failed to %s output device %u", isChecked ? "enable" : "disable",
                      deviceID);

                NSAlert* alert = [NSAlert new];

                alert.messageText =
                    isLastEnabledDevice ?
                        @"Can't disable the last output device." :
                        @"Failed to change the output devices.";
                alert.informativeText =
                    isLastEnabledDevice ?
                        @"BGMApp needs at least one output device to play audio to." :
                        @"This is probably a bug. Feel free to report it.";

                [alert runModal];
            });
        }
    });
}

// Called when the user changes a row's volume slider.
- (void) outputDeviceSliderChanged:(NSSlider*)slider {
    NSMenuItem* menuItem = (NSMenuItem*)slider.cell.representedObject;

    // Make sure the menu item is actually for an output device.
    if (![outputDeviceMenuItems containsObject:menuItem]) {
        return;
    }

    AudioDeviceID deviceID = [[menuItem representedObject][@"deviceID"] unsignedIntValue];

    DebugMsg("BGMOutputDeviceMenuSection::outputDeviceSliderChanged: Device %u. New gain: %f",
             deviceID, slider.doubleValue);

    // Changing a device's gain is fast (it doesn't restart the IO), so no need to dispatch to
    // another thread.
    [audioDevices setDevice:deviceID gain:slider.floatValue];
}

// Called by BGMAudioDeviceManager to tell us the enabled output devices have changed.
- (void) outputDeviceDidChange {
    BGMOutputDeviceMenuSection* __weak weakSelf = self;

    dispatch_async(dispatch_get_main_queue(), ^{
        BGM_Utils::LogAndSwallowExceptions(BGMDbgArgs, [&] {
            [weakSelf populateBGMMenu];
        });
    });
}

@end

#pragma clang assume_nonnull end

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
//  BGMAppVolumes.m
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#import "BGMAppVolumes.h"

// BGM Includes
#include "BGM_Types.h"

// PublicUtility Includes
#include "CACFDictionary.h"
#include "CACFArray.h"
#include "CACFString.h"


static NSInteger const kAppVolumesMenuItemTag = 3;
static NSInteger const kSeparatorBelowAppVolumesMenuItemTag = 4;

static float const kSlidersSnapWithin = 5;

@implementation BGMAppVolumes {
    NSMenu* bgmMenu;
    NSView* appVolumeView;
    BGMAudioDeviceManager* audioDevices;
}

- (id) initWithMenu:(NSMenu*)menu appVolumeView:(NSView*)view audioDevices:(BGMAudioDeviceManager*)devices {
    if ((self = [super init])) {
        bgmMenu = menu;
        appVolumeView = view;
        audioDevices = devices;
        
        // Create the menu items for controlling app volumes
        [self insertMenuItemsForApps:[[NSWorkspace sharedWorkspace] runningApplications]];
        
        // Register for notifications when the user opens or closes apps, so we can update the menu
        [[NSWorkspace sharedWorkspace] addObserver:self
                                        forKeyPath:@"runningApplications"
                                           options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld
                                           context:nil];
    }
    
    return self;
}

- (void) dealloc {
    [[NSWorkspace sharedWorkspace] removeObserver:self forKeyPath:@"runningApplications" context:nil];
}

- (void) insertMenuItemsForApps:(NSArray<NSRunningApplication*>*)apps {
    NSAssert([NSThread isMainThread], @"insertMenuItemsForApps is not thread safe");
    
#ifndef NS_BLOCK_ASSERTIONS  // If assertions are enabled
    NSInteger numMenuItemsBeforeInsert =
        [bgmMenu indexOfItemWithTag:kSeparatorBelowAppVolumesMenuItemTag] - [bgmMenu indexOfItemWithTag:kAppVolumesMenuItemTag] - 1;
    NSUInteger numApps = 0;
#endif
    
    // Create a blank menu item to copy as a template
    NSMenuItem* blankItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [blankItem setView:appVolumeView];
    
    // Get the app volumes currently set on the device
    CACFArray appVolumesOnDevice((CFArrayRef)[audioDevices bgmDevice].GetPropertyData_CFType(kBGMAppVolumesAddress), false);
    
    NSInteger index = [bgmMenu indexOfItemWithTag:kAppVolumesMenuItemTag] + 1;
    
    // Add a volume-control menu item for each app
    for (NSRunningApplication* app in apps) {
        // Only show apps that appear in the dock (at first)
        // TODO: Would it be better to only show apps that are registered as HAL clients?
        if ([app activationPolicy] != NSApplicationActivationPolicyRegular) continue;
        
        // Don't show Finder
        if ([[app bundleIdentifier] isEqualTo:@"com.apple.finder"]) continue;
        
#ifndef NS_BLOCK_ASSERTIONS  // If assertions are enabled
        // Count how many apps we should add menu items for so we can check it at the end of the method
        numApps++;
#endif
        
        NSMenuItem* appVolItem = [blankItem copy];
        
        // Look through the menu item's subviews for the ones we want to set up
        for (NSView* subview in [[appVolItem view] subviews]) {
            if ([subview conformsToProtocol:@protocol(BGMAppVolumeSubview)]) {
                [subview performSelector:@selector(setUpWithApp:context:) withObject:app withObject:self];
            }
        }
        
        // Store the NSRunningApplication object with the menu item so when the app closes we can find the item to remove it
        [appVolItem setRepresentedObject:app];
        
        // Set the slider to the volume for this app if we got one from the driver
        [self setVolumeOfMenuItem:appVolItem fromAppVolumes:appVolumesOnDevice];
        
        [bgmMenu insertItem:appVolItem atIndex:index];
    }
    
#ifndef NS_BLOCK_ASSERTIONS  // If assertions are enabled
    NSInteger numMenuItemsAfterInsert =
        [bgmMenu indexOfItemWithTag:kSeparatorBelowAppVolumesMenuItemTag] - [bgmMenu indexOfItemWithTag:kAppVolumesMenuItemTag] - 1;
    NSAssert3(numMenuItemsAfterInsert == (numMenuItemsBeforeInsert + numApps),
              @"Did not add the expected number of menu items. numMenuItemsBeforeInsert=%ld numMenuItemsAfterInsert=%ld numAppsToAdd=%lu",
              (long)numMenuItemsBeforeInsert,
              (long)numMenuItemsAfterInsert,
              (unsigned long)numApps);
#endif
}

- (void) removeMenuItemsForApps:(NSArray<NSRunningApplication*>*)apps {
    NSAssert([NSThread isMainThread], @"removeMenuItemsForApps is not thread safe");
    
    NSInteger firstItemIndex = [bgmMenu indexOfItemWithTag:kAppVolumesMenuItemTag] + 1;
    NSInteger lastItemIndex = [bgmMenu indexOfItemWithTag:kSeparatorBelowAppVolumesMenuItemTag] - 1;
    
    // Check each app volume menu item, removing the items that control one of the given apps
    for (NSInteger i = firstItemIndex; i <= lastItemIndex; i++) {
        NSMenuItem* item = [bgmMenu itemAtIndex:i];
        
        for (NSRunningApplication* appToBeRemoved in apps) {
            NSRunningApplication* itemApp = [item representedObject];
            
            if ([itemApp isEqual:appToBeRemoved]) {
                [bgmMenu removeItem:item];
                // Correct i to account for the item we removed, since we're editing the menu in place
                i--;
                continue;
            }
        }
    }
}

- (void) setVolumeOfMenuItem:(NSMenuItem*)menuItem fromAppVolumes:(CACFArray&)appVolumes {
    // Set menuItem's volume slider to the volume of the app in appVolumes that menuItem represents
    // Leaves menuItem unchanged if it doesn't match any of the apps in appVolumes
    NSRunningApplication* representedApp = [menuItem representedObject];
    
    for (UInt32 i = 0; i < appVolumes.GetNumberItems(); i++) {
        CACFDictionary appVolume(false);
        appVolumes.GetCACFDictionary(i, appVolume);
        
        // Match the app to the menu item by pid or bundle id
        CACFString bundleID;
        bundleID.DontAllowRelease();
        appVolume.GetCACFString(CFSTR(kBGMAppVolumesKey_BundleID), bundleID);
        
        pid_t pid;
        appVolume.GetSInt32(CFSTR(kBGMAppVolumesKey_ProcessID), pid);
        
        if ([representedApp processIdentifier] == pid ||
            [[representedApp bundleIdentifier] isEqualToString:(__bridge NSString*)bundleID.GetCFString()]) {
            CFTypeRef relativeVolume;
            appVolume.GetCFType(CFSTR(kBGMAppVolumesKey_RelativeVolume), relativeVolume);
            
            // Update the slider
            for (NSView* subview in [[menuItem view] subviews]) {
                if ([subview respondsToSelector:@selector(setRelativeVolume:)]) {
                    [subview performSelector:@selector(setRelativeVolume:) withObject:(__bridge NSNumber*)relativeVolume];
                }
            }
        }
    }
}

- (void) observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    #pragma unused (object, context)
    
    // KVO callback for the apps currently running on the system. Adds/removes the associated menu items.
    if ([keyPath isEqualToString:@"runningApplications"]) {
        NSArray<NSRunningApplication*>* newApps = [change objectForKey:NSKeyValueChangeNewKey];
        NSArray<NSRunningApplication*>* oldApps = [change objectForKey:NSKeyValueChangeOldKey];
        
        int changeKind = [[change valueForKey:NSKeyValueChangeKindKey] intValue];
        switch (changeKind) {
            case NSKeyValueChangeInsertion:
                [self insertMenuItemsForApps:newApps];
                break;
                
            case NSKeyValueChangeRemoval:
                [self removeMenuItemsForApps:oldApps];
                break;
                
            case NSKeyValueChangeReplacement:
                [self removeMenuItemsForApps:oldApps];
                [self insertMenuItemsForApps:newApps];
                break;
                
            case NSKeyValueChangeSetting:
                [bgmMenu removeAllItems];
                [self insertMenuItemsForApps:newApps];
                break;
        }
    }
}

- (void) sendVolumeChangeToBGMDevice:(SInt32)newVolume appProcessID:(pid_t)appProcessID appBundleID:(NSString*)appBundleID {
    CACFDictionary appVolumeChange(true);
    appVolumeChange.AddSInt32(CFSTR(kBGMAppVolumesKey_ProcessID), appProcessID);
    appVolumeChange.AddString(CFSTR(kBGMAppVolumesKey_BundleID), (__bridge CFStringRef)appBundleID);
    // The values from our sliders are in [kAppRelativeVolumeMinRawValue, kAppRelativeVolumeMaxRawValue] already
    appVolumeChange.AddSInt32(CFSTR(kBGMAppVolumesKey_RelativeVolume), newVolume);
    
    CACFArray appVolumeChanges(true);
    appVolumeChanges.AppendDictionary(appVolumeChange.GetDict());

    [audioDevices bgmDevice].SetPropertyData_CFType(kBGMAppVolumesAddress, appVolumeChanges.AsPropertyList());
}

@end

// Custom classes for the UI elements in the app volume menu items

@implementation BGMAVM_AppIcon

- (void) setUpWithApp:(NSRunningApplication*)app context:(BGMAppVolumes*)ctx {
    #pragma unused (ctx)
    
    [self setImage:[app icon]];
}

@end

@implementation BGMAVM_AppNameLabel

- (void) setUpWithApp:(NSRunningApplication*)app context:(BGMAppVolumes*)ctx {
    #pragma unused (ctx)
    
    NSString* name = app.localizedName ? (NSString*)app.localizedName : @"";
    [self setStringValue:name];
}

@end

@implementation BGMAVM_VolumeSlider {
    // Will be set to -1 for apps without a pid
    pid_t appProcessID;
    NSString* appBundleID;
    BGMAppVolumes* context;
}

- (void) setUpWithApp:(NSRunningApplication*)app context:(BGMAppVolumes*)ctx {
    context = ctx;
    
    [self setTarget:self];
    [self setAction:@selector(appVolumeChanged)];
    
    appProcessID = [app processIdentifier];
    appBundleID = [app bundleIdentifier];
    
    [self setMaxValue:kAppRelativeVolumeMaxRawValue];
    [self setMinValue:kAppRelativeVolumeMinRawValue];
}

- (void) snap {
    // Snap to the 50% point
    float midPoint = static_cast<float>(([self maxValue] - [self minValue]) / 2);
    if ([self floatValue] > (midPoint - kSlidersSnapWithin) && [self floatValue] < (midPoint + kSlidersSnapWithin)) {
        [self setFloatValue:midPoint];
    }
}

- (void) setRelativeVolume:(NSNumber*)relativeVolume {
    [self setIntValue:[relativeVolume intValue]];
    [self snap];
}

- (void) appVolumeChanged {
    // TODO: This (sending updates to the driver) should probably be rate-limited. It uses a fair bit of CPU for me.
    
    DebugMsg("BGMAppVolumes::appVolumeChanged: App volume for %s changed to %d", [appBundleID UTF8String], [self intValue]);
    
    [self snap];
    
    [context sendVolumeChangeToBGMDevice:[self intValue] appProcessID:appProcessID appBundleID:appBundleID];
}

@end


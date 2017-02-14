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
//  Copyright © 2016, 2017 Kyle Neideck
//  Copyright © 2017 Andrew Tonner
//

// Self Include
#import "BGMAppVolumes.h"

// BGM Includes
#include "BGM_Types.h"
#include "BGM_Utils.h"

// PublicUtility Includes
#include "CACFDictionary.h"
#include "CACFArray.h"
#include "CACFString.h"


// Tags for UI elements in MainMenu.xib
static NSInteger const kAppVolumesHeadingMenuItemTag = 3;
static NSInteger const kSeparatorBelowAppVolumesMenuItemTag = 4;

static float const kSlidersSnapWithin = 5;

static CGFloat const kAppVolumeViewInitialHeight = 20;

@implementation BGMAppVolumes {
    NSMenu* bgmMenu;
    
    NSView* appVolumeView;
    CGFloat appVolumeViewFullHeight;
    
    BGMAudioDeviceManager* audioDevices;
}

- (id) initWithMenu:(NSMenu*)menu appVolumeView:(NSView*)view audioDevices:(BGMAudioDeviceManager*)devices {
    if ((self = [super init])) {
        bgmMenu = menu;
        appVolumeView = view;
        appVolumeViewFullHeight = appVolumeView.frame.size.height;
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

#pragma mark UI Modifications

- (void) insertMenuItemsForApps:(NSArray<NSRunningApplication*>*)apps {
    NSAssert([NSThread isMainThread], @"insertMenuItemsForApps is not thread safe");
    
#ifndef NS_BLOCK_ASSERTIONS  // If assertions are enabled
    auto numMenuItems = [&self]() {
        NSInteger headingIdx = [bgmMenu indexOfItemWithTag:kAppVolumesHeadingMenuItemTag];
        NSInteger separatorIdx = [bgmMenu indexOfItemWithTag:kSeparatorBelowAppVolumesMenuItemTag];
        return separatorIdx - headingIdx - 1;
    };
    
    NSInteger numMenuItemsBeforeInsert = numMenuItems();
    NSUInteger numApps = 0;
#endif
    
    // Get the app volumes currently set on the device
    CACFArray appVolumesOnDevice((CFArrayRef)[audioDevices bgmDevice].GetPropertyData_CFType(kBGMAppVolumesAddress), false);
    
    NSInteger index = [bgmMenu indexOfItemWithTag:kAppVolumesHeadingMenuItemTag] + 1;
    
    // Add a volume-control menu item for each app
    for (NSRunningApplication* app in apps) {
        // Only show apps that appear in the dock (at first)
        // TODO: Would it be better to only show apps that are registered as HAL clients?
        if (app.activationPolicy != NSApplicationActivationPolicyRegular) continue;
        
#ifndef NS_BLOCK_ASSERTIONS  // If assertions are enabled
        // Count how many apps we should add menu items for so we can check it at the end of the method
        numApps++;
#endif
        
        NSMenuItem* appVolItem = [self createBlankAppVolumeMenuItem];
        
        // Look through the menu item's subviews for the ones we want to set up
        for (NSView* subview in appVolItem.view.subviews) {
            if ([subview conformsToProtocol:@protocol(BGMAppVolumeMenuItemSubview)]) {
                [(NSView<BGMAppVolumeMenuItemSubview>*)subview setUpWithApp:app context:self menuItem:appVolItem];
            }
        }
        
        // Store the NSRunningApplication object with the menu item so when the app closes we can find the item to remove it
        appVolItem.representedObject = app;
        
        // Set the slider to the volume for this app if we got one from the driver
        [self setVolumeOfMenuItem:appVolItem fromAppVolumes:appVolumesOnDevice];
        
        [bgmMenu insertItem:appVolItem atIndex:index];
    }
    
    NSAssert3(numMenuItems() == (numMenuItemsBeforeInsert + numApps),
              @"Added more/fewer menu items than there were apps. Items before: %ld, items after: %ld, apps: %lu",
              (long)numMenuItemsBeforeInsert,
              (long)numMenuItems(),
              (unsigned long)numApps);
}

// Create a blank menu item to copy as a template.
- (NSMenuItem*) createBlankAppVolumeMenuItem {
    NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    
    menuItem.view = appVolumeView;
    menuItem = [menuItem copy];  // So we can modify a copy of the view, rather than the template itself.
    
    return menuItem;
}

- (void) removeMenuItemsForApps:(NSArray<NSRunningApplication*>*)apps {
    NSAssert([NSThread isMainThread], @"removeMenuItemsForApps is not thread safe");
    
    NSInteger firstItemIndex = [bgmMenu indexOfItemWithTag:kAppVolumesHeadingMenuItemTag] + 1;
    NSInteger lastItemIndex = [bgmMenu indexOfItemWithTag:kSeparatorBelowAppVolumesMenuItemTag] - 1;
    
    // Check each app volume menu item, removing the items that control one of the given apps
    for (NSInteger i = firstItemIndex; i <= lastItemIndex; i++) {
        NSMenuItem* item = [bgmMenu itemAtIndex:i];
        
        for (NSRunningApplication* appToBeRemoved in apps) {
            NSRunningApplication* itemApp = item.representedObject;
            
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
    NSRunningApplication* representedApp = menuItem.representedObject;
    
    for (UInt32 i = 0; i < appVolumes.GetNumberItems(); i++) {
        CACFDictionary appVolume(false);
        appVolumes.GetCACFDictionary(i, appVolume);
        
        // Match the app to the menu item by pid or bundle id
        CACFString bundleID;
        bundleID.DontAllowRelease();
        appVolume.GetCACFString(CFSTR(kBGMAppVolumesKey_BundleID), bundleID);
        
        pid_t pid;
        appVolume.GetSInt32(CFSTR(kBGMAppVolumesKey_ProcessID), pid);
        
        if ((representedApp.processIdentifier == pid) ||
            [representedApp.bundleIdentifier isEqualToString:(__bridge NSString*)bundleID.GetCFString()]) {
            CFTypeRef relativeVolume;
            appVolume.GetCFType(CFSTR(kBGMAppVolumesKey_RelativeVolume), relativeVolume);
            
            CFTypeRef panPosition;
            appVolume.GetCFType(CFSTR(kBGMAppVolumesKey_PanPosition), panPosition);
            
            // Update the slider
            for (NSView* subview in menuItem.view.subviews) {
                if ([subview respondsToSelector:@selector(setRelativeVolume:)]) {
                    [subview performSelector:@selector(setRelativeVolume:) withObject:(__bridge NSNumber*)relativeVolume];
                }
                if ([subview respondsToSelector:@selector(setPanPosition:)]) {
                    [subview performSelector:@selector(setPanPosition:) withObject:(__bridge NSNumber*)panPosition];
                }
            }
        }
    }
}

- (void) showHideExtraControls:(BGMAVM_ShowMoreControlsButton*)button {
    // Show or hide an app's extra controls, currently only pan, in its App Volumes menu item.
    
    NSMenuItem* menuItem = button.cell.representedObject;
    
    BGMAssert(button, "!button");
    BGMAssert(menuItem, "!menuItem");
    
    CGFloat width = menuItem.view.frame.size.width;
    CGFloat height = menuItem.view.frame.size.height;
    
#if DEBUG
    const char* appName = [((NSRunningApplication*)menuItem.representedObject).localizedName UTF8String];
#endif
    
    auto nearEnough = [](CGFloat x, CGFloat y) {  // Shouldn't be necessary, but just in case.
        return fabs(x - y) < 0.01;  // We don't need much precision.
    };
    
    if (nearEnough(button.frameCenterRotation, 0.0)) {
        // Hide extra controls
        DebugMsg("BGMAppVolumes::showHideExtraControls: Hiding extra controls (%s)", appName);
        
        BGMAssert(nearEnough(height, appVolumeViewFullHeight), "Extra controls were already hidden");
        
        // Make the menu item shorter to hide the extra controls. Keep the width unchanged.
        menuItem.view.frameSize = { width, kAppVolumeViewInitialHeight };
        // Turn the button upside down so the arrowhead points down.
        button.frameCenterRotation = 180.0;
        // Move the button up slightly so it aligns with the volume slider.
        [button setFrameOrigin:NSMakePoint(button.frame.origin.x, button.frame.origin.y - 1)];
    } else {
        // Show extra controls
        DebugMsg("BGMAppVolumes::showHideExtraControls: Showing extra controls (%s)", appName);
        
        BGMAssert(nearEnough(button.frameCenterRotation, 180.0), "Unexpected button rotation");
        BGMAssert(nearEnough(height, kAppVolumeViewInitialHeight), "Extra controls were already shown");
        
        // Make the menu item taller to show the extra controls. Keep the width unchanged.
        menuItem.view.frameSize = { width, appVolumeViewFullHeight };
        // Turn the button rightside up so the arrowhead points up.
        button.frameCenterRotation = 0.0;
        // Move the button down slightly, back to it's original position.
        [button setFrameOrigin:NSMakePoint(button.frame.origin.x, button.frame.origin.y + 1)];
    }
}

#pragma mark KVO

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

#pragma mark BGMDevice Communication

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

- (void) sendPanPositionChangeToBGMDevice:(SInt32)newPanPosition appProcessID:(pid_t)appProcessID appBundleID:(NSString*)appBundleID {
    CACFDictionary appVolumeChange(true);
    appVolumeChange.AddSInt32(CFSTR(kBGMAppVolumesKey_ProcessID), appProcessID);
    appVolumeChange.AddString(CFSTR(kBGMAppVolumesKey_BundleID), (__bridge CFStringRef)appBundleID);
    
    // The values from our sliders are in [kAppPanLeftRawValue, kAppPanRightRawValue] already
    appVolumeChange.AddSInt32(CFSTR(kBGMAppVolumesKey_PanPosition), newPanPosition);
    
    CACFArray appVolumeChanges(true);
    appVolumeChanges.AppendDictionary(appVolumeChange.GetDict());
    
    [audioDevices bgmDevice].SetPropertyData_CFType(kBGMAppVolumesAddress, appVolumeChanges.AsPropertyList());
}

@end

#pragma mark Custom Classes (IB)

// Custom classes for the UI elements in the app volume menu items

@implementation BGMAVM_AppIcon

- (void) setUpWithApp:(NSRunningApplication*)app context:(BGMAppVolumes*)ctx menuItem:(NSMenuItem*)menuItem {
    #pragma unused (ctx, menuItem)
    
    self.image = app.icon;
}

@end

@implementation BGMAVM_AppNameLabel

- (void) setUpWithApp:(NSRunningApplication*)app context:(BGMAppVolumes*)ctx menuItem:(NSMenuItem*)menuItem {
    #pragma unused (ctx, menuItem)
    
    NSString* name = app.localizedName ? (NSString*)app.localizedName : @"";
    self.stringValue = name;
}

@end

@implementation BGMAVM_ShowMoreControlsButton

- (void) setUpWithApp:(NSRunningApplication*)app context:(BGMAppVolumes*)ctx menuItem:(NSMenuItem*)menuItem {
    #pragma unused (app)
    
    // Set up the button that show/hide the extra controls (currently only a pan slider) for the app.
    self.cell.representedObject = menuItem;
    self.target = ctx;
    self.action = @selector(showHideExtraControls:);
    
    // The menu item starts out with the extra controls visible, so we hide them here.
    //
    // TODO: Leave them visible if any of the controls are set to non-default values. The user has no way to
    //       tell otherwise. Maybe we should also make this button look different if the controls are hidden
    //       when they have non-default values.
    [ctx showHideExtraControls:self];
}

@end

@implementation BGMAVM_VolumeSlider {
    // Will be set to -1 for apps without a pid
    pid_t appProcessID;
    NSString* appBundleID;
    BGMAppVolumes* context;
}

- (void) setUpWithApp:(NSRunningApplication*)app context:(BGMAppVolumes*)ctx menuItem:(NSMenuItem*)menuItem {
    #pragma unused (menuItem)
    
    context = ctx;
    
    self.target = self;
    self.action = @selector(appVolumeChanged);
    
    appProcessID = app.processIdentifier;
    appBundleID = app.bundleIdentifier;
    
    self.maxValue = kAppRelativeVolumeMaxRawValue;
    self.minValue = kAppRelativeVolumeMinRawValue;
}

// We have to handle snapping for volume sliders ourselves because adding a tick mark (snap point) in Interface Builder
// changes how the slider looks.
- (void) snap {
    // Snap to the 50% point.
    float midPoint = static_cast<float>((self.maxValue + self.minValue) / 2);
    if (self.floatValue > (midPoint - kSlidersSnapWithin) && self.floatValue < (midPoint + kSlidersSnapWithin)) {
        self.floatValue = midPoint;
    }
}

- (void) setRelativeVolume:(NSNumber*)relativeVolume {
    self.intValue = relativeVolume.intValue;
    [self snap];
}

- (void) appVolumeChanged {
    // TODO: This (sending updates to the driver) should probably be rate-limited. It uses a fair bit of CPU for me.
    
    DebugMsg("BGMAppVolumes::appVolumeChanged: App volume for %s changed to %d", appBundleID.UTF8String, self.intValue);
    
    [self snap];
    
    [context sendVolumeChangeToBGMDevice:self.intValue appProcessID:appProcessID appBundleID:appBundleID];
}

@end

@implementation BGMAVM_PanSlider {
    // Will be set to -1 for apps without a pid
    pid_t appProcessID;
    NSString* appBundleID;
    BGMAppVolumes* context;
}

- (void) setUpWithApp:(NSRunningApplication*)app context:(BGMAppVolumes*)ctx menuItem:(NSMenuItem*)menuItem {
    #pragma unused (menuItem)
    
    context = ctx;
    
    self.target = self;
    self.action = @selector(appPanPositionChanged);
    
    appProcessID = app.processIdentifier;
    appBundleID = app.bundleIdentifier;
    
    self.minValue = kAppPanLeftRawValue;
    self.maxValue = kAppPanRightRawValue;
}

- (void) setPanPosition:(NSNumber *)panPosition {
    self.intValue = panPosition.intValue;
}

- (void) appPanPositionChanged {
    // TODO: This (sending updates to the driver) should probably be rate-limited. It uses a fair bit of CPU for me.
    
    DebugMsg("BGMAppVolumes::appPanPositionChanged: App pan position for %s changed to %d", appBundleID.UTF8String, self.intValue);
    
    [context sendPanPositionChangeToBGMDevice:self.intValue appProcessID:appProcessID appBundleID:appBundleID];
}

@end


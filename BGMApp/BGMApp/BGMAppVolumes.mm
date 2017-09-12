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

// Local Includes
#import "BGM_Types.h"
#import "BGM_Utils.h"
#import "BGMAppDelegate.h"

// PublicUtility Includes
#import "CACFDictionary.h"
#import "CACFArray.h"
#import "CACFString.h"


static float const kSlidersSnapWithin = 5;

static CGFloat const kAppVolumeViewInitialHeight = 20;

@implementation BGMAppVolumes {
    NSMenu* bgmMenu;
    
    NSView* appVolumeView;
    CGFloat appVolumeViewFullHeight;
    
    BGMAudioDeviceManager* audioDevices;

    NSInteger numMenuItems;
}

- (id) initWithMenu:(NSMenu*)menu appVolumeView:(NSView*)view audioDevices:(BGMAudioDeviceManager*)devices {
    if ((self = [super init])) {
        bgmMenu = menu;
        appVolumeView = view;
        appVolumeViewFullHeight = appVolumeView.frame.size.height;
        audioDevices = devices;
        numMenuItems = 0;
        
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

// This method allows the Interface Builder Custom Classes for controls (below) to send their values
// directly to BGMDevice. Not public to other classes.
- (BGMAudioDeviceManager*) audioDevices {
    return audioDevices;
}

#pragma mark UI Modifications

// Adds a volume control menu item for each given app.
- (void) insertMenuItemsForApps:(NSArray<NSRunningApplication*>*)apps {
    NSAssert([NSThread isMainThread], @"insertMenuItemsForApps is not thread safe");
    
    // Get the app volumes currently set on the device
    CACFArray appVolumesOnDevice([audioDevices bgmDevice].GetAppVolumes(), false);

    for (NSRunningApplication* app in apps) {
        // Only show apps that appear in the dock (at first)
        // TODO: Would it be better to only show apps that are registered as HAL clients?
        if (app.activationPolicy != NSApplicationActivationPolicyRegular) continue;
        
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

        // NSMenuItem didn't implement NSAccessibility before OS X SDK 10.12.
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101200  // MAC_OS_X_VERSION_10_12
        if ([appVolItem respondsToSelector:@selector(setAccessibilityTitle:)]) {
            // TODO: This doesn't show up in Accessibility Inspector for me. Not sure why.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
            appVolItem.accessibilityTitle = [NSString stringWithFormat:@"%@", [app localizedName]];
#pragma clang diagnostic pop
        }
#endif

        [bgmMenu insertItem:appVolItem atIndex:[self firstMenuItemIndex]];
        numMenuItems++;
    }
}

// Create a blank menu item to copy as a template.
- (NSMenuItem*) createBlankAppVolumeMenuItem {
    NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    
    menuItem.view = appVolumeView;
    menuItem = [menuItem copy];  // So we can modify a copy of the view, rather than the template itself.
    
    return menuItem;
}

- (NSInteger) firstMenuItemIndex {
    return [self lastMenuItemIndex] - numMenuItems + 1;
}

- (NSInteger) lastMenuItemIndex {
    return [bgmMenu indexOfItemWithTag:kSeparatorBelowVolumesMenuItemTag] - 1;
}

- (void) removeMenuItemsForApps:(NSArray<NSRunningApplication*>*)apps {
    NSAssert([NSThread isMainThread], @"removeMenuItemsForApps is not thread safe");
    
    // Check each app volume menu item, removing the items that control one of the given apps
    for (NSRunningApplication* appToBeRemoved in apps) {
        for (NSInteger i = [self firstMenuItemIndex]; i <= [self lastMenuItemIndex]; i++) {
            NSMenuItem* item = [bgmMenu itemAtIndex:i];
            NSRunningApplication* itemApp = item.representedObject;

            if ([itemApp isEqual:appToBeRemoved]) {
                [bgmMenu removeItem:item];
                numMenuItems--;
                break;
            }
        }
    }
}

- (void) setVolumeOfMenuItem:(NSMenuItem*)menuItem fromAppVolumes:(const CACFArray&)appVolumes {
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
        NSArray<NSRunningApplication*>* newApps = change[NSKeyValueChangeNewKey];
        NSArray<NSRunningApplication*>* oldApps = change[NSKeyValueChangeOldKey];
        
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

    if ([self respondsToSelector:@selector(setAccessibilityTitle:)]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
        self.accessibilityTitle = @"More options";
#pragma clang diagnostic pop
    }
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

    if ([self respondsToSelector:@selector(setAccessibilityTitle:)]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
        self.accessibilityTitle = [NSString stringWithFormat:@"Volume for %@", [app localizedName]];
#pragma clang diagnostic pop
    }
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

    // The values from our sliders are in
    // [kAppRelativeVolumeMinRawValue, kAppRelativeVolumeMaxRawValue] already.
    context.audioDevices.bgmDevice.SetAppVolume(self.intValue,
                                                appProcessID,
                                                (__bridge_retained CFStringRef)appBundleID);
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

    if ([self respondsToSelector:@selector(setAccessibilityTitle:)]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
        self.accessibilityTitle = [NSString stringWithFormat:@"Pan for %@", [app localizedName]];
#pragma clang diagnostic pop
    }
}

- (void) setPanPosition:(NSNumber *)panPosition {
    self.intValue = panPosition.intValue;
}

- (void) appPanPositionChanged {
    // TODO: This (sending updates to the driver) should probably be rate-limited. It uses a fair bit of CPU for me.
    
    DebugMsg("BGMAppVolumes::appPanPositionChanged: App pan position for %s changed to %d", appBundleID.UTF8String, self.intValue);

    // The values from our sliders are in [kAppPanLeftRawValue, kAppPanRightRawValue] already.
    context.audioDevices.bgmDevice.SetAppPanPosition(self.intValue,
                                                     appProcessID,
                                                     (__bridge_retained CFStringRef)appBundleID);
}

@end


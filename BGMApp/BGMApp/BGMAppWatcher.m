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
//  BGMAppWatcher.m
//  BGMApp
//
//  Copyright © 2019 Kyle Neideck
//

// Self Include
#import "BGMAppWatcher.h"

// System Includes
#import <Cocoa/Cocoa.h>


#pragma clang assume_nonnull begin

@implementation BGMAppWatcher {
    // Tokens for the notification observers so we can remove them in dealloc.
    id<NSObject> didLaunchToken;
    id<NSObject> didTerminateToken;
}

- (instancetype) initWithBundleID:(NSString*)bundleID
                      appLaunched:(void(^)(void))appLaunched
                    appTerminated:(void(^)(void))appTerminated {
    return [self initWithAppLaunched:appLaunched
                       appTerminated:appTerminated
                  isMatchingBundleID:^BOOL(NSString* appBundleID) {
                      return [bundleID isEqualToString:appBundleID];
                  }];
}

- (instancetype) initWithAppLaunched:(void(^)(void))appLaunched
                       appTerminated:(void(^)(void))appTerminated
                  isMatchingBundleID:(BOOL(^)(NSString*))isMatchingBundleID
{
    if ((self = [super init])) {
        NSNotificationCenter* center = [NSWorkspace sharedWorkspace].notificationCenter;

        didLaunchToken =
            [center addObserverForName:NSWorkspaceDidLaunchApplicationNotification
                                object:nil
                                 queue:nil
                            usingBlock:^(NSNotification* notification) {
                                if ([BGMAppWatcher shouldBeHandled:notification
                                                isMatchingBundleID:isMatchingBundleID]) {
                                    appLaunched();
                                }
                            }];

        didTerminateToken =
            [center addObserverForName:NSWorkspaceDidTerminateApplicationNotification
                                object:nil
                                 queue:nil
                            usingBlock:^(NSNotification* notification) {
                                if ([BGMAppWatcher shouldBeHandled:notification
                                                isMatchingBundleID:isMatchingBundleID]) {
                                    appTerminated();
                                }
                            }];
    }

    return self;
}

// Returns YES if we should call the app launch/termination callback for this NSNotification.
+ (BOOL) shouldBeHandled:(NSNotification*)notification
      isMatchingBundleID:(BOOL(^)(NSString*))isMatchingBundleID {
    NSString* __nullable notifiedBundleID =
        [notification.userInfo[NSWorkspaceApplicationKey] bundleIdentifier];

    // Ignore the notification if the app doesn't have a bundle ID or isMatchingBundleID returns NO.
    return notifiedBundleID && isMatchingBundleID((NSString*)notifiedBundleID);
}

- (void) dealloc {
    // Remove the application launch/termination observers.
    NSNotificationCenter* center = [NSWorkspace sharedWorkspace].notificationCenter;

    if (didLaunchToken) {
        [center removeObserver:didLaunchToken];
        didLaunchToken = nil;
    }

    if (didTerminateToken) {
        [center removeObserver:didTerminateToken];
        didTerminateToken = nil;
    }
}

@end

#pragma clang assume_nonnull end


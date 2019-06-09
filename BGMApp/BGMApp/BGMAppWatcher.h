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
//  BGMAppWatcher.h
//  BGMApp
//
//  Copyright © 2019 Kyle Neideck
//
//  Calls callback functions when a given application is launched or terminated. Starts watching
//  after being initialised, stops after being destroyed.
//

// System Includes
#import <Foundation/Foundation.h>


#pragma clang assume_nonnull begin

@interface BGMAppWatcher : NSObject

// appLaunched will be called when the application is launched and appTerminated will be called when
// it's terminated. Background apps, status bar apps, etc. are ignored.
- (instancetype) initWithBundleID:(NSString*)bundleID
                      appLaunched:(void(^)(void))appLaunched
                    appTerminated:(void(^)(void))appTerminated;

// With this constructor, when an application is launched or terminated, isMatchingBundleID will be
// called first to decide whether or not the callback should be called.
- (instancetype) initWithAppLaunched:(void(^)(void))appLaunched
                       appTerminated:(void(^)(void))appTerminated
                  isMatchingBundleID:(BOOL(^)(NSString* appBundleID))isMatchingBundleID;

@end

#pragma clang assume_nonnull end


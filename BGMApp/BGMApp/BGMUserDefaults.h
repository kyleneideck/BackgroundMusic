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
//  BGMUserDefaults.h
//  BGMApp
//
//  Copyright © 2016-2019 Kyle Neideck
//
//  A simple wrapper around our use of NSUserDefaults. Used to store the preferences/state that only
//  apply to BGMApp. The others are stored by BGMDriver.
//
//  Private data will be stored in the user's keychain instead of user defaults.
//

// Local Includes
#import "BGMStatusBarItem.h"

// System Includes
#import <Cocoa/Cocoa.h>


#pragma clang assume_nonnull begin

@interface BGMUserDefaults : NSObject

// If inDefaults is nil, settings are not loaded from or saved to disk, which is useful for testing.
- (instancetype) initWithDefaults:(NSUserDefaults* __nullable)inDefaults;

// The musicPlayerID (see BGMMusicPlayer.h), as a string, of the music player selected by the user.
// Must be either null or a string that can be parsed by NSUUID.
@property NSString* __nullable selectedMusicPlayerID;

@property BOOL autoPauseMusicEnabled;

// The UIDs of the output devices most recently selected by the user. The most-recently selected
// device is at index 0. See BGMPreferredOutputDevices.
@property NSArray<NSString*>* preferredDeviceUIDs;

// The (type of) icon to show in the button in the status bar. (The button the user clicks to open
// BGMApp's main menu.)
@property BGMStatusBarIcon statusBarIcon;

// The auth code we're required to send when connecting to GPMDP. Stored in the keychain. Reading
// this property is thread-safe, but writing it isn't.
//
// Returns nil if no code is found or if reading fails. If writing fails, an error is logged, but no
// exception is thrown.
@property NSString* __nullable googlePlayMusicDesktopPlayerPermanentAuthCode;

@end

#pragma clang assume_nonnull end


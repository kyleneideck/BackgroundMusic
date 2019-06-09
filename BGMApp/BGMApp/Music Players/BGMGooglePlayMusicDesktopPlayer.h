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
//  BGMGooglePlayMusicDesktopPlayer.h
//  BGMApp
//
//  Copyright © 2019 Kyle Neideck
//
//  We have a lot more code for GPMDP than most music players largely because GPMDP has a WebSockets
//  API and because the user has to enter a code from GPMDP to allow BGMApp to control it.
//  Currently, the other music players all have AppleScript APIs, so for them the OS asks the user
//  for permission on our behalf automatically and handles the whole process for us.
//
//  This class implements the usual BGMMusicPlayer methods and handles the UI for authenticating
//  with GPMDP. BGMGooglePlayMusicDesktopPlayerConnection manages the connection to GPMDP and hides
//  the details of its API.
//

// Superclass/Protocol Import
#import "BGMMusicPlayer.h"


#pragma clang assume_nonnull begin

API_AVAILABLE(macos(10.10))
@interface BGMGooglePlayMusicDesktopPlayer : BGMMusicPlayerBase<BGMMusicPlayer>

+ (NSArray<id<BGMMusicPlayer>>*) createInstancesWithDefaults:(BGMUserDefaults*)userDefaults;

@end

#pragma clang assume_nonnull end


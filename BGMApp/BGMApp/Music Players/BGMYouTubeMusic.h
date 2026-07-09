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
//  BGMYouTubeMusic.h
//  BGMApp
//
//  Copyright © 2026 Background Music contributors
//
//  Auto-pause support for YouTube Music (https://music.youtube.com) running in a Chromium-based
//  browser: Brave, Google Chrome or Microsoft Edge.
//
//  These browsers all ship the same AppleScript dictionary (see Chromium.h), so we use Scripting
//  Bridge to find the tab that has YouTube Music open and to read/control its <video> element via
//  the browser's "execute javascript" command. createInstancesWithDefaults returns one instance
//  per installed browser.
//
//  Requirements and limitations:
//
//  - "execute javascript" only works if the user has enabled
//    View → Developer → Allow JavaScript from Apple Events in their browser (a one-time setting
//    that persists). If it's disabled, the browser refuses the command. When that happens we show a
//    one-time dialog explaining how to enable it and behave as if nothing is playing.
//
//  - BGMDriver matches the audio it should control to the browser by its bundle ID. All of a
//    browser's tabs share the same bundle ID, so at the driver level audio from YouTube Music is
//    indistinguishable from audio played by any other tab of the same browser. This means
//    auto-pause won't trigger for audio played in another tab of the same browser (and, conversely,
//    pausing YouTube Music while another tab is still playing audio won't stop that other audio).
//

// Superclass/Protocol Import
#import "BGMMusicPlayer.h"


#pragma clang assume_nonnull begin

@interface BGMYouTubeMusic : BGMMusicPlayerBase<BGMMusicPlayer>

+ (NSArray<id<BGMMusicPlayer>>*) createInstancesWithDefaults:(BGMUserDefaults*)userDefaults;

@end

#pragma clang assume_nonnull end

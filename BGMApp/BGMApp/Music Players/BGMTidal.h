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
//  BGMTidal.h
//  BGMApp
//
//  Copyright © 2026 Background Music contributors
//
//  Auto-pause support for the TIDAL desktop app.
//
//  TIDAL is an Electron app that doesn't expose an AppleScript dictionary or a local API, so unlike
//  most of the other music player classes we can't use Scripting Bridge. Instead, we control it
//  through the macOS Accessibility API: we read and press the first item of TIDAL's "Playback" menu
//  (the play/pause toggle) directly, without visibly opening the menu.
//
//  This requires the user to grant Background Music the Accessibility permission
//  (System Settings > Privacy & Security > Accessibility). See BGMTidal.m for the details.
//

// Superclass/Protocol Import
#import "BGMMusicPlayer.h"


@interface BGMTidal : BGMMusicPlayerBase<BGMMusicPlayer>

@end

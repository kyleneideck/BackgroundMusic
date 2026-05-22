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
//  BGIINA.h
//  BGMApp
//
//  Copyright © 2024 Background Music contributors
//
//  IINA 播放器支持。通过 mpv IPC socket 控制播放。
//  需要用户在 ~/.config/mpv/mpv.conf 中配置: input-ipc-server=/tmp/iina-mpv-socket
//

// Superclass/Protocol Import
#import "BGMMusicPlayer.h"


@interface BGIINA : BGMMusicPlayerBase<BGMMusicPlayer>

@end

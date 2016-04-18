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
//  BGM_XPCHelper.h
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//

#ifndef __BGMDriver__BGM_XPCHelper__
#define __BGMDriver__BGM_XPCHelper__

// System Includes
#include <MacTypes.h>

#pragma clang assume_nonnull begin

#if defined(__cplusplus)
extern "C" {
#endif

// On failure, returns one of the kBGMXPC_* error codes, or the error code received from BGMXPCHelper. Returns kBGMXPC_Success otherwise.
UInt64 WaitForBGMAppToStartOutputDevice(void);

#if defined(__cplusplus)
}
#endif

#pragma clang assume_nonnull end

#endif /* __BGMDriver__BGM_XPCHelper__ */


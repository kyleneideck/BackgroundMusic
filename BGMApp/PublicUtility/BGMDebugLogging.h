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
//  BGMDebugLogging.h
//  PublicUtility
//
//  Copyright © 2020 Kyle Neideck
//
//  Functions to globally enable/disable debug logging, i.e. more detailed logging to help diagnose
//  bugs. If debug logging is enabled, the DebugMsg macro from CADebugMacros.h (and possibly others)
//  will log messages. If not, it won't do anything.
//
//  If the preprocessor macro CoreAudio_UseSysLog is true, which is currently the case for all build
//  variants (see BGMApp/BGMApp.xcodeproj/project.pbxproj and
//  BGMDriver/BGMDriver.xcodeproj/project.pbxproj), those messages will be logged using syslog and
//  can be read using Console.app. Try searching for "background music", "bgm" or "coreaudiod".
//
//  Debug logging is enabled by default in debug builds, but in release builds you have to enable it
//  by option-clicking the status bar icon and then checking the Debug Logging menu item. Enabling
//  debug logging probably won't cause glitches, but we don't try to guarantee that and it's not
//  well tested.
//

#ifndef PublicUtility__BGMDebugLogging
#define PublicUtility__BGMDebugLogging

#pragma clang assume_nonnull begin

/*!
 * @return Non-zero if debug logging is globally enabled. (Probably -- it's not synchronised.)
 *         Real-time safe.
 */
#if defined(__cplusplus)
extern "C"
#endif
int BGMDebugLoggingIsEnabled(void);

/*!
 * @param inEnabled Non-zero to globally enable debug logging, zero to disable it. The change might
 *                  not be visible to other threads immediately.
 */
#if defined(__cplusplus)
extern "C"
#endif
void BGMSetDebugLoggingEnabled(int inEnabled);

#pragma clang assume_nonnull end

#endif /* PublicUtility__BGMDebugLogging */


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
//  BGMDebugLogging.c
//  PublicUtility
//
//  Copyright © 2020 Kyle Neideck
//

// Self Include
#include "BGMDebugLogging.h"


#pragma clang assume_nonnull begin

// It's probably not ideal to use a global variable for this, but it's a lot easier.
#if DEBUG || CoreAudio_Debug
    // Enable debug logging by default in debug builds.
    int gDebugLoggingIsEnabled = 1;
#else
    int gDebugLoggingIsEnabled = 0;
#endif

// We don't bother synchronising accesses of gDebugLoggingIsEnabled because it isn't really
// necessary and would complicate code that accesses it on realtime threads.
int BGMDebugLoggingIsEnabled()
{
    return gDebugLoggingIsEnabled;
}

void BGMSetDebugLoggingEnabled(int inEnabled)
{
    gDebugLoggingIsEnabled = inEnabled;
}

#pragma clang assume_nonnull end


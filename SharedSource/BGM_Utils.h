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
//  BGM_Utils.h
//  SharedSource
//
//  Copyright © 2016 Kyle Neideck
//

#ifndef __SharedSource__BGM_Utils__
#define __SharedSource__BGM_Utils__

// PublicUtility Includes
#include "CADebugMacros.h"
#include "CAException.h"

// System Includes
#include <mach/mach_error.h>

// The Assert macro from CADebugMacros with support for format strings added.
#define BGMAssert(inCondition, inMessage, ...)                                  \
    if(!(inCondition))                                                          \
    {                                                                           \
        DebugMsg(inMessage, ## __VA_ARGS__);                                    \
        __ASSERT_STOP;                                                          \
    }


class BGM_Utils
{
    
public:
    // Similar to ThrowIfKernErr, but also logs the Mach error string that corresponds to inError
    static void ThrowIfMachError(const char* inCallingMethod, const char* inErrorReturnedBy, kern_return_t inError)
    {
#if !DEBUG
    #pragma unused (inCallingMethod, inErrorReturnedBy)
#endif
        
        if(inError != KERN_SUCCESS)
        {
            DebugMsg("%s: %s returned an error (%d): %s", inCallingMethod, inErrorReturnedBy, inError, mach_error_string(inError));
            Throw(CAException(inError));
        }
    }
    
};

#endif /* __SharedSource__BGM_Utils__ */


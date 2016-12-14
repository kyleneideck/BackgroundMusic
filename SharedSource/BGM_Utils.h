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

#ifndef SharedSource__BGM_Utils
#define SharedSource__BGM_Utils

// PublicUtility Includes
#include "CADebugMacros.h"
#include "CAException.h"

#if defined(__cplusplus)

// STL Includes
#include <functional>

#endif /* defined(__cplusplus) */

// System Includes
#include <mach/error.h>


// The Assert macro from CADebugMacros with support for format strings added.
#define BGMAssert(inCondition, inMessage, ...)                                  \
    if(!(inCondition))                                                          \
    {                                                                           \
        DebugMsg(inMessage, ## __VA_ARGS__);                                    \
        __ASSERT_STOP;                                                          \
    }


#pragma clang assume_nonnull begin

#if defined(__cplusplus)

#define BGMLogAndSwallowExceptions(callerName, function) \
    BGM_Utils::LogAndSwallowExceptions(__FILE__, __LINE__, callerName, function)

#define BGMLogAndSwallowExceptionsMsg(callerName, message, function) \
    BGM_Utils::LogAndSwallowExceptions(__FILE__, __LINE__, callerName, message, function)

#define BGMLogUnexpectedException(callerName) \
    BGM_Utils::LogUnexpectedException(__FILE__, __LINE__, callerName)

#define BGMLogUnexpectedExceptions(callerName, function) \
    BGM_Utils::LogUnexpectedExceptions(__FILE__, __LINE__, callerName, function)

#define BGMLogUnexpectedExceptionsMsg(callerName, message, function) \
    BGM_Utils::LogUnexpectedExceptions(__FILE__, __LINE__, callerName, message, function)


namespace BGM_Utils
{
    // Log (and swallow) errors returned by Mach functions. Returns false if there was an error.
    bool LogIfMachError(const char* callerName,
                        const char* errorReturnedBy,
                        mach_error_t error);
    
    // Similar to ThrowIfKernelError from CADebugMacros.h, but also logs (in debug builds) the
    // Mach error string that corresponds to the error.
    void ThrowIfMachError(const char* callerName,
                          const char* errorReturnedBy,
                          mach_error_t error);
    
    // If function throws an exception, log an error and continue.
    //
    // Fails/stops debug builds. It's likely that if we log an error for an exception in release
    // builds, even if it's expected (i.e. not a bug in Background Music), we'd want to know if
    // it gets thrown during testing/debugging.
    OSStatus LogAndSwallowExceptions(const char* __nullable fileName,
                                     int lineNumber,
                                     const char* callerName,
                                     const std::function<void(void)>& function);
    
    OSStatus LogAndSwallowExceptions(const char* __nullable fileName,
                                     int lineNumber,
                                     const char* callerName,
                                     const char* __nullable message,
                                     const std::function<void(void)>& function);
    
    void     LogException(const char* __nullable fileName,
                          int lineNumber,
                          const char* callerName,
                          CAException e);
    
    void     LogUnexpectedException(const char* __nullable fileName,
                                    int lineNumber,
                                    const char* callerName);
    
    OSStatus LogUnexpectedExceptions(const char* callerName,
                                     const std::function<void(void)>& function);
    
    OSStatus LogUnexpectedExceptions(const char* __nullable fileName,
                                     int lineNumber,
                                     const char* callerName,
                                     const std::function<void(void)>& function);
    
    // Log unexpected exceptions and continue.
    //
    // Generally, you don't want to use this unless the alternative is to crash. And even then
    // crashing is often the better option. (Especially if we've added crash reporting by the
    // time you're reading this.)
    //
    // Fails/stops debug builds.
    //
    // TODO: Allow a format string and args for the message.
    OSStatus LogUnexpectedExceptions(const char* __nullable fileName,
                                     int lineNumber,
                                     const char* callerName,
                                     const char* __nullable message,
                                     const std::function<void(void)>& function);
    
}

#endif /* defined(__cplusplus) */

#pragma clang assume_nonnull end

#endif /* SharedSource__BGM_Utils */


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
//  BGM_Utils.cpp
//  SharedSource
//
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#include "BGM_Utils.h"

// Local Includes
#include "BGM_Types.h"

// System Includes
#include <MacTypes.h>
#include <mach/mach_error.h>


#pragma clang assume_nonnull begin

namespace BGM_Utils
{
    // Forward declarations
    static OSStatus LogAndSwallowExceptions(const char* __nullable fileName,
                                            int lineNumber,
                                            const char* callerName,
                                            const char* __nullable message,
                                            bool expected,
                                            const std::function<void(void)>& function);

#pragma mark Exception utils
    
    bool LogIfMachError(const char* callerName,
                        const char* errorReturnedBy,
                        mach_error_t error)
    {
        if(error != KERN_SUCCESS)
        {
            char* errorStr = mach_error_string(error);
            LogError("%s: %s returned an error (%d): %s\n",
                     callerName,
                     errorReturnedBy,
                     error,
                     errorStr ? errorStr : "Unknown error");
            return false;
        }
        
        return true;
    }

    void ThrowIfMachError(const char* callerName,
                          const char* errorReturnedBy,
                          mach_error_t error)
    {
        if(!LogIfMachError(callerName, errorReturnedBy, error))
        {
            Throw(CAException(error));
        }
    }
    
    OSStatus LogAndSwallowExceptions(const char* __nullable fileName,
                                     int lineNumber,
                                     const char* callerName,
                                     const std::function<void(void)>& function)
    {
        return LogAndSwallowExceptions(fileName, lineNumber, callerName, nullptr, true, function);
    }

    OSStatus LogAndSwallowExceptions(const char* __nullable fileName,
                                     int lineNumber,
                                     const char* callerName,
                                     const char* __nullable message,
                                     const std::function<void(void)>& function)
    {
        return LogAndSwallowExceptions(fileName, lineNumber, callerName, message, true, function);
    }
    
    void LogException(const char* __nullable fileName,
                      int lineNumber,
                      const char* callerName,
                      CAException e)
    {
        LogError("%s:%d:%s: CAException, error code: %d.",
                 (fileName ? fileName : ""),
                 lineNumber,
                 callerName,
                 e.GetError());
    }
    
    void LogUnexpectedException(const char* __nullable fileName,
                                int lineNumber,
                                const char* callerName)
    {
        LogError("%s:%d:%s: Unknown unexpected exception.",
                 (fileName ? fileName : ""),
                 lineNumber,
                 callerName);
    }
    
    OSStatus LogUnexpectedExceptions(const char* callerName,
                                     const std::function<void(void)>& function)
    {
        return LogUnexpectedExceptions(nullptr, -1, callerName, nullptr, function);
    }
    
    OSStatus LogUnexpectedExceptions(const char* __nullable fileName,
                                     int lineNumber,
                                     const char* callerName,
                                     const std::function<void(void)>& function)
    {
        return LogUnexpectedExceptions(fileName, lineNumber, callerName, nullptr, function);
    }
    
    OSStatus LogUnexpectedExceptions(const char* __nullable fileName,
                                     int lineNumber,
                                     const char* callerName,
                                     const char* __nullable message,
                                     const std::function<void(void)>& function)
    {
        return LogAndSwallowExceptions(fileName, lineNumber, callerName, message, false, function);
    }

#pragma mark Implementation

    static OSStatus LogAndSwallowExceptions(const char* __nullable fileName,
                                            int lineNumber,
                                            const char* callerName,
                                            const char* __nullable message,
                                            bool expected,
                                            const std::function<void(void)>& function)
    {
        try
        {
            function();
        }
        catch(CAException e)
        {
            // TODO: Can/should we log a stack trace somewhere? (If so, also in the following catch
            //       block.)
            // TODO: Log a warning instead of an error for expected exceptions?
            LogError("%s:%d:%s: %sCAException, error code: %d. %s%s %s %s ",
                     (fileName ? fileName : ""),
                     lineNumber,
                     callerName,
                     (expected ? "" : "Unexpected "),
                     e.GetError(),
                     (message ? message : ""),
                     (message ? "." : ""),
                     (expected ? "If you think this might be a bug:"
                               : "Feel free to report this at"),
                     kBGMIssueTrackerURL);
            
#if BGM_StopDebuggerOnLoggedExceptions
            BGMAssert(false, "CAException");
#endif
            return e.GetError();
        }
        catch(...)
        {
            LogError("%s:%d:%s: %s exception. %s%s %s %s",
                     (fileName ? fileName : ""),
                     lineNumber,
                     callerName,
                     (expected ? "Unknown" : "Unexpected unknown"),
                     (message ? message : ""),
                     (message ? "." : ""),
                     (expected ? "If you think this might be a bug:"
                               : "Feel free to report this at"),
                     kBGMIssueTrackerURL);

#if BGM_StopDebuggerOnLoggedExceptions
            BGMAssert(false, "Unknown exception");
#endif
            return -1;
        }
        
        return noErr;
    }
}

#pragma clang assume_nonnull end


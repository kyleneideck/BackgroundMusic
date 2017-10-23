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
//  Copyright © 2016, 2017 Kyle Neideck
//

// Self Include
#include "BGM_Utils.h"

// Local Includes
#include "BGM_Types.h"

// System Includes
#include <MacTypes.h>
#include <mach/mach_error.h>
#include <CoreFoundation/CoreFoundation.h>  // For kCFCoreFoundationVersionNumber


#pragma clang assume_nonnull begin

dispatch_queue_t BGMGetDispatchQueue_PriorityUserInteractive()
{
    long queueClass;

    // Compile-time check that QOS_CLASS_USER_INTERACTIVE can be used. It was added in 10.10.
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101000  // MAC_OS_X_VERSION_10_10
    // Runtime check for the same.
    if(floor(kCFCoreFoundationVersionNumber) > kCFCoreFoundationVersionNumber10_9)
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
        queueClass = QOS_CLASS_USER_INTERACTIVE;
#pragma clang diagnostic pop
    }
    else
#endif
    {
        // Fallback for older versions.
        queueClass = DISPATCH_QUEUE_PRIORITY_HIGH;
    }

    return dispatch_get_global_queue(queueClass, 0);
}

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
                      const CAException& e)
    {
        OSStatus err = e.GetError();
        const char err4CC[5] = CA4CCToCString(err);

        LogError("%s:%d:%s: CAException, code: '%s' (%d).",
                 (fileName ? fileName : ""),
                 lineNumber,
                 callerName,
                 err4CC,
                 err);
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
        catch(const CAException& e)
        {
            // TODO: Can/should we log a stack trace somewhere? (If so, also in the following catch
            //       block.)
            // TODO: Log a warning instead of an error for expected exceptions?
            OSStatus err = e.GetError();
            const char err4CC[5] = CA4CCToCString(err);

            LogError("%s:%d:%s: %sCAException, code: '%s' (%d). %s%s %s %s ",
                     (fileName ? fileName : ""),
                     lineNumber,
                     callerName,
                     (expected ? "" : "Unexpected "),
                     err4CC,
                     err,
                     (message ? message : ""),
                     (message ? "." : ""),
                     (expected ? "If you think this might be a bug:"
                               : "Feel free to report this at"),
                     kBGMIssueTrackerURL);
            
#if BGM_StopDebuggerOnLoggedExceptions || BGM_StopDebuggerOnLoggedUnexpectedExceptions
#if !BGM_StopDebuggerOnLoggedExceptions
            if(!expected)
#endif
            {
                BGMAssert(false, "CAException");
            }
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

#if BGM_StopDebuggerOnLoggedExceptions || BGM_StopDebuggerOnLoggedUnexpectedExceptions
            BGMAssert(false, "Unknown exception");
#endif
            return -1;
        }
        
        return noErr;
    }
}

#pragma clang assume_nonnull end


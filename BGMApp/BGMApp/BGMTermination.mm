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
//  BGMTermination.mm
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//

// Self Include
#import "BGMTermination.h"

// Local Includes
#import "BGM_Utils.h"

// PublicUtility Includes
#import "CADebugMacros.h"

// STL Includes
#import <string>

// System Includes
#import <signal.h>
#import <Cocoa/Cocoa.h>


#pragma clang assume_nonnull begin

std::terminate_handler BGMTermination::sOriginalTerminateHandler = std::get_terminate();

CAPThread* const       BGMTermination::sExitSignalsThread = new CAPThread(ExitSignalsProc, nullptr);
sigset_t               BGMTermination::sExitSignals;

BGMAudioDeviceManager* __nullable BGMTermination::sAudioDevices = nullptr;

// If BGMApp crashes, CrashReporter will read this string from our process' memory and include it in
// the crash report.
const char* __nullable __crashreporter_info__ = nullptr;
// Set the REFERENCED_DYNAMICALLY bit so the symbol doesn't get stripped from the binary. See
// <https://developer.apple.com/library/content/documentation/DeveloperTools/Reference/Assembler/040-Assembler_Directives/asm_directives.html>
// and
// <https://github.com/aidansteele/osx-abi-macho-file-format-reference#symbol-table-and-related-data-structures>
// (Ctrl+F "REFERENCED_DYNAMICALLY").
asm(".desc ___crashreporter_info__, 0x10");


// static
void BGMTermination::TestCrash()
{
    // To give BGMApp a few seconds to finish launching and then crash:
    //
    // dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)),
    //                dispatch_get_main_queue(),
    //                ^{
    //                    BGMTermination::TestCrash();
    //                });

    // throw CAException(kAudioHardwareBadDeviceError);
    // throw BGM_InvalidClientRelativeVolumeException();
    // std::string().at(1);
    // *reinterpret_cast<int*>(0x1234) = 9;
    // [NSException raise:@"ObjC Test Exception" format:@"The description of the test exception."];
}

// static
void BGMTermination::SetUpTerminationCleanUp(BGMAudioDeviceManager* inAudioDevices)
{
    sAudioDevices = inAudioDevices;

    StartExitSignalsThread();

    // Wrap the default handler for std::terminate, which is called if BGMApp crashes because of an
    // uncaught C++ or Objective C exception, so we can clean up first.
    sOriginalTerminateHandler = std::get_terminate();

    std::set_terminate([] {
        CleanUpAudioDevices();

        AddCurrentExceptionToCrashReport();

        // Call the default terminate handler to finish crashing normally.
        sOriginalTerminateHandler();
    });
}

// static
void    BGMTermination::StartExitSignalsThread()
{
    // Block the signals the thread will handle, so they can be unblocked for just that thread.
    sigemptyset(&sExitSignals);
    sigaddset(&sExitSignals, SIGQUIT);
    sigaddset(&sExitSignals, SIGTERM);
    sigaddset(&sExitSignals, SIGINT);

    if(pthread_sigmask(SIG_BLOCK, &sExitSignals, nullptr) != 0)
    {
        perror("pthread_sigmask");
        return;  // This would just mean the signals would be handled by the default handlers.
    }

    // Start the thread.
    sExitSignalsThread->Start();
}

// static
void    BGMTermination::CleanUpAudioDevices()
{
    // BGMXPCHelper would set the output device back if we didn't do it here, but in general
    // it's better for things to work even if BGMXPCHelper isn't installed.
    if(sAudioDevices)
    {
        [sAudioDevices unsetBGMDeviceAsOSDefault];
    }
}

// static
void    BGMTermination::AddCurrentExceptionToCrashReport()
{
    std::exception_ptr exceptionPtr = std::current_exception();

    if(exceptionPtr)
    {
        // The message to add to the crash report (and log).
        std::string* msg = new std::string("");

        // Throw the exception again and catch it so we can get some info if it's a CAException. If
        // it's a std::exception, the default terminate handler will do the same thing, so we can
        // just ignore it here.
        try
        {
            std::rethrow_exception(exceptionPtr);
        }
        catch(const CAException& e)
        {
            OSStatus err = e.GetError();
            const char err4CC[5] = CA4CCToCString(err);

            msg = new std::string("Uncaught CAException. Error code: '");
            msg->append(err4CC);
            msg->append("' (");
            msg->append(std::to_string(err));
            msg->append(").");
        }
        catch(...)
        {
        }

        // CrashReporter will read the contents of __crashreporter_info__.
        __crashreporter_info__ = msg->c_str();
        NSLog(@"%s", msg->c_str());
    }
}

// The entry point for the thread that handles SIGQUIT, SIGTERM and SIGINT.
// static
void* __nullable BGMTermination::ExitSignalsProc(void* __nullable ignored)
{
    #pragma unused (ignored)

    DebugMsg("BGMTermination::ExitSignalsProc: Thread started.");

    int signal = -1;

    // Wait until we receive a signal.
    while((signal != SIGINT) && (signal != SIGTERM) && (signal != SIGQUIT))
    {
        if(sigwait(&sExitSignals, &signal) != 0)
        {
            perror("sigwait");
            return nullptr;
        }
    }

    if(signal == SIGINT)
    {
        NSLog(@"Interrupted.");
    }
    else if(signal == SIGTERM)
    {
        NSLog(@"Exiting.");
    }

    CleanUpAudioDevices();

    // Unblock the signal and resend it to ourselves so it will be handled by the default handler
    // and exit BGMApp.
    if(pthread_sigmask(SIG_UNBLOCK, &sExitSignals, nullptr) != 0)
    {
        perror("pthread_sigmask");
        abort();
    }

    raise(signal);
    return nullptr;
}

#pragma clang assume_nonnull end


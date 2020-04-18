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
//  BGMPlayThroughRTLogger.h
//  BGMApp
//
//  Copyright © 2020 Kyle Neideck
//
//  A real-time safe logger for BGMPlayThrough. The messages are logged asynchronously by a
//  non-realtime thread.
//
//  For the sake of simplicity, this class is very closely coupled with BGMPlayThrough and its
//  methods make assumptions about where they will be called. Also, if the same logging method is
//  called multiple times before the logging thread next checks for messages, it will only log the
//  message for one of those calls and ignore the others.
//
//  This class's methods are real-time safe in that they return in a bounded amount of time and we
//  think they're probably fast enough that the callers won't miss their deadlines, but we don't try
//  to guarantee it. Some of them should only be called in unusual cases where it's worth increasing
//  the risk of a thread missing its deadline.
//

#ifndef BGMApp__BGMPlayThroughRTLogger
#define BGMApp__BGMPlayThroughRTLogger

// PublicUtility Includes
#include "CARingBuffer.h"

// STL Includes
#include <thread>

// System Includes
#include <mach/error.h>
#include <mach/semaphore.h>


#pragma clang assume_nonnull begin

class BGMPlayThroughRTLogger
{

#pragma mark Construction/Destruction

public:
                            BGMPlayThroughRTLogger();
                            ~BGMPlayThroughRTLogger();
                            BGMPlayThroughRTLogger(const BGMPlayThroughRTLogger&) = delete;
                            BGMPlayThroughRTLogger& operator=(
                                    const BGMPlayThroughRTLogger&) = delete;
private:
    static semaphore_t      CreateSemaphore();

#pragma mark Log Messages

public:
    /*! For BGMPlayThrough::ReleaseThreadsWaitingForOutputToStart. */
    void                    LogReleasingWaitingThreads();
    /*! For BGMPlayThrough::ReleaseThreadsWaitingForOutputToStart. */
    void                    LogIfMachError_ReleaseWaitingThreadsSignal(mach_error_t inError);

    /*! For BGMPlayThrough::OutputDeviceIOProc. Not thread-safe. */
    void                    LogIfDroppedFrames(Float64 inFirstInputSampleTime,
                                               Float64 inLastInputSampleTime);
    /*! For BGMPlayThrough::OutputDeviceIOProc. Not thread-safe. */
    void                    LogNoSamplesReady(CARingBuffer::SampleTime inLastInputSampleTime,
                                              CARingBuffer::SampleTime inReadHeadSampleTime,
                                              Float64 inInToOutSampleOffset);

    /*! For BGMPlayThrough::UpdateIOProcState. Not thread-safe. */
    void                    LogExceptionStoppingIOProc(const char* inCallerName)
                            {
                                LogExceptionStoppingIOProc(inCallerName, noErr, false);
                            }
    /*! For BGMPlayThrough::UpdateIOProcState. Not thread-safe. */
    void                    LogExceptionStoppingIOProc(const char* inCallerName, OSStatus inError)
                            {
                                LogExceptionStoppingIOProc(inCallerName, inError, true);
                            }

private:
    void                    LogExceptionStoppingIOProc(const char* inCallerName,
                                                       OSStatus inError,
                                                       bool inErrorKnown);

public:
    /*! For BGMPlayThrough::UpdateIOProcState. Not thread-safe. */
    void                    LogUnexpectedIOStateAfterStopping(const char* inCallerName,
                                                              int inIOState);
    /*! For BGMPlayThrough::InputDeviceIOProc and BGMPlayThrough::OutputDeviceIOProc. */
    void                    LogRingBufferUnavailable(const char* inCallerName, bool inGotLock);
    /*! For BGMPlayThrough::OutputDeviceIOProc. */
    void                    LogIfRingBufferError_Fetch(CARingBufferError inError)
                            {
                                LogIfRingBufferError(inError, mRingBufferFetchError);
                            }
    /*! For BGMPlayThrough::InputDeviceIOProc. */
    void                    LogIfRingBufferError_Store(CARingBufferError inError)
                            {
                                LogIfRingBufferError(inError, mRingBufferStoreError);
                            }

private:
    void                    LogIfRingBufferError(CARingBufferError inError,
                                                 std::atomic<CARingBufferError>& outError);

    template <typename T, typename F>
    void                    LogAsync(T& inMessageData, F&& inStoreMessageData);

    // Wrapper methods used to mock out the logging for unit tests.
    void                    LogSync_Warning(const char* inFormat, ...) __printflike(2, 3);
    void                    LogSync_Error(const char* inFormat, ...) __printflike(2, 3);

#pragma mark Logging Thread

private:
    void                    WakeLoggingThread();

    void                    LogMessages();
    void                    LogSync_ReleasingWaitingThreads();
    void                    LogSync_ReleaseWaitingThreadsSignalError();
    void                    LogSync_DroppedFrames();
    void                    LogSync_NoSamplesReady();
    void                    LogSync_ExceptionStoppingIOProc();
    void                    LogSync_UnexpectedIOStateAfterStopping();
    void                    LogSync_RingBufferUnavailable();
    void                    LogSync_RingBufferError(
                                std::atomic<CARingBufferError>& ioRingBufferError,
                                const char* inMethodName);

    // The entry point of the logging thread (mLoggingThread).
    static void* __nullable LoggingThreadEntry(BGMPlayThroughRTLogger* inRefCon);

#if BGM_UnitTest

#pragma mark Test Helpers

public:
    /*!
     * @return True if the logger thread finished logging the requested messages. False if it still
     *         had messages to log after 5 seconds.
     */
    bool                    WaitUntilLoggerThreadIdle();

#endif /* BGM_UnitTest */

private:
    // For BGMPlayThrough::ReleaseThreadsWaitingForOutputToStart
    std::atomic<bool>       mLogReleasingWaitingThreadsMsg { false };
    std::atomic<kern_return_t> mReleaseWaitingThreadsSignalError { KERN_SUCCESS };

    // For BGMPlayThrough::InputDeviceIOProc and BGMPlayThrough::OutputDeviceIOProc
    struct {
        Float64 firstInputSampleTime;
        Float64 lastInputSampleTime;
        std::atomic<bool> shouldLogMessage { false };
    } mDroppedFrames;

    struct {
        CARingBuffer::SampleTime lastInputSampleTime;
        CARingBuffer::SampleTime readHeadSampleTime;
        Float64 inToOutSampleOffset;
        std::atomic<bool> shouldLogMessage { false };
    } mNoSamplesReady;

    struct {
        const char* callerName;
        bool gotLock;
        std::atomic<bool> shouldLogMessage { false };
    } mRingBufferUnavailable;

    // For BGMPlayThrough::UpdateIOProcState
    struct {
        const char* callerName;
        int ioState;
        std::atomic<bool> shouldLogMessage { false };
    } mUnexpectedIOStateAfterStopping;

    struct {
        const char* callerName;
        OSStatus error;
        bool errorKnown;  // If false, we didn't get an error code from the exception.
        std::atomic<bool> shouldLogMessage { false };
    } mExceptionStoppingIOProc;

    // For BGMPlayThrough::OutputDeviceIOProc
    std::atomic<CARingBufferError> mRingBufferStoreError { kCARingBufferError_OK };
    // For BGMPlayThrough::InputDeviceIOProc.
    std::atomic<CARingBufferError> mRingBufferFetchError { kCARingBufferError_OK };

    // Signalled to wake up the mLoggingThread when it has messages to log.
    semaphore_t             mWakeUpLoggingThreadSemaphore;
    std::atomic<bool>       mLoggingThreadShouldExit { false };
    // The thread that actually logs the messages.
    std::thread             mLoggingThread;

#if BGM_UnitTest

public:
    // Tests normally crash (abort) if LogError is called. This flag lets us test the code that
    // would otherwise call LogError.
    bool                    mContinueOnErrorLogged { false };

    int                     mNumDebugMessagesLogged { 0 };
    int                     mNumWarningMessagesLogged { 0 };
    int                     mNumErrorMessagesLogged { 0 };

#endif /* BGM_UnitTest */

};

#pragma clang assume_nonnull end

#endif /* BGMApp__BGMPlayThroughRTLogger */


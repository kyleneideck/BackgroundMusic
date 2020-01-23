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
//  BGMPlayThroughRTLoggerTests.mm
//  BGMAppUnitTests
//
//  Copyright © 2020 Kyle Neideck
//

// Unit Include
#import "BGMPlayThroughRTLogger.h"

// PublicUtility Includes
#import "CARingBuffer.h"

// System Includes
#import <CoreAudio/CoreAudio.h>
#import <XCTest/XCTest.h>


@interface BGMPlayThroughRTLoggerTests : XCTestCase

@end

@implementation BGMPlayThroughRTLoggerTests
{
    BGMPlayThroughRTLogger* logger;
}

- (void) setUp {
    [super setUp];
    logger = new BGMPlayThroughRTLogger;
}

- (void) tearDown {
    [super tearDown];
    delete logger;
}

- (void) testLogReleasingWaitingThreads {
    logger->LogReleasingWaitingThreads();
    [self assertLoggedOneDebugMessage];
}

- (void) testLogIfMachError_ReleaseWaitingThreadsSignal {
    logger->LogIfMachError_ReleaseWaitingThreadsSignal(KERN_SUCCESS);
    [self assertLoggedNoMessages];
}

- (void) testLogIfDroppedFrames_didNotDropFrames {
    logger->LogIfDroppedFrames(11256.0, 11256.0);
    [self assertLoggedNoMessages];
}

- (void) testLogIfDroppedFrames_didDropFrames {
    logger->LogIfDroppedFrames(11256.0, 11768.0);
    [self assertLoggedOneDebugMessage];
}

- (void) testLogNoSamplesReady {
    logger->LogNoSamplesReady(512, 1024, 512.0);
    [self assertLoggedOneDebugMessage];
}

- (void) testLogExceptionStoppingIOProc_withoutErrorCode {
    // Set a test-only flag that keeps it from calling abort() after logging the error when the
    // tests are compiled with the debug configuration.
    logger->mContinueOnErrorLogged = true;

    logger->LogExceptionStoppingIOProc("InputDeviceIOProc");
    [self assertLoggedOneErrorMessage];
}

- (void) testLogExceptionStoppingIOProc_withErrorCode {
    // Set a test-only flag that keeps it from calling abort() after logging the error when the
    // tests are compiled with the debug configuration.
    logger->mContinueOnErrorLogged = true;

    logger->LogExceptionStoppingIOProc("OutputDeviceIOProc", kAudioHardwareUnknownPropertyError);
    [self assertLoggedOneErrorMessage];
}

- (void) testLogUnexpectedIOStateAfterStopping {
    logger->LogUnexpectedIOStateAfterStopping("OutputDeviceIOProc", 1);
    [self assertLoggedOneWarningMessage];
}

- (void) testLogIfRingBufferError_Fetch_noError {
    logger->LogIfRingBufferError_Fetch(kCARingBufferError_OK);
    [self assertLoggedNoMessages];
}

- (void) testLogIfRingBufferError_Fetch_errorCPUOverload {
    logger->LogIfRingBufferError_Fetch(kCARingBufferError_CPUOverload);
    [self assertLoggedOneWarningMessage];
}

- (void) testLogIfRingBufferError_Fetch_errorTooMuch {
    // Set a test-only flag that keeps it from calling abort() after logging the error when the
    // tests are compiled with the debug configuration.
    logger->mContinueOnErrorLogged = true;

    logger->LogIfRingBufferError_Fetch(kCARingBufferError_TooMuch);
    [self assertLoggedOneErrorMessage];
}

- (void) testLogIfRingBufferError_Store_noError {
    logger->LogIfRingBufferError_Store(kCARingBufferError_OK);
    [self assertLoggedNoMessages];
}

- (void) testLogIfRingBufferError_Store_errorCPUOverload {
    logger->LogIfRingBufferError_Store(kCARingBufferError_CPUOverload);
    [self assertLoggedOneWarningMessage];
}

- (void) testLogIfRingBufferError_Store_errorTooMuch {
    // Set a test-only flag that keeps it from calling abort() after logging the error when the
    // tests are compiled with the debug configuration.
    logger->mContinueOnErrorLogged = true;

    logger->LogIfRingBufferError_Store(kCARingBufferError_TooMuch);
    [self assertLoggedOneErrorMessage];
}

- (void) waitForLoggingThread {
    // Wait for it to finish logging the messages.
    bool noMessagesLeft = logger->WaitUntilLoggerThreadIdle();
    XCTAssert(noMessagesLeft);
}

- (void) assertLoggedNoMessages {
    [self waitForLoggingThread];
    XCTAssertEqual(0, logger->mNumDebugMessagesLogged);
    XCTAssertEqual(0, logger->mNumWarningMessagesLogged);
    XCTAssertEqual(0, logger->mNumErrorMessagesLogged);
}

- (void) assertLoggedOneDebugMessage {
    [self waitForLoggingThread];
    XCTAssertEqual(1, logger->mNumDebugMessagesLogged);
    XCTAssertEqual(0, logger->mNumWarningMessagesLogged);
    XCTAssertEqual(0, logger->mNumErrorMessagesLogged);
}

- (void) assertLoggedOneWarningMessage {
    [self waitForLoggingThread];
    XCTAssertEqual(0, logger->mNumDebugMessagesLogged);
    XCTAssertEqual(1, logger->mNumWarningMessagesLogged);
    XCTAssertEqual(0, logger->mNumErrorMessagesLogged);
}

- (void) assertLoggedOneErrorMessage {
    [self waitForLoggingThread];
    XCTAssertEqual(0, logger->mNumDebugMessagesLogged);
    XCTAssertEqual(0, logger->mNumWarningMessagesLogged);
    XCTAssertEqual(1, logger->mNumErrorMessagesLogged);
}

@end


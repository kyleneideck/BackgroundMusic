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
//  BGMXPCHelperTests.m
//  BGMXPCHelperTests
//
//  Copyright © 2016 Kyle Neideck
//

// Local Includes
#import "BGM_TestUtils.h"
#import "BGMXPCProtocols.h"

// System Includes
#import <Foundation/Foundation.h>


#pragma clang assume_nonnull begin

// To run these tests, BGMXPCHelper has to be installed and its launchd job enabled.

@interface BGMXPCHelperTests : XCTestCase
@end

@implementation BGMXPCHelperTests {
    NSXPCConnection* connection;
}

- (void) setUp {
    [super setUp];
    
    connection = [[NSXPCConnection alloc] initWithMachServiceName:kBGMXPCHelperMachServiceName
                                                          options:NSXPCConnectionPrivileged];
    
    connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(BGMXPCHelperXPCProtocol)];
    [connection resume];
}

- (void) tearDown {
    [connection invalidate];
    
    [super tearDown];
}

- (void) testStartOutputDeviceWithoutBGMAppConnected {
    dispatch_semaphore_t replySemaphore = dispatch_semaphore_create(0);

    // Unregister BGMXPCHelper's connection to BGMApp in case BGMApp didn't shutdown cleanly the last time it ran.
    [[connection remoteObjectProxy] unregisterAsBGMApp];
        
    [[connection remoteObjectProxy] startBGMAppPlayThroughSyncWithReply:^(NSError* reply) {
        XCTAssertEqual([reply code],
                       kBGMXPC_MessageFailure,
                       @"Check that BGMApp isn't running, which would cause this failure");
        
        dispatch_semaphore_signal(replySemaphore);
    } forUISoundsDevice:NO];

    // Very long timeout to make it less likely to fail on Travis CI when there's high contention.
    if (0 != dispatch_semaphore_wait(replySemaphore, dispatch_time(DISPATCH_TIME_NOW, 5 * 60 * NSEC_PER_SEC))) {
        XCTFail(@"Timed out waiting for BGMXPCHelper");
    }
}

@end

#pragma clang assume_nonnull end


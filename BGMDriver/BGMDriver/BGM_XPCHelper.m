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
//  BGM_XPCHelper.cpp
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#import "BGM_XPCHelper.h"

// Local Includes
#import "BGMXPCProtocols.h"

// PublicUtility Includes
#include "CADebugMacros.h"

// System Includes
#import <Foundation/Foundation.h>


#pragma clang assume_nonnull begin

static const UInt64 REMOTE_CALL_DEFAULT_TIMEOUT_SECS = 30;

static NSXPCConnection* CreateXPCHelperConnection()
{
    // Create a connection to BGMXPCHelper's Mach service. If it isn't already running, launchd will start BGMXPCHelper when we send
    // a message to this connection.
    //
    // Uses the NSXPCConnectionPrivileged option because BGMXPCHelper has to run in the privileged/global bootstrap context for
    // BGMDriver to be able to look it up. BGMDriver runs in the coreaudiod process, which runs in the global context, and services
    // in the global context are only able to look up other services in that context.
    NSXPCConnection* theConnection = [[NSXPCConnection alloc] initWithMachServiceName:kBGMXPCHelperMachServiceName
                                                                              options:NSXPCConnectionPrivileged];
    
    if (theConnection) {
        theConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(BGMXPCHelperXPCProtocol)];
        [theConnection resume];
    } else {
        @throw(@"BGM_XPCHelper::WaitForBGMAppToStartOutputDevice: initWithMachServiceName returned nil");
    }
    
    return theConnection;
}

UInt64 WaitForBGMAppToStartOutputDevice()
{
    __block UInt64 theAnswer = kBGMXPC_Success;
    
    // Connect to our XPC helper.
    //
    // We can't initiate an XPC connection with BGMApp directly for security reasons, so we use BGMXPCHelper as an intermediary. (We
    // could use BGMXPCHelper to initiate the connection and then talk to BGMApp directly, but so far we haven't had any reason to.)
    //
    // It would be faster to keep the connection ready whenever BGMApp is a client of BGMDevice, but it's not important for this case.
    NSXPCConnection* theConnection = CreateXPCHelperConnection();
    
    // This semaphore will be signalled when we get a reply from BGMXPCHelper, or the message fails.
    dispatch_semaphore_t theReplySemaphore = dispatch_semaphore_create(0);
   
    // Set the failure callbacks to signal the reply semaphore so we can return immediately if BGMXPCHelper can't be reached. (It
    // doesn't matter how many times we signal the reply semaphore because we create a new one each time.)
    void (^failureHandler)(void) = ^{
        DebugMsg("BGM_XPCHelper::WaitForBGMAppToStartOutputDevice: Connection to BGMXPCHelper failed");
        
        theAnswer = kBGMXPC_MessageFailure;
        dispatch_semaphore_signal(theReplySemaphore);
    };
    theConnection.interruptionHandler = failureHandler;
    theConnection.invalidationHandler = failureHandler;
    
    // This remote call to BGMXPCHelper will send a reply when the output device is ready to receive IO. Note that, for security
    // reasons, we shouldn't trust the reply object.
    [[theConnection remoteObjectProxyWithErrorHandler:^(NSError* error) {
#if !DEBUG
    #pragma unused (error)
#endif
        DebugMsg("BGM_XPCHelper::WaitForBGMAppToStartOutputDevice: Remote call error: %s",
                 [[error debugDescription] UTF8String]);
        
        failureHandler();
    }] waitForBGMAppToStartOutputDeviceWithReply:^(NSError* reply) {
        DebugMsg("BGM_XPCHelper::WaitForBGMAppToStartOutputDevice: Got reply from BGMXPCHelper: \"%s\"",
                 [[reply localizedDescription] UTF8String]);
        
        theAnswer = (UInt64)[reply code];
        
        // We only need the connection for one call, which was successful, so the losing the connection is no longer a problem.
        theConnection.interruptionHandler = nil;
        theConnection.invalidationHandler = nil;
        
        // Tell the enclosing function it can return now.
        dispatch_semaphore_signal(theReplySemaphore);
    }];
    
    DebugMsg("BGM_XPCHelper::WaitForBGMAppToStartOutputDevice: Waiting for BGMApp to tell us the output device is ready for IO");
    
    // Wait on the reply semaphore until we get the reply (or a connection failure).
    if (0 != dispatch_semaphore_wait(theReplySemaphore,
                                     dispatch_time(DISPATCH_TIME_NOW, REMOTE_CALL_DEFAULT_TIMEOUT_SECS * NSEC_PER_SEC))) {
        // Log a warning if we timeout.
        //
        // TODO: It's possible that the output device is just taking a really long time to start. Is there some way we could check for
        //       that, rather than timing out?
        NSLog(@"BGM_XPCHelper::WaitForBGMAppToStartOutputDevice: Timed out waiting for the Background Music app to start the output device");
        
        theAnswer = kBGMXPC_Timeout;
    }
    
   [theConnection invalidate];
    
    return theAnswer;
}

#pragma clang assume_nonnull end


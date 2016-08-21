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
//  BGMXPCHelperService.m
//  BGMXPCHelper
//
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#import "BGMXPCHelperService.h"

// Local Includes
#import "BGMXPCListenerDelegate.h"

// PublicUtility Includes
#undef CoreAudio_ThreadStampMessages
#define CoreAudio_ThreadStampMessages 0  // Requires C++
#include "CADebugMacros.h"


#pragma clang assume_nonnull begin

static NSXPCListenerEndpoint* __nullable sBGMAppEndpoint = nil;
static NSXPCConnection* __nullable sBGMAppConnection = nil;

@implementation BGMXPCHelperService {
    NSXPCConnection* connection;
}

- (id) initWithConnection:_connection {
    if ((self = [super init])) {
        connection = _connection;
    }
    
    return self;
}

+ (NSError*) errorWithCode:(NSInteger)code description:(NSString*)description {
        return [NSError errorWithDomain:kBGMXPCHelperMachServiceName
                                   code:code
                               userInfo:@{ NSLocalizedDescriptionKey: description }];
}

+ (NSError*) errorWithCode:(NSInteger)code description:(NSString*)description underlyingError:(NSError*)underlyingError {
        return [NSError errorWithDomain:kBGMXPCHelperMachServiceName
                                   code:code
                               userInfo:@{ NSLocalizedDescriptionKey: description,
                                           NSUnderlyingErrorKey: underlyingError }];
}

+ (void) withBGMAppRemoteProxy:(void (^)(id))block errorHandler:(void (^)(NSError* error))errorHandler {
    // Retry by default
    return [BGMXPCHelperService withBGMAppRemoteProxy:block errorHandler:errorHandler retryOnError:YES];
}

+ (void) withBGMAppRemoteProxy:(void (^)(id))block errorHandler:(void (^)(NSError* error))errorHandler retryOnError:(BOOL)retry {
    // Wraps some error handling around a block that calls BGMApp remotely, and runs it.
    
    if (!sBGMAppConnection && sBGMAppEndpoint) {
        // Create a new connection to BGMApp from the endpoint
        @synchronized(self) {
            sBGMAppConnection = [[NSXPCConnection alloc] initWithListenerEndpoint:(NSXPCListenerEndpoint* __nonnull)sBGMAppEndpoint];
            NSAssert(sBGMAppConnection, @"NSXPCConnection::initWithListenerEndpoint returned nil");
            
            [sBGMAppConnection setRemoteObjectInterface:[NSXPCInterface interfaceWithProtocol:@protocol(BGMAppXPCProtocol)]];
            sBGMAppConnection.invalidationHandler = ^{
                sBGMAppConnection = nil;
            };
            
            [sBGMAppConnection resume];
        }
    }
    
    if (sBGMAppConnection) {
        id proxy = [sBGMAppConnection remoteObjectProxyWithErrorHandler:^(NSError* error) {
            if (retry) {
                DebugMsg("BGMXPCHelperService::withBGMAppRemoteProxy: %s error=<%lu, %s>",
                         "Error sending message to BGMApp. Creating new connection and retrying.",
                         [error code],
                         [[error localizedDescription] UTF8String]);
                
                // Clear the stored connection so a new one will be created when we retry. (The connection might still be
                // valid, but it's simpler to just make a new one every time.)
                @synchronized(self) {
                    sBGMAppConnection = nil;
                }
                
                // Retry the message.
                [BGMXPCHelperService withBGMAppRemoteProxy:block errorHandler:errorHandler retryOnError:NO];
            } else {
                NSLog(@"BGMXPCHelperService::withBGMAppRemoteProxy: Error sending message to BGMApp: %@", error);
                errorHandler(error);
            }
        }];
        
        block(proxy);
    } else {
        errorHandler([BGMXPCHelperService errorWithCode:kBGMXPC_MessageFailure description:@"No connection to BGMApp"]);
    }
}

#pragma mark Exported Methods

- (void) registerAsBGMAppWithListenerEndpoint:(NSXPCListenerEndpoint*)endpoint reply:(void (^)(void))reply {
    [self debugWarnIfCalledByBGMDriver];
    
    DebugMsg("BGMXPCHelperService::registerAsBGMAppWithListenerEndpoint: Received BGMApp listener endpoint");
    
    // Store the connection (which we now know is from BGMApp) and endpoint so all instances of this class can use them.
    @synchronized([self class]) {
        sBGMAppEndpoint = endpoint;
        sBGMAppConnection = connection;
        
        [sBGMAppConnection setRemoteObjectInterface:[NSXPCInterface interfaceWithProtocol:@protocol(BGMAppXPCProtocol)]];
        
        // Set the stored connection back to nil when BGMApp closes, dies or invalidates the connection.
        void(^cleanUp)(void) = ^{
            @synchronized([self class]) {
                sBGMAppConnection = nil;
            }
        };
        sBGMAppConnection.interruptionHandler = cleanUp;
        sBGMAppConnection.invalidationHandler = cleanUp;
    }
    
    reply();
}

- (void) unregisterAsBGMApp {
    [self debugWarnIfCalledByBGMDriver];
    
    DebugMsg("BGMXPCHelperService::unregisterAsBGMApp: Destroying connection to BGMApp");
    
    // TODO: We don't want to assume only one instance of BGMApp will be running, in case multiple users are running it.
    @synchronized([self class]) {
        if (sBGMAppConnection) {
            [sBGMAppConnection invalidate];
            sBGMAppConnection = nil;
            sBGMAppEndpoint = nil;
        }
    }
}

- (void) waitForBGMAppToStartOutputDeviceWithReply:(void (^)(NSError*))reply {
    [self debugWarnIfCalledByBGMApp];
    
    // If this reply string isn't set before the end of this method, it's a bug
    __block NSError* replyToBGMDriver = [BGMXPCHelperService errorWithCode:kBGMXPC_InternalError
                                                               description:@"Reply not set in waitForBGMAppToStartOutputDeviceWithReply"];
    
    // I couldn't find the Obj-C equivalent of xpc_connection_send_message_with_reply_sync so just wait on this
    // semaphore until we get a reply from BGMApp (or timeout)
    dispatch_semaphore_t bgmAppReplySemaphore = dispatch_semaphore_create(0);
    
    DebugMsg("BGMXPCHelperService::waitForBGMAppToStartOutputDeviceWithReply: Waiting for BGMApp to start IO on the output device");
    
    // Send the message to BGMApp
    [BGMXPCHelperService withBGMAppRemoteProxy:^(id remoteObjectProxy) {
        [remoteObjectProxy waitForOutputDeviceToStartWithReply:^(NSError* bgmAppReply) {
            replyToBGMDriver = bgmAppReply;
            dispatch_semaphore_signal(bgmAppReplySemaphore);
        }];
    } errorHandler:^(NSError* error) {
        replyToBGMDriver = [BGMXPCHelperService errorWithCode:kBGMXPC_MessageFailure
                                                  description:[error localizedDescription]
                                              underlyingError:error];
        dispatch_semaphore_signal(bgmAppReplySemaphore);
    }];
    
    // Wait for BGMApp's reply
    long err = dispatch_semaphore_wait(bgmAppReplySemaphore, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC));
    
    if (err != 0) {
        replyToBGMDriver = [BGMXPCHelperService errorWithCode:kBGMXPC_Timeout
                                                  description:@"Timed out waiting for BGMApp"];
    }

    // Return the reply to BGMDriver
    DebugMsg("BGMXPCHelperService::waitForBGMAppToStartOutputDeviceWithReply: Reply to BGMDriver: %s",
             [[replyToBGMDriver localizedDescription] UTF8String]);
    reply(replyToBGMDriver);
}

#pragma mark Debug Utils

- (void) debugWarnIfCalledByBGMApp {
#if DEBUG
    if ([connection effectiveUserIdentifier] != [BGMXPCListenerDelegate _coreaudiodUID]) {
        DebugMsg("BGMXPCHelperService::debugWarnIfCalledByBGMDriver: A method intended for BGMDriver only was (probably) called "
                 "by BGMApp. Or it could have just been the tests.");
        NSLog(@"%@", [NSThread callStackSymbols]);
    }
#endif
}

- (void) debugWarnIfCalledByBGMDriver {
#if DEBUG
    if ([connection effectiveUserIdentifier] == [BGMXPCListenerDelegate _coreaudiodUID]) {
        DebugMsg("BGMXPCHelperService::debugWarnIfCalledByBGMDriver: A method intended for BGMApp only was (probably) called by BGMDriver");
        NSLog(@"%@", [NSThread callStackSymbols]);
    }
#endif
}

@end

#pragma clang assume_nonnull end


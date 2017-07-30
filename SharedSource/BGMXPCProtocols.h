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
//  BGMXPCProtocols.h
//  SharedSource
//
//  Copyright © 2016 Kyle Neideck
//

// Local Includes
#include "BGM_Types.h"

// System Includes
#import <Foundation/Foundation.h>


#pragma clang assume_nonnull begin

static NSString* kBGMXPCHelperMachServiceName = @kBGMXPCHelperBundleID;

// The protocol that BGMXPCHelper will vend as its XPC API.
@protocol BGMXPCHelperXPCProtocol

// Tells BGMXPCHelper that the caller is BGMApp and passes a listener endpoint that BGMXPCHelper can use to create connections to BGMApp.
// BGMXPCHelper may also pass the endpoint on to BGMDriver so it can do the same.
- (void) registerAsBGMAppWithListenerEndpoint:(NSXPCListenerEndpoint*)endpoint reply:(void (^)(void))reply;
- (void) unregisterAsBGMApp;

// BGMDriver calls this remote method when it wants BGMApp to start IO. BGMXPCHelper passes the message along and then passes the response
// back. This allows BGMDriver to wait for the audio hardware to start up, which means it can let the HAL know when it's safe to start
// sending us audio data from the client.
//
// If BGMApp can be reached, the error it returns will be passed the reply block. Otherwise, the reply block will be passed an error with
// one of the kBGMXPC_* error codes. It may have an underlying error using one of the NSXPCConnection* error codes from FoundationErrors.h.
- (void) startBGMAppPlayThroughSyncWithReply:(void (^)(NSError*))reply forUISoundsDevice:(BOOL)isUI;
    
@end


// The protocol that BGMApp will vend as its XPC API.
@protocol BGMAppXPCProtocol

- (void) startPlayThroughSyncWithReply:(void (^)(NSError*))reply forUISoundsDevice:(BOOL)isUI;

@end

#pragma clang assume_nonnull end


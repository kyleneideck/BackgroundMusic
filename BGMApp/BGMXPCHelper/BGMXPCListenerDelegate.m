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
//  BGMXPCListenerDelegate.m
//  BGMXPCHelper
//
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#import "BGMXPCListenerDelegate.h"

// Local Includes
#import "BGMXPCHelperService.h"

// PublicUtility Includes
#include "CADebugMacros.h"

// System Includes
#import <pwd.h>


#pragma clang assume_nonnull begin

static uid_t sCoreaudiodUID = 0; // Uses 0 for unset

@implementation BGMXPCListenerDelegate

+ (uid_t) _coreaudiodUID {
    // Lazily get the UID of the _coreaudiod user.
    if (sCoreaudiodUID == 0) {
        long bufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
        if (-1 == bufferSize) {
            @throw @"BGMXPCListenerDelegate::_coreaudiodUID: sysconf failed";
        }
        char unusedBuffer[bufferSize];
        struct passwd passwd, *resultPtr = NULL;
        
        if (0 != getpwnam_r("_coreaudiod", &passwd, unusedBuffer, bufferSize, &resultPtr)) {
            @throw @"BGMXPCListenerDelegate::_coreaudiodUID: getpwnam_r failed";
        }
        
        // When getpwnam_r can't find the user it leaves resultPtr null
        if (resultPtr) {
            DebugMsg("BGMXPCListenerDelegate::_coreaudiodUID: Found user: name=%s uid=%u", passwd.pw_name, passwd.pw_uid);
            sCoreaudiodUID = passwd.pw_uid;
        } else {
            DebugMsg("BGMXPCListenerDelegate::_coreaudiodUID: Your system doesn't appear to have a user named \"_coreaudiod\". Unless "
                     "there's something weird going on with your system, this is probably a bug.");
            sCoreaudiodUID = 0;
        }
    }
    
    return sCoreaudiodUID;
}

- (BOOL) listener:(NSXPCListener*)listener shouldAcceptNewConnection:(NSXPCConnection*)newConnection {
    #pragma unused (listener)
    
    // This method is where the NSXPCListener configures, accepts, and resumes a new incoming NSXPCConnection.
    
    DebugMsg("BGMXPCListenerDelegate::listener: Received new connection. effectiveUserIdentifier=%u _coreaudiodUID=%u",
             [newConnection effectiveUserIdentifier],
             [BGMXPCListenerDelegate _coreaudiodUID]);
    
    // Configure the connection.
    // First, set the interface that the exported object implements.
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(BGMXPCHelperXPCProtocol)];
    
    // Next, set the object that the connection exports. All messages sent on the connection to this service will be sent to
    // the exported object to handle. The connection retains the exported object.
    newConnection.exportedObject = [[BGMXPCHelperService alloc] initWithConnection:newConnection];
    
    // Resuming the connection allows the system to deliver more incoming messages.
    [newConnection resume];
    
    // Returning YES from this method tells the system that you have accepted this connection. If you want to reject the
    // connection for some reason, call -invalidate on the connection and return NO.
    return YES;
}

@end

#pragma clang assume_nonnull end


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
//  main.m
//  BGMXPCHelper
//
//  Copyright © 2016 Kyle Neideck
//
//  BGMXPCHelper passes XPC messages between BGMDriver and BGMApp. So far it's only used for synchronization while starting IO.
//
//  BGMApp and BGMDriver usually communicate by changing device properties and listening for notifications about those changes, which the
//  HAL sends. We use XPC, or plan to use it, for the few cases that notifications don't suit. For example, shared memory regions can be
//  sent via XPC. So we might be able to share a ring buffer between BGMDriver and BGMApp and avoid the latency and CPU overhead of
//  sending the audio data through an input stream. XPC should also let us safely send messages from real-time functions.
//
//  Notifications would have probably worked fine for synchronizing BGMDriver and BGMApp while starting IO. But the Core Audio headers
//  don't specify which threads notifications are sent/received on, what (if anything) they can block on, or what can be blocked by their
//  handler callbacks. So using XPC instead makes it a bit easier to reason about the process and avoid potential deadlocks. (That said,
//  I ended up adding a huge amount of error handling and boilerplate code for XPC, so this way is probably more confusing overall.)
//
//  CFMessagePort would be much simpler than XPC to use, but as far as I could tell there's no way to create a remote CFMessagePort for
//  a Mach port without looking the Mach port up using the bootstrap service. And because BGMXPCHelper and BGMApp have to use different
//  bootstrap namespaces, BGMXPCHelper wouldn't be able to see services vended by BGMApp. So BGMXPCHelper would be able to receive
//  messages from BGMApp and reply to them, but not send them.
//
//  BGMXPCHelper has to run as a launchd daemon so the Mach service it vends will use the global bootstrap namespace. See Table 2 in
//  <https://developer.apple.com/library/mac/technotes/tn2083/_index.html>. This is because the coreaudiod process, where BGMDriver runs,
//  uses the global bootstrap namespace, which means it can only see services in that namespace. In practical terms, this just means we
//  have to run BGMXPCHelper by creating a launchd.plist file for it in /Library/LaunchDaemons and loading it with a command like
//      launchctl bootstrap system /Library/LaunchDaemons/com.bearisdriving.BGM.XPCHelper.plist
//

// Local Includes
#import "BGMXPCProtocols.h"
#import "BGMXPCListenerDelegate.h"

// PublicUtility Includes
#include "CADebugMacros.h"

// System Includes
#import <Foundation/Foundation.h>


#pragma clang assume_nonnull begin

int main(int argc, const char* __nullable argv[]) {
    #pragma unused (argc, argv)
    
    DebugMsg("BGMXPCHelper::main: Service starting up");
    
    // Set up the one NSXPCListener for this service. It will handle all incoming connections. This checks our service in with
    // the bootstrap service.
    NSXPCListener* listener = [[NSXPCListener alloc] initWithMachServiceName:kBGMXPCHelperMachServiceName];
    BGMXPCListenerDelegate* delegate = [BGMXPCListenerDelegate new];
    listener.delegate = delegate;
    
    // Start receiving requests.
    [listener resume];
    
    [[NSRunLoop currentRunLoop] run];
    
    return 0;
}

#pragma clang assume_nonnull end


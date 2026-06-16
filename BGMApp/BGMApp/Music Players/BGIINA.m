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
//  BGIINA.m
//  BGMApp
//
//  Copyright © 2024 Background Music contributors
//
//  IINA player support. Controls playback via mpv IPC socket.
//
//  Prerequisite: User must configure ~/.config/mpv/mpv.conf with:
//    input-ipc-server=/tmp/iina-mpv-socket
//
//  Alternative socket paths supported:
//    - /tmp/iina-mpv-socket (preferred)
//    - /tmp/mpv-socket (mpv default)
//
//  IINA is based on mpv and supports controlling playback via
//  Unix domain socket using JSON IPC commands.
//  Reference: https://mpv.io/manual/stable/#json-ipc
//

// Self Include
#import "BGIINA.h"

// Local Includes
#import "BGMAppWatcher.h"

// PublicUtility Includes
#import "CADebugMacros.h"

// System Includes
#import <Cocoa/Cocoa.h>
#import <sys/socket.h>
#import <sys/un.h>
#import <sys/stat.h>
#import <unistd.h>
#import <sys/ucred.h>


// mpv IPC socket paths to check (in priority order)
// 1. User-configured path in /tmp
// 2. Default mpv socket location
// 3. IINA-specific socket location
static NSString* const kIINAMPVSocketPaths[] = {
    @"/tmp/iina-mpv-socket",
    @"/tmp/mpv-socket",
};
static const NSUInteger kIINAMPVSocketPathCount = sizeof(kIINAMPVSocketPaths) / sizeof(kIINAMPVSocketPaths[0]);

// Socket timeout in seconds
static const NSTimeInterval kSocketTimeout = 5.0;

// Cached socket path (nil until first successful connection)
static NSString* _Nullable cachedSocketPath = nil;


#pragma clang assume_nonnull begin

@implementation BGIINA {
    BGMAppWatcher* appWatcher;
    BOOL _running;
}

- (instancetype) init {
    if ((self = [super initWithMusicPlayerID:[BGMMusicPlayerBase makeID:@"41FE4E09-85FF-40F3-B336-5C973F4CCD86"]
                                        name:@"IINA"
                                    bundleID:@"com.colliderli.iina"])) {
        BGIINA* __weak weakSelf = self;
        appWatcher = [[BGMAppWatcher alloc]
            initWithBundleID:@"com.colliderli.iina"
                 appLaunched:^{
                     BGIINA* strongSelf = weakSelf;
                     if (strongSelf) {
                         strongSelf->_running = YES;
                         DebugMsg("BGIINA: IINA launched");
                     }
                 }
               appTerminated:^{
                   BGIINA* strongSelf = weakSelf;
                   if (strongSelf) {
                       strongSelf->_running = NO;
                       DebugMsg("BGIINA: IINA terminated");
                   }
               }];

        NSArray<NSRunningApplication*>* running =
            [NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.colliderli.iina"];
        _running = (running.count > 0);
    }
    return self;
}

#pragma mark BGMMusicPlayer

- (BOOL) isRunning {
    NSArray<NSRunningApplication*>* running =
        [NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.colliderli.iina"];
    return running.count > 0;
}

- (BOOL) isPlaying {
    if (!self.running) return NO;

    // Query playback state via mpv IPC
    // get_property pause returns true when paused
    NSDictionary* response = [self sendMPVGetProperty:@"pause"];
    if (response && [response[@"error"] isEqualToString:@"success"]) {
        // data: false means not paused (playing)
        return [response[@"data"] boolValue] == NO;
    }
    return NO;
}

- (BOOL) isPaused {
    if (!self.running) return NO;

    NSDictionary* response = [self sendMPVGetProperty:@"pause"];
    if (response && [response[@"error"] isEqualToString:@"success"]) {
        // data: true means paused
        return [response[@"data"] boolValue] == YES;
    }
    return NO;
}

- (BOOL) pause {
    if (!self.running) {
        DebugMsg("BGIINA::pause: IINA is not running");
        return NO;
    }

    // Check if playing first to avoid redundant pause
    BOOL wasPlaying = self.playing;

    if (wasPlaying) {
        DebugMsg("BGIINA::pause: Pausing IINA via mpv IPC");
        [self sendMPVSetProperty:@"pause" value:@YES];
    }

    return wasPlaying;
}

- (BOOL) unpause {
    if (!self.running) {
        DebugMsg("BGIINA::unpause: IINA is not running");
        return NO;
    }

    // Check if paused first to avoid redundant unpause
    BOOL wasPaused = self.paused;

    if (wasPaused) {
        DebugMsg("BGIINA::unpause: Resuming IINA via mpv IPC");
        [self sendMPVSetProperty:@"pause" value:@NO];
    }

    return wasPaused;
}

#pragma mark mpv IPC - High-level Interface

- (NSDictionary* __nullable) sendMPVGetProperty:(NSString*)property {
    NSData* response = [self sendMPVCommand:@[@"get_property", property]];
    if (response) {
        return [self parseResponse:response];
    }
    return nil;
}

- (BOOL) sendMPVSetProperty:(NSString*)property value:(id)value {
    NSData* response = [self sendMPVCommand:@[@"set_property", property, value]];
    if (response) {
        NSDictionary* json = [self parseResponse:response];
        return json && [json[@"error"] isEqualToString:@"success"];
    }
    return NO;
}

- (NSDictionary* __nullable) parseResponse:(NSData*)data {
    NSError* error = nil;
    NSDictionary* json = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
    if (error) {
        DebugMsg("BGIINA::parseResponse: JSON parse failed: %s", error.localizedDescription.UTF8String);
        return nil;
    }
    return json;
}

#pragma mark mpv IPC - Low-level Interface

- (BOOL) isValidSocket:(NSString*)path {
    // Use lstat instead of stat to avoid following symlinks
    struct stat socketStat;
    if (lstat([path UTF8String], &socketStat) != 0) {
        return NO;
    }

    // Verify it's a socket file
    if (!S_ISSOCK(socketStat.st_mode)) {
        return NO;
    }

    // Verify owner is current user
    uid_t currentUid = getuid();
    if (socketStat.st_uid != currentUid) {
        DebugMsg("BGIINA::isValidSocket: Socket owner mismatch for %s (expected=%d, actual=%d)",
                 path.UTF8String, currentUid, socketStat.st_uid);
        return NO;
    }

    // Note: Do not check socket permissions because mpv creates sockets with 755 by default.
    // Security relies on LOCAL_PEERCRED verification after connect.

    return YES;
}

- (NSString* __nullable) findSocketPath {
    // Return cached path if still valid
    if (cachedSocketPath != nil) {
        NSString* path = cachedSocketPath;
        if ([self isValidSocket:path]) {
            return path;
        }
    }

    // Search for valid socket in priority order
    for (NSUInteger i = 0; i < kIINAMPVSocketPathCount; i++) {
        NSString* path = kIINAMPVSocketPaths[i];
        if ([self isValidSocket:path]) {
            cachedSocketPath = path;
            DebugMsg("BGIINA::findSocketPath: Found valid socket at %s", path.UTF8String);
            return path;
        }
    }

    DebugMsg("BGIINA::findSocketPath: No valid socket found");
    return nil;
}

- (NSData* __nullable) sendMPVCommand:(NSArray*)command {
    // Find valid socket path
    NSString* socketPath = [self findSocketPath];
    if (!socketPath) {
        DebugMsg("BGIINA::sendMPVCommand: No valid mpv IPC socket found");
        return nil;
    }

    // Create Unix domain socket connection
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        DebugMsg("BGIINA::sendMPVCommand: Failed to create socket");
        return nil;
    }

    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = (long)kSocketTimeout;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // Set socket address
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, [socketPath UTF8String], sizeof(addr.sun_path) - 1);

    // Connect to socket
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        DebugMsg("BGIINA::sendMPVCommand: Failed to connect to socket");
        close(sock);
        return nil;
    }

    // Verify peer credentials
    struct xucred peerCred;
    socklen_t credLen = sizeof(peerCred);
    if (getsockopt(sock, 0, LOCAL_PEERCRED, &peerCred, &credLen) == 0) {
        uid_t currentUid = getuid();
        if (peerCred.cr_uid != currentUid) {
            DebugMsg("BGIINA::sendMPVCommand: Peer credential verification failed (expected=%d, actual=%d)",
                     currentUid, peerCred.cr_uid);
            close(sock);
            return nil;
        }
    }

    // Build JSON IPC command
    // Format: {"command": [cmd, arg1, arg2, ...]}
    NSDictionary* ipcCommand = @{@"command": command};
    NSData* jsonData = [NSJSONSerialization dataWithJSONObject:ipcCommand options:0 error:nil];
    NSString* jsonCommand = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
    jsonCommand = [jsonCommand stringByAppendingString:@"\n"];

    // Send command
    ssize_t sent = send(sock, [jsonCommand UTF8String], [jsonCommand length], 0);
    if (sent < 0) {
        DebugMsg("BGIINA::sendMPVCommand: Failed to send command");
        close(sock);
        return nil;
    }

    // Receive response
    char buffer[4096];
    ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    close(sock);

    if (received <= 0) {
        DebugMsg("BGIINA::sendMPVCommand: No response received");
        return nil;
    }

    buffer[received] = '\0';
    return [NSData dataWithBytes:buffer length:received];
}

@end

#pragma clang assume_nonnull end

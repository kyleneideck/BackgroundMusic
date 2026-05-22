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
//  IINA 播放器支持。通过 mpv IPC socket 控制播放。
//
//  前提条件：用户需要在 ~/.config/mpv/mpv.conf 中配置：
//    input-ipc-server=/tmp/iina-mpv-socket
//
//  IINA 基于 mpv，支持通过 Unix domain socket 发送 JSON IPC 命令来控制播放。
//  参考：https://mpv.io/manual/stable/#json-ipc
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


// mpv IPC socket 路径
static NSString* const kIINAMPVSocketPath = @"/tmp/iina-mpv-socket";


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
                         DebugMsg("BGIINA: IINA 已启动");
                     }
                 }
               appTerminated:^{
                   BGIINA* strongSelf = weakSelf;
                   if (strongSelf) {
                       strongSelf->_running = NO;
                       DebugMsg("BGIINA: IINA 已退出");
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

    // 通过 mpv IPC 查询播放状态
    // get_property pause 返回 true 表示暂停中
    NSData* response = [self sendMPVCommand:@[@"get_property", @"pause"]];
    if (response) {
        NSDictionary* json = [NSJSONSerialization JSONObjectWithData:response options:0 error:nil];
        if (json && [json[@"error"] isEqualToString:@"success"]) {
            // data: false 表示未暂停（正在播放）
            return [json[@"data"] boolValue] == NO;
        }
    }
    return NO;
}

- (BOOL) isPaused {
    if (!self.running) return NO;

    NSData* response = [self sendMPVCommand:@[@"get_property", @"pause"]];
    if (response) {
        NSDictionary* json = [NSJSONSerialization JSONObjectWithData:response options:0 error:nil];
        if (json && [json[@"error"] isEqualToString:@"success"]) {
            // data: true 表示暂停中
            return [json[@"data"] boolValue] == YES;
        }
    }
    return NO;
}

- (BOOL) pause {
    if (!self.running) {
        DebugMsg("BGIINA::pause: IINA 未运行");
        return NO;
    }

    DebugMsg("BGIINA::pause: 通过 mpv IPC 暂停 IINA");
    NSData* response = [self sendMPVCommand:@[@"set_property", @"pause", @YES]];
    if (response) {
        NSDictionary* json = [NSJSONSerialization JSONObjectWithData:response options:0 error:nil];
        return json && [json[@"error"] isEqualToString:@"success"];
    }
    return NO;
}

- (BOOL) unpause {
    if (!self.running) {
        DebugMsg("BGIINA::unpause: IINA 未运行");
        return NO;
    }

    DebugMsg("BGIINA::unpause: 通过 mpv IPC 恢复 IINA");
    NSData* response = [self sendMPVCommand:@[@"set_property", @"pause", @NO]];
    if (response) {
        NSDictionary* json = [NSJSONSerialization JSONObjectWithData:response options:0 error:nil];
        return json && [json[@"error"] isEqualToString:@"success"];
    }
    return NO;
}

#pragma mark mpv IPC

- (BOOL) isValidSocket:(NSString*)path {
    // 使用 lstat 而不是 stat，避免跟踪 symlink
    struct stat socketStat;
    if (lstat([path UTF8String], &socketStat) != 0) {
        return NO;
    }

    // 验证是否是 socket 文件
    if (!S_ISSOCK(socketStat.st_mode)) {
        DebugMsg("BGIINA::isValidSocket: 文件不是 socket");
        return NO;
    }

    // 验证所有者是否是当前用户
    uid_t currentUid = getuid();
    if (socketStat.st_uid != currentUid) {
        DebugMsg("BGIINA::isValidSocket: socket 所有者不匹配 (expected=%d, actual=%d)",
                 currentUid, socketStat.st_uid);
        return NO;
    }

    // 注意：不检查 socket 权限，因为 mpv 默认创建 755 权限的 socket
    // 安全性主要依赖 LOCAL_PEERCRED 验证（在 connect 后执行）

    return YES;
}

- (NSData* __nullable) sendMPVCommand:(NSArray*)command {
    // 安全验证：检查 socket 文件是否合法
    if (![self isValidSocket:kIINAMPVSocketPath]) {
        DebugMsg("BGIINA::sendMPVCommand: mpv IPC socket 验证失败");
        return nil;
    }

    // 创建 Unix domain socket 连接
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        DebugMsg("BGIINA::sendMPVCommand: 创建 socket 失败");
        return nil;
    }

    // 设置 socket 地址
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, [kIINAMPVSocketPath UTF8String], sizeof(addr.sun_path) - 1);

    // 连接到 socket
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        DebugMsg("BGIINA::sendMPVCommand: 连接 socket 失败");
        close(sock);
        return nil;
    }

    // 验证连接的 peer credentials
    struct xucred peerCred;
    socklen_t credLen = sizeof(peerCred);
    if (getsockopt(sock, 0, LOCAL_PEERCRED, &peerCred, &credLen) == 0) {
        uid_t currentUid = getuid();
        if (peerCred.cr_uid != currentUid) {
            DebugMsg("BGIINA::sendMPVCommand: peer 身份验证失败 (expected=%d, actual=%d)",
                     currentUid, peerCred.cr_uid);
            close(sock);
            return nil;
        }
    }

    // 构建 JSON IPC 命令
    // 格式: {"command": [cmd, arg1, arg2, ...]}
    NSDictionary* ipcCommand = @{@"command": command};
    NSData* jsonData = [NSJSONSerialization dataWithJSONObject:ipcCommand options:0 error:nil];
    NSString* jsonCommand = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
    jsonCommand = [jsonCommand stringByAppendingString:@"\n"];

    // 发送命令
    ssize_t sent = send(sock, [jsonCommand UTF8String], [jsonCommand length], 0);
    if (sent < 0) {
        DebugMsg("BGIINA::sendMPVCommand: 发送命令失败");
        close(sock);
        return nil;
    }

    // 接收响应
    char buffer[4096];
    ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    close(sock);

    if (received <= 0) {
        DebugMsg("BGIINA::sendMPVCommand: 未收到响应");
        return nil;
    }

    buffer[received] = '\0';
    return [NSData dataWithBytes:buffer length:received];
}

@end

#pragma clang assume_nonnull end

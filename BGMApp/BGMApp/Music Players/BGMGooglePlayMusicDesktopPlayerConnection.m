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
//  BGMGooglePlayMusicDesktopPlayerConnection.m
//  BGMApp
//
//  Copyright © 2019 Kyle Neideck
//

// Self Include
#import "BGMGooglePlayMusicDesktopPlayerConnection.h"

// Local Includes
#import "BGM_Utils.h"

// PublicUtility Includes
#import "CADebugMacros.h"


#pragma clang assume_nonnull begin

// When GooglePlayMusicDesktopPlayer.js sends a message to this class, it sets the message handler
// name to one of these, which tells us what type of message it is. (This is a macro because you
// can't make a static const NSArray.)
#define kScriptMessageHandlerNames (@[@"gpmdp", @"log", @"error"])

@implementation BGMGooglePlayMusicDesktopPlayerConnection {
    // GPMDP has a WebSocket API, so we use a WKWebView to access it using Javascript. Using a
    // proper library would make the code a bit cleaner and save a little memory, but I'm not sure
    // it would be worth adding an external dependency for that.
    WKWebView* webView;
    NSString* __nullable permanentAuthCode;
    BGMUserDefaults* userDefaults;
    // The number of times to retry if we fail to connect. For example, if GPMDP is still starting
    // up. Set to 0 when we aren't trying to connect.
    int connectionRetries;

    // A UI callback that asks the user for the auth code GPMDP will display.
    NSString* __nullable (^authRequiredHandler)(void);
    // A UI callback that shows a connection error message.
    void (^connectionErrorHandler)(void);
    // A UI callback that shows a warning dialog explaining that GPMDP reported an API version that
    // we don't support yet.
    void (^apiVersionMismatchHandler)(NSString* reportedAPIVersion);
}

- (instancetype) initWithUserDefaults:(BGMUserDefaults*)defaults
                  authRequiredHandler:(NSString* __nullable (^)(void))authHandler
               connectionErrorHandler:(void (^)(void))errorHandler
            apiVersionMismatchHandler:(void (^)(NSString* reportedAPIVersion))apiVersionHandler {
    if((self = [super init])) {
        userDefaults = defaults;
        authRequiredHandler = authHandler;
        connectionErrorHandler = errorHandler;
        apiVersionMismatchHandler = apiVersionHandler;
        connectionRetries = 0;

        // Lazily initialised.
        permanentAuthCode = nil;

        // Report that GPMDP is stopped until we know otherwise.
        _playing = NO;
        _paused = NO;
    }

    return self;
}

// Creates and initialises webView, a WKWebView we use to communicate with GPMDP over WebSockets.
- (void) createWebView {
    // Read the Javascript we'll need for this.
    NSString* __nullable jsPath =
            [[NSBundle mainBundle] pathForResource:@"GooglePlayMusicDesktopPlayer.js"
                                            ofType:nil];
    NSError* err;
    NSString* __nullable jsStr =
        (!jsPath ? nil : [NSString stringWithContentsOfFile:BGMNN(jsPath)
                                                   encoding:NSUTF8StringEncoding
                                                      error:&err]);

    if (err || !jsStr || [jsStr isEqualToString:@""]) {
        // TODO: Return an error so the caller can show an error dialog or something.
        NSLog(@"Error loading GPMDP Javascript file: %@", err);
    } else {
        webView = [WKWebView new];

        // Register to receive messages from our Javascript. The messages are handled in
        // userContentController. We register several times using different names as a convenient
        // way to separate messages from GPMDP, messages to log and errors.
        for (NSString* name in kScriptMessageHandlerNames) {
            [webView.configuration.userContentController addScriptMessageHandler:self name:name];
        }

        // Load our Javascript functions into webView so we can call them later.
        [self evaluateJavaScript:BGMNN(jsStr)];
    }
}

- (void) connectWithRetries:(int)retries {
    if (retries < 0) {
        BGMAssert(false, "retries < 0");
        return;
    }

    if (!permanentAuthCode) {
        // Read the API auth code from user defaults (actually the keychain), if there is one. If
        // the user hasn't authenticated before, it will be nil.
        //
        // We do this lazily because it can show a password dialog in debug/unsigned builds.
        permanentAuthCode = userDefaults.googlePlayMusicDesktopPlayerPermanentAuthCode;
    }

    connectionRetries = retries;

    // Create the WKWebView we'll use to connect to GPMDP with WebSockets. Using a WKWebView means
    // Background Music uses a bit more memory while connected to GPMDP, around 15 MB for me, but
    // saves us having to complicate the build process to add a dependency on a proper library.
    [self createWebView];

    if (permanentAuthCode) {
        DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::connectWithRetries: "
                 "Connecting with auth code");

        NSString* __nullable percentEncodedCode =
                [BGMGooglePlayMusicDesktopPlayerConnection
                        toPercentEncoded:BGMNN(permanentAuthCode)];

        [self evaluateJavaScript:[NSString stringWithFormat:@"connect('%@');", percentEncodedCode]];
    } else {
        DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::connectWithRetries: "
                 "Connecting without auth code");
        [self evaluateJavaScript:@"connect();"];
    }

    // Check whether GPMDP is playing, paused or stopped.
    [self requestPlaybackState];
}

- (void) disconnect {
    // Stop retrying if we're in the process of connecting.
    connectionRetries = 0;

    // evaluateJavaScript is only safe to call on the main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
        DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::disconnect: Disconnecting");

        [webView evaluateJavaScript:@"disconnect();"
                  completionHandler:^(id __nullable result, NSError* __nullable error) {
#pragma unused (result)
                      if (error) {
                          NSLog(@"Error closing connection to GPMDP: %@", error);
                      }

                      // Allow the WKWebView to be garbage collected.
                      for (NSString* name in kScriptMessageHandlerNames) {
                          [webView.configuration.userContentController
                                  removeScriptMessageHandlerForName:name];
                      }
                      webView = nil;
                  }];
    });
}

- (void) evaluateJavaScript:(NSString*)js {
    // evaluateJavaScript is only safe to call on the main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
        [webView evaluateJavaScript:js
                  completionHandler:^(id __nullable result, NSError* __nullable error) {
#pragma unused (result)
                      if (error) {
                          // TODO: We should probably show an error dialog in some cases.
                          NSLog(@"JS error: %@", error);
                      }
                  }];
    });
}

- (void) playPause {
    [self evaluateJavaScript:@"playPause();"];
}

- (void) sendAuthCode:(NSString*)authCode {
    // Don't log the code itself just in case it could be a security problem.
    DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::sendAuthCode: Sending GPMDP auth code");

    // Percent-encode the user input just in case they entered something that could execute as
    // Javascript. We could limit the input to four digits instead, but this should be fine.
    NSString* __nullable percentEncodedCode =
            [BGMGooglePlayMusicDesktopPlayerConnection toPercentEncoded:authCode];

    // We send the message to GPMDP even if percentEncodedCode is nil so it will reply with an error
    // and BGMApp will ask the user for the auth code again.
    NSString* js = [NSString stringWithFormat:@"window.sendAuthCode('%@');", percentEncodedCode];
    [self evaluateJavaScript:js];
}

- (void) sendPermanentAuthCode {
    NSString* __nullable code = permanentAuthCode;

    if (code) {
        // Don't log the code itself just in case it could be a security problem.
        DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::sendPermanentAuthCode: "
                 "Sending GPMDP permanent auth code");

        // Percent-encode it just in case something it includes could be executed as Javascript.
        NSString* __nullable percentEncodedCode =
                [BGMGooglePlayMusicDesktopPlayerConnection toPercentEncoded:BGMNN(code)];

        // Pass the code to our WKWebView so it can send it to GPMDP.
        NSString* js =
                [NSString stringWithFormat:@"sendPermanentAuthCode('%@');", percentEncodedCode];
        [self evaluateJavaScript:js];
    } else {
        NSLog(@"BGMGooglePlayMusicDesktopPlayerConnection::sendPermanentAuthCode: No code to send");
    }
}

+ (NSString* __nullable)toPercentEncoded:(NSString*)rawString {
    // Just percent-encode every character (by passing an empty NSCharacterSet as the allowed
    // characters).
    NSString* __nullable percentEncoded = [rawString
                                           stringByAddingPercentEncodingWithAllowedCharacters:
                                           [NSCharacterSet characterSetWithCharactersInString:@""]];
    if (percentEncoded) {
        return percentEncoded;
    } else {
        // The docs say that stringByAddingPercentEncodingWithAllowedCharacters returns nil "if the
        // transformation is not possible", but don't explain when that could happen. According to
        // https://stackoverflow.com/a/33558934/1091063 it can be caused by the string containing
        // invalid unicode.
        NSLog(@"Could not encode");
        return nil;
    }
}

// Ask GPMDP whether it's playing, paused or stopped. The response is handled asynchronously in
// handleResultMessage.
- (void) requestPlaybackState {
    [self evaluateJavaScript:@"requestPlaybackState();"];
}

#pragma mark WKScriptMessageHandler Methods

- (void) userContentController:(WKUserContentController*)userContentController
       didReceiveScriptMessage:(WKScriptMessage*)message {
#pragma unused (userContentController)

    if ([@"log" isEqual:message.name]) {
        // The message body is always a string in this case.
        [self handleLogMessage:message.body];
    } else if ([@"error" isEqual:message.name]) {
        [self handleConnectionError];
    } else {
        BGMAssert([@"gpmdp" isEqual:message.name], "Unexpected message handler name");
        [self handleGPMDPMessage:message];
    }
}

- (void) handleLogMessage:(NSString*)message {
    (void)message;
#if DEBUG
    if (permanentAuthCode) {
        // Avoid logging the auth code, which would be a minor security issue.
        message = [message stringByReplacingOccurrencesOfString:BGMNN(permanentAuthCode)
                                                     withString:@"<private>"];
    }

    DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::userContentController: %s",
             message.UTF8String);
#endif
}

- (void) handleConnectionError {
    if (connectionRetries > 0) {
        DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::handleConnectionError: "
                 "Retrying in 1 second");

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(),
                       ^{
                           // Check connectionRetries again because disconnect may have been called.
                           if (connectionRetries > 0) {
                               DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::"
                                        "handleConnectionError: Retrying");
                               [self connectWithRetries:(connectionRetries - 1)];
                           }
                       });
    } else {
        NSLog(@"BGMGooglePlayMusicDesktopPlayerConnection::handleConnectionError: "
               "No retries left. Giving up.");
        connectionErrorHandler();
    }
}

- (void) handleGPMDPMessage:(WKScriptMessage*)message {
    // See https://github.com/MarshallOfSound/Google-Play-Music-Desktop-Player-UNOFFICIAL-/blob/master/docs/PlaybackAPI_WebSocket.md

    // Type check.
    if (![message.body isKindOfClass:[NSDictionary class]]) {
        NSLog(@"Unexpected message body type");
        return;
    }

    NSDictionary* body = message.body;
    NSString* messageType;

    // The key for the message type is "channel", except when the message is a response, in which
    // case the key can be "namespace".
    if ([body[@"channel"] isKindOfClass:[NSString class]]) {
        messageType = body[@"channel"];
    } else if ([body[@"namespace"] isKindOfClass:[NSString class]]) {
        messageType = body[@"namespace"];
    } else {
        NSLog(@"No channel/namespace");
        return;
    }

    // Handle the message depending on its type (or ignore it).
    if ([@"API_VERSION" isEqual:messageType]) {
        [self handleAPIVersionMessage:body];
    } else if ([@"connect" isEqual:messageType]) {
        [self handleConnectMessage:body];
    } else if ([@"playState" isEqual:messageType]) {
        [self handlePlayStateMessage:body];
    } else if ([@"result" isEqual:messageType]) {
        [self handleResultMessage:body];
    }
}

- (void) handleAPIVersionMessage:(NSDictionary*)body {
    DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::handleAPIVersionMessage: Response: %s",
             [NSString stringWithFormat:@"%@", body].UTF8String);

    // Type check.
    if (![body[@"payload"] isKindOfClass:[NSString class]]) {
        NSLog(@"Unexpected payload type");
        [self handleConnectionError];
        return;
    }

    NSString* apiVersion = body[@"payload"];
    // "1.0.0" -> ["1", "0", "0"]
    NSArray<NSString*>* versionParts = [apiVersion componentsSeparatedByString:@"."];

    // Check the major version number is 1, which is the only major version we support.
    if (versionParts.count > 0) {
        NSInteger majorVersion = versionParts[0].integerValue;

        DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::handleAPIVersionMessage: "
                 "Major version: %lu", majorVersion);

        if (majorVersion == 1) {
            // GPMDP uses SemVer, so as long as the major version number matches what we can handle,
            // it should work.
            DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::handleAPIVersionMessage: "
                     "This API version is supported");
            return;
        }
    }

    // Show a warning dialog box to the user, but try to continue anyway. There's probably a
    // reasonable chance it'll still work.
    DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::handleAPIVersionMessage: "
             "Unsupported GPMDP API version");
    apiVersionMismatchHandler(apiVersion);
}

- (void) handleConnectMessage:(NSDictionary*)body {
    // Don't log the response as it may contain the auth code.
    DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::handleConnectMessage: Received response");

    // Type check.
    if (![body[@"payload"] isKindOfClass:[NSString class]]) {
        NSLog(@"Unexpected payload type");
        [self handleConnectionError];
        return;
    }

    NSString* payload = body[@"payload"];

    if ([@"CODE_REQUIRED" isEqual:payload]) {
        // Ask the user for the auth code GPMDP is displaying and send it to GPMDP to finish
        // connecting.
        NSString* __nullable authCode = authRequiredHandler();

        if (authCode) {
            [self sendAuthCode:BGMNN(authCode)];
        }
    } else {
        // The payload should be the permanent auth code.
        permanentAuthCode = payload;
        [self sendPermanentAuthCode];

        // Save the code to the keychain so we can use it when connecting to GPMDP in future.
        userDefaults.googlePlayMusicDesktopPlayerPermanentAuthCode = permanentAuthCode;
    }
}

- (void) handlePlayStateMessage:(NSDictionary*)body {
    (void)body;
    DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::handlePlayStateMessage: Response: %s",
             [NSString stringWithFormat:@"%@", body].UTF8String);

    // This message tells us the playstate has changed, but doesn't differentiate between stopped
    // and paused. The response to this API request will. See handleResultMessage.
    // TODO: Can it transition from stopped to paused? Would that be a problem?
    [self requestPlaybackState];
}

- (void) handleResultMessage:(NSDictionary*)body {
    DebugMsg("BGMGooglePlayMusicDesktopPlayerConnection::handleResultMessage: Response: %s",
             [NSString stringWithFormat:@"%@", body].UTF8String);

    // Type check.
    if (![body[@"value"] isKindOfClass:[NSNumber class]]) {
        NSLog(@"No value");
        return;
    }

    // 0 - Playback is stopped
    // 1 - Track is paused
    // 2 - Track is playing
    int state = ((NSNumber*)body[@"value"]).intValue;
    _playing = (state == 2);
    _paused = (state == 1);
}

@end

#pragma clang assume_nonnull end


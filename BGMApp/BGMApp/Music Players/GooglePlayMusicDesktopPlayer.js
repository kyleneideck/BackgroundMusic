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
//  GooglePlayMusicDesktopPlayer.js
//  BGMApp
//
//  Copyright © 2019 Kyle Neideck
//

// The specification for GPMDP's API:
// https://github.com/MarshallOfSound/Google-Play-Music-Desktop-Player-UNOFFICIAL-/blob/master/docs/PlaybackAPI_WebSocket.md

try {

window._log = msg => {
    window.webkit.messageHandlers.log.postMessage(msg);
};

// Global JS error handler.
window.onerror = (msg, url, line, col, error) => {
    let extra = !col ? '' : '\nColumn: ' + col;
    extra += !error ? '' : '\nError: ' + error;

    // TODO: I'm not sure this log message is ever actually useful.
    window._log('Error: ' + msg + '\nURL: ' + url + '\nLine: ' + line + extra);

    window.webkit.messageHandlers.error.postMessage(error);
};

// Send a JSON message to GPMDP.
//
// If we're connecting, this function will return immediately and the message will be sent after we
// finish connecting. Logs an error and returns if window.connect() hasn't been called yet.
window._sendJSON = json => {
    if (window._wsPromise) {
        window._wsPromise.then(() => {
            window._sendJSONImmediate(json);
        }).catch(error => {
            // TODO: Is there anything else we can do? Retries?
            window._log('Error sending JSON: ' + JSON.stringify(error));
        });
    } else {
        window._log('Error: No WebSocket promise. Discarding JSON message: ' +
                    JSON.stringify(json));
    }
};

// Send a JSON message to GPMDP, but don't wait if we're in the process of connecting.
//
// Logs an error and returns if window.connect() hasn't been called yet. The authCode param is
// optional and only used to hide the code in log messages.
window._sendJSONImmediate = (json, authCode) => {
    let jsonStr = JSON.stringify(json);
    let jsonStrSanitized = authCode ? jsonStr.replace(authCode, "<private>") : jsonStr;

    if (window._ws) {
        window._log('Sending JSON: ' + jsonStrSanitized);
        window._ws.send(jsonStr);
    } else {
        window._log('Error: No WebSocket. Discarding JSON message: ' + jsonStrSanitized);
    }
};

// permanentAuthCode is optional. If this is the first time they've selected GPMDP, we won't have a
// permanent code yet.
window.connect = permanentAuthCode => {
    // Reset the connection state.
    window._requestID = 1;

    // Close the existing connection if we're already connected.
    window.disconnect();

    // Create the new connection.
    window._ws = new WebSocket('ws://localhost:5672');

    window._ws.onmessage = event => {
        // Pass the message along to BGMGooglePlayMusicDesktopPlayerConnection.
        let reply = JSON.parse(event.data);
        window.webkit.messageHandlers.gpmdp.postMessage(reply);
    };

    window._wsPromise = new Promise((resolve, reject) => {
        window._ws.onopen = () => {
            // Send GPMDP the initial connection message.
            if (permanentAuthCode) {
                window._log('Connecting with auth code');
                window.sendPermanentAuthCode(permanentAuthCode);
            } else {
                // Since we don't have an auth code, it will display a four-digit code and reply
                // telling us to ask the user to type it into Background Music.
                window._log('Connecting without auth code');

                window._sendJSONImmediate({
                    'namespace': 'connect',
                    'method': 'connect',
                    'arguments': ['Background Music']
                });
            }
        };

        window._ws.onerror = error => {
            // Report the error to BGMGooglePlayMusicDesktopPlayerConnection.
            window.webkit.messageHandlers.error.postMessage(error);
            // Reject the connection promise.
            reject(error);
        };

        // Store the function that resolves this promise. We resolve it after we finish
        // authenticating.
        window._resolveConnectionPromise = resolve;
    });
};

// Close the connection to GPMDP. Does nothing if we aren't connected.
window.disconnect = () => {
    if (window._ws) {
        window._log('Closing WebSocket');
        window._ws.close();
        window._ws = null;
    }
};

// Send an authentication code to GPMDP. To send a four-digit code (i.e. one entered by the user),
// call this directly. To send a permanent code received from GPMDP, use
// window.sendPermanentAuthCode().
//
// authCode should be percent-encoded.
window.sendAuthCode = authCode => {
    // Percent-decode the auth code string. We pass it percent-encoded just to make sure nothing in
    // it accidentally gets executed as Javascript.
    authCode = window.decodeURIComponent(authCode);

    window._sendJSONImmediate({
        'namespace': 'connect',
        'method': 'connect',
        'arguments': ['Background Music', authCode]
    }, authCode);
};

// Send a permanent authentication code, received from GPMDP previously, to GPMDP.
window.sendPermanentAuthCode = permanentAuthCode => {
    window._log('Sending permanent auth code');
    window.sendAuthCode(permanentAuthCode);
    // TODO: If the code is rejected, GPMDP will send us a connect message and we'll show the auth
    //       code dialog, but accepting the promise here means some messages we send might get
    //       ignored.
    window._resolveConnectionPromise();
};

// Ask GPMDP to send us its current playback state (playing, paused or stopped).
window.requestPlaybackState = () => {
    window._sendJSON({
        'namespace': 'playback',
        'method': 'getPlaybackState',
        // We don't send any other types of request, so the ID we send only needs to be unique.
        'requestID': window._requestID++
    });
};

// Tell GPMDP to toggle between playing and paused.
window.playPause = () => {
    window._sendJSON({
        'namespace': 'playback',
        'method': 'playPause'
    });
};

} catch (error) {
    window.webkit.messageHandlers.log.postMessage('Error: ' + JSON.stringify(error));
    window.webkit.messageHandlers.log.postMessage(JSON.stringify(error.stack));
    window.webkit.messageHandlers.error.postMessage(error);
}

// Return an empty string as returning some types can cause an error when this Javascript is loaded
// into the WKWebView.
""


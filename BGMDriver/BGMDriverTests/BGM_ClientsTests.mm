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
//  BGM_ClientsTests.mm
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//

// Unit Include
#include "BGM_Clients.h"

// Local Includes
#include "BGM_TestUtils.h"

// BGMDriver Includes
#include "BGM_Types.h"


static BGM_TaskQueue taskQueue;

static const AudioServerPlugInClientInfo client1Info = {
    /* mClientID = */ 11,
    /* mProcessID = */ 1181,
    /* mIsNativeEndian = */ true,
    /* mBundleID = */ CFSTR("com.bearisdriving.BGMDriver.ClientOne")
};

static const AudioServerPlugInClientInfo client2Info = {
    /* mClientID = */ 22,
    /* mProcessID = */ 222,
    /* mIsNativeEndian = */ true,
    /* mBundleID = */ CFSTR("com.bearisdriving.BGMDriver.ClientTwo")
};

@interface BGM_ClientsTests : XCTestCase

@end

@implementation BGM_ClientsTests {
    BGM_Clients* clients;
}

- (void)setUp {
    [super setUp];
    
    clients = new BGM_Clients(kAudioObjectUnknown, &taskQueue);
}

- (void)tearDown {
    delete clients;
    
    [super tearDown];
}

- (void)testMusicPlayer {
    // Should be able to set the music player PID before clients have been added
    clients->SetMusicPlayer(441);
    XCTAssertEqual(clients->GetMusicPlayerProcessIDProperty(), 441);
    
    // IsMusicPlayerRT takes a client ID, not a PID
    XCTAssertFalse(clients->IsMusicPlayerRT(441));
    
    // Set the bundle ID
    clients->SetMusicPlayer("com.example.music.player");
    NSString* bundleID = (NSString*)CFBridgingRelease(clients->CopyMusicPlayerBundleIDProperty());
    XCTAssertEqualObjects(bundleID, @"com.example.music.player");
    
    // Setting the bundle ID unsets the PID, and vice versa
    XCTAssertEqual(clients->GetMusicPlayerProcessIDProperty(), 0);
    clients->SetMusicPlayer(2169);
    bundleID = (NSString*)CFBridgingRelease(clients->CopyMusicPlayerBundleIDProperty());
    XCTAssertEqualObjects(bundleID, @"");
    
    // Set client 2's bundle ID as the music player bundle ID
    clients->SetMusicPlayer(client2Info.mBundleID);
    
    // No client can be the music player yet because we haven't added any clients yet
    XCTAssertFalse(clients->IsMusicPlayerRT(client2Info.mClientID));
    
    // When we add client 2 it should become the music player because its bundle ID matches what we set above
    clients->AddClient(&client2Info);
    XCTAssert(clients->IsMusicPlayerRT(client2Info.mClientID));
    XCTAssertFalse(clients->IsMusicPlayerRT(client1Info.mClientID));
    
    // Check the bundle ID property matches client 2's
    CFStringRef bundleIDRef = clients->CopyMusicPlayerBundleIDProperty();
    XCTAssertEqual(bundleIDRef, client2Info.mBundleID);
    CFRelease(bundleIDRef);
    
    // Change the music player to client 1
    clients->AddClient(&client1Info);
    clients->SetMusicPlayer(client1Info.mProcessID);
    
    XCTAssert(clients->IsMusicPlayerRT(client1Info.mClientID));
    XCTAssertFalse(clients->IsMusicPlayerRT(client2Info.mClientID));
    
    // The music player should be unset after removing the music player as a client
    clients->RemoveClient(client1Info.mClientID);
    XCTAssertFalse(clients->IsMusicPlayerRT(client1Info.mClientID));
    
    // ...but the music player PID shouldn't change
    XCTAssertEqual(clients->GetMusicPlayerProcessIDProperty(), client1Info.mProcessID);
}

- (void)testSetMusicPlayerInvalidPID {
    BGMShouldThrow<BGM_InvalidClientPIDException>(self, [=](){
        clients->SetMusicPlayer(-1);
    });
    
    BGMShouldThrow<BGM_InvalidClientPIDException>(self, [=](){
        clients->SetMusicPlayer(INT_MIN);
    });
}

@end


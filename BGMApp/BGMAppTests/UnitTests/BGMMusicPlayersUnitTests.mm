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
//  BGMMusicPlayersUnitTests.mm
//  BGMAppUnitTests
//
//  Copyright © 2016-2020 Kyle Neideck
//

// Unit include
#import "BGMMusicPlayers.h"

// BGM includes
#import "BGM_Types.h"
#import "BGMAudioDeviceManager.h"
#import "BGMiTunes.h"
#import "BGMDecibel.h"
#import "BGMSpotify.h"
#import "BGMVLC.h"

// Local includes
#import "BGM_TestUtils.h"
#import "MockAudioObject.h"
#import "MockAudioObjects.h"

// System includes
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>


// Note that the PublicUtility classes that we use to communicate with the HAL, CAHALAudioObject and
// CAHALAudioSystemObject, are also mocked. The unit tests are compiled with mock implementations:
// Mock_CAHALAudioObject.cpp and Mock_CAHALAudioSystemObject.cpp.

@interface BGMMockUserDefaults : BGMUserDefaults

@property NSUUID* selectedPlayerID;

@end

@implementation BGMMockUserDefaults

- (void) registerDefaults {
}

- (NSString* __nullable) selectedMusicPlayerID {
    return [self.selectedPlayerID UUIDString];
}

- (void) setSelectedMusicPlayerID:(NSString* __nullable)selectedMusicPlayerID {
    #pragma unused (selectedMusicPlayerID)
}

- (BOOL) autoPauseMusicEnabled {
    return YES;
}

- (void) setAutoPauseMusicEnabled:(BOOL)autoPauseMusicEnabled {
    #pragma unused (autoPauseMusicEnabled)
}

@end

// -------------------------------------------------------------------------------------------------

@interface BGMMockAudioDeviceManager : BGMAudioDeviceManager
@end

@implementation BGMMockAudioDeviceManager {
    BGMBackgroundMusicDevice bgmDevice;
}

- (BGMBackgroundMusicDevice) bgmDevice {
    return bgmDevice;
}

@end

// -------------------------------------------------------------------------------------------------

@interface BGMMusicPlayersUnitTests : XCTestCase
@end

@implementation BGMMusicPlayersUnitTests {
    BGMAudioDeviceManager* devices;
    BGMMockUserDefaults* defaults;
    
    NSUUID* spotifyID;
    NSUUID* vlcID;
}

- (void) setUp {
    [super setUp];

    // Mock BGMDevice.
    MockAudioObjects::CreateMockDevice(kBGMDeviceUID);
    MockAudioObjects::CreateMockDevice(kBGMDeviceUID_UISounds);

    devices = [BGMMockAudioDeviceManager new];
    defaults = [BGMMockUserDefaults new];
    
    // These are the IDs hardcoded in BGMSpotify and BGMVLC.
    spotifyID = [[NSUUID alloc] initWithUUIDString:@"EC2A907F-8515-4687-9570-1BF63176E6D8"];
    vlcID = [[NSUUID alloc] initWithUUIDString:@"5226F4B9-C740-4045-A273-4B8EABC0E8FC"];
}

- (void) tearDown {
    [super tearDown];
    MockAudioObjects::DestroyMocks();
}

- (void) testNoSelectedMusicPlayerStored_iTunesDefault {
    // Test the case where the user has never changed the music player preference.
    
    // Test with iTunes as the default.
    BGMMusicPlayers* players = [[BGMMusicPlayers alloc] initWithAudioDevices:devices
                                                        defaultMusicPlayerID:[BGMiTunes sharedMusicPlayerID]
                                                          musicPlayerClasses:@[ BGMiTunes.class, BGMVLC.class ]
                                                                userDefaults:defaults];
    
    XCTAssertEqual(players.musicPlayers.count, 2);
    
    for (id<BGMMusicPlayer> player in players.musicPlayers) {
        XCTAssertTrue([player isKindOfClass:BGMiTunes.class] || [player isKindOfClass:BGMVLC.class]);
    }
    
    XCTAssertEqualObjects(players.selectedMusicPlayer.musicPlayerID, [BGMiTunes sharedMusicPlayerID]);
    XCTAssertEqualObjects(players.selectedMusicPlayer.name, @"iTunes");
}


- (void) testNoSelectedMusicPlayerStored_vlcDefault {
    // Test the case where the user has never changed the music player preference.

    // Test with VLC as the default.
    BGMMusicPlayers* players =
            [[BGMMusicPlayers alloc] initWithAudioDevices:devices
                                     defaultMusicPlayerID:vlcID
                                       musicPlayerClasses:@[ BGMiTunes.class,
                                                             BGMVLC.class,
                                                             BGMDecibel.class ]
                                             userDefaults:defaults];
    
    XCTAssertEqual(players.musicPlayers.count, 3);
    
    for (id<BGMMusicPlayer> player in players.musicPlayers) {
        XCTAssertTrue([player isKindOfClass:BGMiTunes.class] ||
                      [player isKindOfClass:BGMVLC.class] ||
                      [player isKindOfClass:BGMDecibel.class]);
    }
    
    XCTAssertEqualObjects(players.selectedMusicPlayer.musicPlayerID, vlcID);
    XCTAssertEqualObjects(players.selectedMusicPlayer.name, @"VLC");
}

- (void) testSelectedMusicPlayerInUserDefaults {
    defaults.selectedPlayerID = spotifyID;
    
    BGMMusicPlayers* players = [[BGMMusicPlayers alloc] initWithAudioDevices:devices
                                                        defaultMusicPlayerID:[BGMiTunes sharedMusicPlayerID]
                                                          musicPlayerClasses:@[ BGMiTunes.class,
                                                                                BGMVLC.class,
                                                                                BGMSpotify.class ]
                                                                userDefaults:defaults];
    
    XCTAssertEqual(players.musicPlayers.count, 3);
    
    XCTAssertEqualObjects(players.selectedMusicPlayer.musicPlayerID, spotifyID);
    XCTAssertEqualObjects(players.selectedMusicPlayer.name, @"Spotify");
}

- (void) testUnrecognizedSelectedMusicPlayerInUserDefaults {
    // If there's an unrecognized ID in user defaults, the default music player should be selected.
    defaults.selectedPlayerID = [[NSUUID alloc] initWithUUIDString:@"11111111-1111-1111-0000-000000000000"];
    
    // This initializer sets iTunes as the default music player and adds all the other music players.
    BGMMusicPlayers* players = [[BGMMusicPlayers alloc] initWithAudioDevices:devices
                                                                userDefaults:defaults];
    
    XCTAssert(players.musicPlayers.count >= 6);
    
    XCTAssertEqualObjects(players.selectedMusicPlayer.musicPlayerID, [BGMiTunes sharedMusicPlayerID]);
    XCTAssertEqualObjects(players.selectedMusicPlayer.name, @"iTunes");
}

- (void) testSelectedMusicPlayerInBGMDeviceProperties {
    // When it doesn't find a selected music player in user defaults, it should check BGMDevice's music
    // player properties.
    
    [devices bgmDevice].SetMusicPlayerBundleID(CFSTR("org.videolan.vlc"));
    
    BGMMusicPlayers* players = [[BGMMusicPlayers alloc] initWithAudioDevices:devices
                                                                userDefaults:defaults];
    
    XCTAssert(players.musicPlayers.count >= 6);
    
    XCTAssertEqualObjects(players.selectedMusicPlayer.musicPlayerID, vlcID);
    XCTAssertEqualObjects(players.selectedMusicPlayer.name, @"VLC");
}

// TODO: Test setting the selectedMusicPlayer property

@end


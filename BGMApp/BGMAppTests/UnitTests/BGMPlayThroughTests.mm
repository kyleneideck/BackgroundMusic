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
//  BGMPlayThroughTests.mm
//  BGMAppUnitTests
//
//  Copyright © 2020 Kyle Neideck
//

// Unit Include
#import "BGMPlayThrough.h"

// Local Includes
#import "MockAudioDevice.h"
#import "MockAudioObjects.h"

// BGM Includes
#import "BGM_Types.h"
#import "BGMAudioDevice.h"

// STL Includes
#import <memory>

// System Includes
#import <XCTest/XCTest.h>


@interface BGMPlayThroughTests : XCTestCase

@end

@implementation BGMPlayThroughTests {
    BGMAudioDevice inputDevice;
    BGMAudioDevice outputDevice;

    // The unit tests use mock implementations of CAHALAudioObject and CAHALAudioDevice, which are
    // the superclasses of BGMAudioDevice. When BGMPlayThrough calls methods on inputDevice or
    // outputDevice, some of them will update these objects.
    std::shared_ptr<MockAudioDevice> mockInputDevice;
    std::shared_ptr<MockAudioDevice> mockOutputDevice;
}

- (void) setUp {
    [super setUp];

    // Set up the mocks.
    mockInputDevice = MockAudioObjects::CreateMockDevice(kBGMDeviceUID);
    mockOutputDevice = MockAudioObjects::CreateMockDevice("Mock Output Device");

    inputDevice = BGMAudioDevice(mockInputDevice->GetObjectID());
    outputDevice = BGMAudioDevice(mockOutputDevice->GetObjectID());
}

- (void) tearDown {
    [super tearDown];
    MockAudioObjects::DestroyMocks();
}

- (void) testActivate {
    // Set the mock output device's sample rate and IO buffer size.
    outputDevice.SetNominalSampleRate(12345.0);
    outputDevice.SetIOBufferSize(123);

    // Create an instance and activate it.
    BGMPlayThrough playThrough(inputDevice, outputDevice);
    playThrough.Activate();

    // It should set the input device's sample rate and IO buffer size to match the output device.
    XCTAssertEqual(12345.0, inputDevice.GetNominalSampleRate());
    XCTAssertEqual(123, inputDevice.GetIOBufferSize());

    // It should add the property listeners it needs.
    std::set<AudioObjectPropertySelector> expectedProperties {
            kAudioDevicePropertyDeviceIsRunning,
            kAudioDeviceProcessorOverload,
            kAudioDeviceCustomPropertyDeviceIsRunningSomewhereOtherThanBGMApp
    };

    XCTAssertEqual(expectedProperties, mockInputDevice->mPropertiesWithListeners);
}

- (void) testDeactivate {
    BGMPlayThrough playThrough(inputDevice, outputDevice);

    playThrough.Activate();
    playThrough.Deactivate();

    // It should remove the property listeners added by Activate.
    XCTAssert(mockInputDevice->mPropertiesWithListeners.empty());
}

@end


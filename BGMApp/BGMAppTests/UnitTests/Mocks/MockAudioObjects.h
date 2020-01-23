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
//  MockAudioObjects.h
//  BGMAppUnitTests
//
//  Copyright © 2020 Kyle Neideck
//

#ifndef BGMAppUnitTests__MockAudioObjects
#define BGMAppUnitTests__MockAudioObjects

// Local Includes
#include "MockAudioObject.h"
#include "MockAudioDevice.h"

// STL Includes
#include <map>
#include <memory>
#include <string>

// System Includes
#include <CoreAudio/CoreAudio.h>


class MockAudioObjects
{

public:
    /*!
     * Create a mock audio device in the mock CoreAudio HAL.
     *
     * The mock device will then be accessible using GetAudioObject and GetAudioDevice. The
     * Mock_CAHAL* implementations will access the mock device when they query the mock HAL.
     *
     * Unit tests can check the mock device to verify the code they're testing has called the mocked
     * CAHAL classes correctly. They can also modify the mock device to control the Mock_CAHAL*
     * implementations, e.g. to have CAHALAudioDevice::IsAlive return false so the test can cover
     * the case where a device is being removed from the system.
     *
     * @param inUID The UID string to give the device. The UID is a persistent token used to
     *              identify a particular audio device across boot sessions.
     * @return The mock device.
     */
    static std::shared_ptr<MockAudioDevice> CreateMockDevice(const std::string& inUID);

    /*!
     * Remove all mock audio objects from the mock HAL. (Currently, mock devices are the only mock
     * objects that can be created.)
     */
    static void DestroyMocks();

    /*! Get a mock audio object by its ID. */
    static std::shared_ptr<MockAudioObject> GetAudioObject(AudioObjectID inAudioObjectID);

    /*! Get a mock audio device by its ID. */
    static std::shared_ptr<MockAudioDevice> GetAudioDevice(AudioObjectID inAudioDeviceID);
    /*! Get a mock audio device by its UID. */
    static std::shared_ptr<MockAudioDevice> GetAudioDevice(const std::string& inUID);
    /*! Get a mock audio device by its UID. */
    static std::shared_ptr<MockAudioDevice> GetAudioDevice(CFStringRef inUID);

private:
    typedef std::map<AudioObjectID, std::shared_ptr<MockAudioDevice>> MockDeviceMap;
    typedef std::map<std::string, std::shared_ptr<MockAudioDevice>> MockDeviceMapByUID;

    static std::shared_ptr<MockAudioDevice> GetAudioDeviceOrNull(AudioObjectID inAudioDeviceID);

    /*! Maps IDs to mocked audio devices. */
    static MockDeviceMap sDevices;
    /*! Maps UIDs (ID strings) to mocked audio devices. */
    static MockDeviceMapByUID sDevicesByUID;

};

#endif /* BGMAppUnitTests__MockAudioObjects */


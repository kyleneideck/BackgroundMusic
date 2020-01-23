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
//  MockAudioObject.h
//  BGMAppUnitTests
//
//  Copyright © 2020 Kyle Neideck
//

#ifndef BGMAppUnitTests__MockAudioDevice
#define BGMAppUnitTests__MockAudioDevice

// Superclass Includes
#include "MockAudioObject.h"

// STL Includes
#include <string>


/*!
 * A mock audio device in our mock CoreAudio HAL. In the HAL's API class hierarchy, the base class
 * for audio devices, kAudioDeviceClassID, is the audio objects class, kAudioObjectClassID.
 *
 * The unit tests generally use instances of this class to verify the HAL is being queried correctly
 * and to control the responses that the code they're testing will receive from the mock HAL.
 */
class MockAudioDevice
:
        public MockAudioObject
{

public:
    MockAudioDevice(const std::string& inUID);

    /*!
     * @return This device's music player bundle ID property.
     * @throws If this device isn't a mock of BGMDevice.
     */
    CACFString GetPlayerBundleID() const;
    /*!
     * Set this device's music player bundle ID property.
     * @throws If this device isn't a mock of BGMDevice.
     */
    void SetPlayerBundleID(const CACFString& inPlayerBundleID);

    /*!
     * The device's UID. The UID is a persistent token used to identify a particular audio device
     * across boot sessions.
     */
    const std::string mUID;
    Float64 mNominalSampleRate;
    UInt32 mIOBufferSize;

private:
    CACFString mPlayerBundleID { "" };

};

#endif /* BGMAppUnitTests__MockAudioDevice */


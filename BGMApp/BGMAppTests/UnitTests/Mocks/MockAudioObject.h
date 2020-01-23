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

#ifndef BGMAppUnitTests__MockAudioObject
#define BGMAppUnitTests__MockAudioObject

// PublicUtility Includes
#include "CACFString.h"

// STL Includes
#include <set>

// System Includes
#include <CoreAudio/CoreAudio.h>


/*!
 * The base class for mock audio objects in our mock CoreAudio HAL. Maps to kAudioObjectClassID
 * (AudioHardwareBase.h) in the HAL's API class hierarchy.
 */
class MockAudioObject
{

public:
    MockAudioObject(AudioObjectID inAudioObjectID);
    virtual ~MockAudioObject() = default;

    AudioObjectID GetObjectID() const;

    /*!
     * The properties that callers have added listeners for (and haven't since removed). See
     * CAHALAudioObject::AddPropertyListener and CAHALAudioObject::RemovePropertyListener.
     */
    std::set<AudioObjectPropertySelector> mPropertiesWithListeners;

private:
    AudioObjectID mAudioObjectID;

};

#endif /* BGMAppUnitTests__MockAudioObject */


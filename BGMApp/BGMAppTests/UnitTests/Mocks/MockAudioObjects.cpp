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
//  MockAudioObjects.cpp
//  BGMAppUnitTests
//
//  Copyright © 2020 Kyle Neideck
//

// Self Include
#include "MockAudioObjects.h"

// PublicUtility Includes
#include "CACFString.h"


// static
MockAudioObjects::MockDeviceMap MockAudioObjects::sDevices;

// static
MockAudioObjects::MockDeviceMapByUID MockAudioObjects::sDevicesByUID;

// static
std::shared_ptr<MockAudioDevice> MockAudioObjects::CreateMockDevice(const std::string& inUID)
{
    std::shared_ptr<MockAudioDevice> mockDevice = std::make_shared<MockAudioDevice>(inUID);

    sDevices.insert(MockDeviceMap::value_type(mockDevice->GetObjectID(), mockDevice));
    sDevicesByUID.insert(MockDeviceMapByUID::value_type(inUID, mockDevice));

    return mockDevice;
}

// static
void MockAudioObjects::DestroyMocks()
{
    sDevices.clear();
}

// static
std::shared_ptr<MockAudioObject> MockAudioObjects::GetAudioObject(AudioObjectID inAudioObjectID)
{
    auto device = GetAudioDeviceOrNull(inAudioObjectID);

    if(device)
    {
        return device;
    }

    // Devices are the only audio objects we currently mock.

    // Tests have to create mocks for all of the audio objects they expect the code they test to
    // access. They should fail if it accesses any others.
    throw "Mock audio object not found.";
}

// static
std::shared_ptr<MockAudioDevice> MockAudioObjects::GetAudioDevice(AudioObjectID inAudioObjectID)
{
    auto device = GetAudioDeviceOrNull(inAudioObjectID);

    if(device)
    {
        return device;
    }

    // Tests have to create mocks for all of the audio devices they expect the code they test to
    // access. They should fail if it accesses any others.
    throw "Mock audio device not found.";
}

// static
std::shared_ptr<MockAudioDevice> MockAudioObjects::GetAudioDevice(CFStringRef inUID)
{
    // Convert inUID to a std::string.
    UInt32 uidCStringLen = CACFString::GetStringByteLength(inUID) + 1;
    char uidCString[uidCStringLen];
    CACFString::GetCString(inUID, uidCString, uidCStringLen);
    std::string uid = std::string(uidCString);

    return GetAudioDevice(uid);
}

// static
std::shared_ptr<MockAudioDevice> MockAudioObjects::GetAudioDevice(const std::string& inUID)
{
    auto device = sDevicesByUID.find(inUID);

    if(device != sDevicesByUID.end())
    {
        return device->second;
    }

    return nullptr;
}

// static
std::shared_ptr<MockAudioDevice>
MockAudioObjects::GetAudioDeviceOrNull(AudioObjectID inAudioObjectID)
{
    auto device = sDevices.find(inAudioObjectID);

    if(device != sDevices.end())
    {
        return device->second;
    }

    return nullptr;
}


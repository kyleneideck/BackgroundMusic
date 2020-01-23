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
//  MockAudioObject.cpp
//  BGMAppUnitTests
//
//  Copyright © 2020 Kyle Neideck
//

// Self Include
#include "MockAudioObject.h"


MockAudioObject::MockAudioObject(AudioObjectID inAudioObjectID)
:
        mAudioObjectID(inAudioObjectID)
{
}

AudioObjectID MockAudioObject::GetObjectID() const
{
    return mAudioObjectID;
}


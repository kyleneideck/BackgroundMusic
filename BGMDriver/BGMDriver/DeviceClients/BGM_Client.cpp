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
//  BGM_Client.cpp
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//

// Self Include
#include "BGM_Client.h"


BGM_Client::BGM_Client(const AudioServerPlugInClientInfo* inClientInfo)
:
    mClientID(inClientInfo->mClientID),
    mProcessID(inClientInfo->mProcessID),
    mIsNativeEndian(inClientInfo->mIsNativeEndian),
    mBundleID(inClientInfo->mBundleID)
{
    // The bundle ID ref we were passed is only valid until our plugin returns control to the HAL, so we need to retain
    // it. (CACFString will handle the rest of its ownership/destruction.)
    if(inClientInfo->mBundleID != NULL)
    {
        CFRetain(inClientInfo->mBundleID);
    }
}

void    BGM_Client::Copy(const BGM_Client& inClient)
{
    mClientID = inClient.mClientID;
    mProcessID = inClient.mProcessID;
    mBundleID = inClient.mBundleID;
    mIsNativeEndian = inClient.mIsNativeEndian;
    mDoingIO = inClient.mDoingIO;
    mIsMusicPlayer = inClient.mIsMusicPlayer;
    mRelativeVolume = inClient.mRelativeVolume;
    mPanPosition = inClient.mPanPosition;
}


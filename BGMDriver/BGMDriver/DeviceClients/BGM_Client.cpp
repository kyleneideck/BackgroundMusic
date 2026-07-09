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

bool    BGM_Client::BundleIDMatchesMusicPlayer(const CACFString& inMusicPlayerBundleID) const
{
    // An unset (empty) music player bundle ID never matches anything.
    if(!mBundleID.IsValid() ||
       !inMusicPlayerBundleID.IsValid() ||
       CFStringGetLength(inMusicPlayerBundleID.GetCFString()) == 0)
    {
        return false;
    }

    if(mBundleID == inMusicPlayerBundleID)
    {
        return true;
    }

    // Match child bundle IDs by prefix, e.g. "com.brave.Browser.helper" for a music player set to
    // "com.brave.Browser". The trailing dot ensures we only match actual children and not unrelated
    // bundle IDs that happen to share a prefix (e.g. "com.brave.BrowserBeta").
    CFStringRef theParentPrefix =
        CFStringCreateWithFormat(NULL, NULL, CFSTR("%@."), inMusicPlayerBundleID.GetCFString());

    bool theResult = (theParentPrefix != NULL) && mBundleID.StartsWith(theParentPrefix);

    if(theParentPrefix != NULL)
    {
        CFRelease(theParentPrefix);
    }

    return theResult;
}

void    BGM_Client::Copy(const BGM_Client& inClient)
{
    mClientID = inClient.mClientID;
    mProcessID = inClient.mProcessID;
    mBundleID = inClient.mBundleID;
    mIsNativeEndian = inClient.mIsNativeEndian;
    mIsMusicPlayer = inClient.mIsMusicPlayer;
    mRelativeVolume = inClient.mRelativeVolume;
    mPanPosition = inClient.mPanPosition;
}


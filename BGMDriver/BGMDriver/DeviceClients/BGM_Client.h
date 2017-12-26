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
//  BGM_Client.h
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//

#ifndef __BGMDriver__BGM_Client__
#define __BGMDriver__BGM_Client__

// PublicUtility Includes
#include "CACFString.h"

// System Includes
#include <CoreAudio/AudioServerPlugIn.h>


#pragma clang assume_nonnull begin

//==================================================================================================
//	BGM_Client
//
//  Client meaning a client (of the host) of the BGMDevice, i.e. an app registered with the HAL,
//  generally so it can do IO at some point.
//==================================================================================================

class BGM_Client
{
    
public:
                                  BGM_Client() = default;
                                  BGM_Client(const AudioServerPlugInClientInfo* inClientInfo);
                                  ~BGM_Client() = default;
                                  BGM_Client(const BGM_Client& inClient) { Copy(inClient); };
                                  BGM_Client& operator=(const BGM_Client& inClient) { Copy(inClient); return *this; }
    
private:
    void                          Copy(const BGM_Client& inClient);
    
public:
    // These fields are duplicated from AudioServerPlugInClientInfo (except the mBundleID CFStringRef is
    // wrapped in a CACFString here).
    UInt32                        mClientID;
    pid_t                         mProcessID;
    Boolean                       mIsNativeEndian = true;
    CACFString                    mBundleID;
    
    // Becomes true when the client triggers the plugin host to call StartIO or to begin
    // kAudioServerPlugInIOOperationThread, and false again on StopIO or when
    // kAudioServerPlugInIOOperationThread ends
    bool                          mDoingIO = false;
    
    // True if BGMApp has set this client as belonging to the music player app
    bool                          mIsMusicPlayer = false;
    
    // The client's volume relative to other clients. In the range [0.0, 4.0], defaults to 1.0 (unchanged).
    // mRelativeVolumeCurve is applied to this value when it's set.
    Float32                       mRelativeVolume = 1.0;
    
    // The client's pan position, in the range [-100, 100] where -100 is left and 100 is right
    SInt32                        mPanPosition = 0;
    
};

#pragma clang assume_nonnull end

#endif /* __BGMDriver__BGM_Client__ */


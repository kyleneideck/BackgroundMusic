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
//  BGMVolumeChangeListener.h
//  BGMApp
//
//  Copyright © 2019 Kyle Neideck
//

// Local Includes
#include "BGMBackgroundMusicDevice.h"

// PublicUtility Includes
#import "CAPropertyAddress.h"

// STL Includes
#include <functional>

// System Includes
#include <CoreAudio/CoreAudio.h>


#pragma clang assume_nonnull begin

class BGMVolumeChangeListener
{

public:
    /*!
     * @param device Listens for notifications about this device.
     * @param handler The function to call when the device's volume (or mute) changes. Called on the
     *                main queue.
     */
    BGMVolumeChangeListener(BGMAudioDevice device, std::function<void(void)> handler);
    virtual ~BGMVolumeChangeListener();
    BGMVolumeChangeListener(const BGMVolumeChangeListener&) = delete;
    BGMVolumeChangeListener& operator=(const BGMVolumeChangeListener&) = delete;

private:
    AudioObjectPropertyListenerBlock     mListenerBlock;
    BGMAudioDevice                       mDevice;

};

#pragma clang assume_nonnull end


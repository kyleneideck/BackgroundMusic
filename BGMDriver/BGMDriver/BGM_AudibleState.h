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
//  BGM_AudibleState.h
//  BGMDriver
//
//  Copyright © 2016, 2017 Kyle Neideck
//
//  Inspects a stream of audio data and reports whether it's silent, silent except for the user's
//  music player, or audible.
//
//  See kAudioDeviceCustomPropertyDeviceAudibleState and the BGMDeviceAudibleState enum in
//  BGM_Types.h for more info.
//
//  Not thread-safe.
//

#ifndef BGMDriver__BGM_AudibleState
#define BGMDriver__BGM_AudibleState

// Local Includes
#include "BGM_Types.h"

// System Includes
#include <MacTypes.h>


#pragma clang assume_nonnull begin

class BGM_AudibleState
{

public:
                                BGM_AudibleState();

    /*!
     @return The current audible state of the device, to be used as the value of the
             kAudioDeviceCustomPropertyDeviceAudibleState property.
     */
    BGMDeviceAudibleState       GetState() const noexcept;

    /*! Set the audible state back to kBGMDeviceIsSilent and ignore all previous IO. */
    void                        Reset() noexcept;
    
    /*!
     Read an audio buffer sent by a single device client (i.e. a process playing audio) and update
     the audible state. The update will only affect the return value of GetState after the next
     call to UpdateWithMixedIO, when all IO for the cycle has been read.

     Real-time safe. Not thread safe.
     */
    void                        UpdateWithClientIO(bool inClientIsMusicPlayer,
                                                   UInt32 inIOBufferFrameSize,
                                                   Float64 inOutputSampleTime,
                                                   const Float32* inBuffer);
    /*!
     Read a fully mixed audio buffer and update the audible state. All client (unmixed) buffers for
     the same cycle must be read with UpdateWithClientIO before calling this function.

     Real-time safe. Not thread safe.

     @return True if the audible state changed.
     */
    bool                        UpdateWithMixedIO(UInt32 inIOBufferFrameSize,
                                                  Float64 inOutputSampleTime,
                                                  const Float32* inBuffer);

private:
    bool                        RecalculateState(Float64 inEndFrameSampleTime);

    static bool                 BufferIsAudible(UInt32 inIOBufferFrameSize,
                                                const Float32* inBuffer);

private:
    BGMDeviceAudibleState       mState;

    struct
    {
        Float64                 latestAudibleNonMusic;
        Float64                 latestSilent;
        Float64                 latestAudibleMusic;
        Float64                 latestSilentMusic;
    }                           mSampleTimes;

};

#pragma clang assume_nonnull end

#endif /* BGMDriver__BGM_AudibleState */


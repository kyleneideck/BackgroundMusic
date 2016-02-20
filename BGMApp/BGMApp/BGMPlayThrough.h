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
//  BGMPlayThrough.h
//  BGMApp
//
//  Copyright © 2016 Kyle Neideck
//
//  Reads audio from an input device and immediately writes it to an output device. We currently use this class with the input
//  device always set to BGMDevice and the output device set to the one selected in the preferences menu.
//
//  Apple's CAPlayThrough sample code (https://developer.apple.com/library/mac/samplecode/CAPlayThrough/Introduction/Intro.html)
//  has a similar class, but I couldn't get it fast enough to use here. Soundflower also has a similar class
//  (https://github.com/mattingalls/Soundflower/blob/master/SoundflowerBed/AudioThruEngine.h) that seems to be based on Apple
//  sample code from 2004. This class's main addition is pausing playthrough when idle to save CPU.
//
//  This class will hopefully not be needed after CoreAudio's aggregate devices get support for controls, which is planned for
//  a future release.
//

#ifndef __BGMApp__BGMPlayThrough__
#define __BGMApp__BGMPlayThrough__

// PublicUtility Includes
#include "CARingBuffer.h"
#include "CAHALAudioDevice.h"
#include "CAMutex.h"


#pragma clang assume_nonnull begin

class BGMPlayThrough
{

public:
                        BGMPlayThrough(CAHALAudioDevice inInputDevice, CAHALAudioDevice inOutputDevice);
                        ~BGMPlayThrough();
                        // Disallow copying
                        BGMPlayThrough(const BGMPlayThrough&) = delete;
                        BGMPlayThrough& operator=(const BGMPlayThrough&) = delete;
                        // Move constructor/assignment
                        BGMPlayThrough(BGMPlayThrough&& inPlayThrough) { Swap(inPlayThrough); }
                        BGMPlayThrough& operator=(BGMPlayThrough&& inPlayThrough) { Swap(inPlayThrough); return *this; }

#ifdef __OBJC__
                        // Only intended as a convenience for Objective-C instance vars
                        BGMPlayThrough() { }
#endif
    
private:
    void                Swap(BGMPlayThrough& inPlayThrough);
    
    void                Activate();
    void                Deactivate();
    
    void                AllocateBuffer();
    
    static bool         IsBGMDevice(CAHALAudioDevice inDevice);

    void                CreateIOProcs();
    void                DestroyIOProcs();
	
public:
    OSStatus            Start();
    
private:
    OSStatus            Pause();
    void                PauseIfIdle();
    static OSStatus     BGMDeviceListenerProc(AudioObjectID inObjectID,
                                              UInt32 inNumberAddresses,
                                              const AudioObjectPropertyAddress* inAddresses,
                                              void* __nullable inClientData);
    static bool         RunningSomewhereOtherThanBGMApp(const CAHALAudioDevice inBGMDevice);

    static OSStatus     InputDeviceIOProc(AudioObjectID           inDevice,
                                          const AudioTimeStamp*   inNow,
                                          const AudioBufferList*  inInputData,
                                          const AudioTimeStamp*   inInputTime,
                                          AudioBufferList*        outOutputData,
                                          const AudioTimeStamp*   inOutputTime,
                                          void* __nullable        inClientData);
    static OSStatus     OutputDeviceIOProc(AudioObjectID           inDevice,
                                           const AudioTimeStamp*   inNow,
                                           const AudioBufferList*  inInputData,
                                           const AudioTimeStamp*   inInputTime,
                                           AudioBufferList*        outOutputData,
                                           const AudioTimeStamp*   inOutputTime,
                                           void* __nullable        inClientData);
    static void         HandleRingBufferError(CARingBufferError err,
                                              const char* methodName,
                                              const char* callReturningErr);
    
private:
    CARingBuffer        mBuffer;
    
    AudioDeviceIOProcID mInputDeviceIOProcID;
    AudioDeviceIOProcID mOutputDeviceIOProcID;
    
    CAHALAudioDevice    mInputDevice { kAudioDeviceUnknown };
    CAHALAudioDevice    mOutputDevice { kAudioDeviceUnknown };
    
    CAMutex             mStateMutex { "Playthrough state" };
    
    bool                mActive = false;
    bool                mPlayingThrough = false;
    
    UInt64              mLastNotifiedIOStoppedOnBGMDevice;
    
    bool                mInputDeviceIOProcShouldStop = false;
    bool                mOutputDeviceIOProcShouldStop = false;

    // IOProc vars. (Should only be used inside IOProcs.)
    
    // The earliest/latest sample times seen by the IOProcs since starting playthrough. -1 for unset.
    Float64             mFirstInputSampleTime = -1;
    Float64             mLastInputSampleTime = -1;
    Float64             mLastOutputSampleTime = -1;
    
    // Subtract this from the output time to get the input time
    Float64             mInToOutSampleOffset;
    
};

#pragma clang assume_nonnull end

#endif /* __BGMApp__BGMPlayThrough__ */


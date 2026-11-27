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
//  BGMPlayThroughTests.mm
//  BGMAppUnitTests
//
//  Copyright © 2020, 2026 Kyle Neideck
//

// Unit Include
#import "BGMPlayThrough.h"

// Local Includes
#import "MockAudioDevice.h"
#import "MockAudioObjects.h"

// BGM Includes
#import "BGM_Types.h"
#import "BGMAudioDevice.h"

// STL Includes
#import <memory>
#import <cstdlib>
#import <cstring>
#import <cmath>   // For fabsf

// System Includes
#import <CoreAudio/CoreAudio.h>
#import <XCTest/XCTest.h>


@interface BGMPlayThroughTests : XCTestCase

@end

// Friend of BGMPlayThrough so the tests can reach its private members.
class BGMPlayThroughTestAccess
{
public:
    static bool IsRunningSomewhereOtherThanBGMApp(const BGMAudioDevice& inBGMDevice)
    {
        return BGMPlayThrough::IsRunningSomewhereOtherThanBGMApp(inBGMDevice);
    }

    static bool IsActive(const BGMPlayThrough& inPlayThrough)
    {
        return inPlayThrough.mActive;
    }

    static bool IsPlayingThrough(const BGMPlayThrough& inPlayThrough)
    {
        return inPlayThrough.mPlayingThrough;
    }

    static void SetInputIOProcState(BGMPlayThrough& inPlayThrough, BGMPlayThrough::IOState inState)
    {
        inPlayThrough.mInputDeviceIOProcState.store(inState);
    }

    static void SetOutputDeviceIOProcState(BGMPlayThrough& inPlayThrough,
                                           AudioObjectID inDeviceID,
                                           BGMPlayThrough::IOState inState)
    {
        for(auto& outputDevice : inPlayThrough.mOutputDevices)
        {
            if(outputDevice->device.GetObjectID() == inDeviceID)
            {
                outputDevice->state.store(inState);
            }
        }
    }

    /*!
     * Restores all IOProc states to Stopped.
     *
     * DriveIOProcs leaves the IOProcs in the Running state, simulating the HAL calling them. In
     * production the HAL would stop them through Stop() and a final IOProc call, but the mock's
     * StopIOProc is unimplemented (it aborts), so tests that drive the IOProcs must call this
     * before their BGMPlayThrough goes out of scope. Otherwise its destructor fails the
     * CheckIOProcsAreStopped assert in DestroyIOProcIDs.
     */
    static void StopIOProcs(BGMPlayThrough& inPlayThrough)
    {
        inPlayThrough.mInputDeviceIOProcState.store(BGMPlayThrough::IOState::Stopped);
        for(auto& outputDevice : inPlayThrough.mOutputDevices)
        {
            outputDevice->state.store(BGMPlayThrough::IOState::Stopped);
        }
    }

    /*! @return The device's gain, or -1 if the device isn't one of the playthrough's output devices. */
    static Float32 GainForOutputDevice(const BGMPlayThrough& inPlayThrough, AudioObjectID inDeviceID)
    {
        for(const auto& outputDevice : inPlayThrough.mOutputDevices)
        {
            if(outputDevice->device.GetObjectID() == inDeviceID)
            {
                return outputDevice->gain.load();
            }
        }
        return -1.0f;
    }

    /*!
     * @return True if the device's sample rate differs from the ring buffer's (i.e. it will be
     *         resampled), or false if the device isn't one of the playthrough's output devices.
     */
    static bool NeedsResamplingForOutputDevice(const BGMPlayThrough& inPlayThrough, AudioObjectID inDeviceID)
    {
        for(const auto& outputDevice : inPlayThrough.mOutputDevices)
        {
            if(outputDevice->device.GetObjectID() == inDeviceID)
            {
                return outputDevice->needsResampling;
            }
        }
        return false;
    }

    /*!
     * Calls the input and output IOProcs directly to move inFrames frames of audio through the
     * playthrough's ring buffer.
     *
     * The IOProcs' states are set to Running first (so they do their normal work instead of
     * returning early or stopping themselves). The input IOProc stores inFrames frames of
     * inInputValue into the ring buffer at sample time 0, and then the output IOProc for
     * inOutputDeviceID fetches them (also at sample time 0).
     *
     * inFetchedABL is filled with the fetched audio. It's pre-filled with garbage values first,
     * so tests can check that the IOProc overwrites it. The caller must free its mData.
     */
    static OSStatus DriveIOProcs(BGMPlayThrough& inPlayThrough,
                                 const BGMAudioDevice& inInputDevice,
                                 AudioObjectID inOutputDeviceID,
                                 UInt32 inFrames,
                                 Float32 inInputValue,
                                 AudioBufferList* inFetchedABL)
    {
        SetInputIOProcState(inPlayThrough, BGMPlayThrough::IOState::Running);

        // The IOProc parameters that the implementations don't use must still be non-null, so we
        // pass valid dummy values for them.
        AudioTimeStamp dummyTime;
        memset(&dummyTime, 0, sizeof(dummyTime));
        AudioBufferList dummyABL;
        memset(&dummyABL, 0, sizeof(dummyABL));

        AudioBufferList inABL = MakeStereoFloatABL(inFrames, inInputValue);
        AudioTimeStamp inTime;
        memset(&inTime, 0, sizeof(inTime));
        OSStatus err = BGMPlayThrough::InputDeviceIOProc(inInputDevice.GetObjectID(),
                                                         &dummyTime,
                                                         &inABL,
                                                         &inTime,
                                                         &dummyABL,
                                                         &dummyTime,
                                                         &inPlayThrough);
        free(inABL.mBuffers[0].mData);

        if(err != noErr)
        {
            return err;
        }

        SetOutputDeviceIOProcState(inPlayThrough, inOutputDeviceID, BGMPlayThrough::IOState::Running);

        *inFetchedABL = MakeStereoFloatABL(inFrames, /* garbage = */ -1234.5f);

        AudioTimeStamp outTime;
        memset(&outTime, 0, sizeof(outTime));

        return BGMPlayThrough::OutputDeviceIOProc(inOutputDeviceID,
                                                  &dummyTime,
                                                  &dummyABL,
                                                  &dummyTime,
                                                  inFetchedABL,
                                                  &outTime,
                                                  &inPlayThrough);
    }

    /*!
     * Stores inInputFrames frames of inInputValue into the ring buffer, then fetches
     * inOutputFrames frames through the output device's IOProc.
     *
     * Unlike DriveIOProcs (which stores and fetches the same number of frames), this stores more
     * input than it fetches, so a resampling device's AudioConverter never runs out of input while
     * producing the output window. For a DC input signal the resampled output should settle to the
     * input value scaled by the master and device gains (after the converter's short start-up
     * transient), so tests can check the level without depending on exact resampler output.
     *
     * inFetchedABL is pre-filled with garbage values (so tests can check the IOProc overwrote it)
     * and must have its mData freed by the caller.
     */
    static OSStatus DriveResampler(BGMPlayThrough& inPlayThrough,
                                   const BGMAudioDevice& inInputDevice,
                                   AudioObjectID inOutputDeviceID,
                                   UInt32 inInputFrames,
                                   UInt32 inOutputFrames,
                                   Float32 inInputValue,
                                   AudioBufferList* inFetchedABL)
    {
        SetInputIOProcState(inPlayThrough, BGMPlayThrough::IOState::Running);

        AudioTimeStamp dummyTime;
        memset(&dummyTime, 0, sizeof(dummyTime));
        AudioBufferList dummyABL;
        memset(&dummyABL, 0, sizeof(dummyABL));

        AudioBufferList inABL = MakeStereoFloatABL(inInputFrames, inInputValue);
        AudioTimeStamp inTime;
        memset(&inTime, 0, sizeof(inTime));
        OSStatus err = BGMPlayThrough::InputDeviceIOProc(inInputDevice.GetObjectID(),
                                                         &dummyTime,
                                                         &inABL,
                                                         &inTime,
                                                         &dummyABL,
                                                         &dummyTime,
                                                         &inPlayThrough);
        free(inABL.mBuffers[0].mData);

        if(err != noErr)
        {
            return err;
        }

        SetOutputDeviceIOProcState(inPlayThrough, inOutputDeviceID, BGMPlayThrough::IOState::Running);

        *inFetchedABL = MakeStereoFloatABL(inOutputFrames, /* garbage = */ -1234.5f);
        AudioTimeStamp outTime;
        memset(&outTime, 0, sizeof(outTime));

        return BGMPlayThrough::OutputDeviceIOProc(inOutputDeviceID,
                                                  &dummyTime,
                                                  &dummyABL,
                                                  &dummyTime,
                                                  inFetchedABL,
                                                  &outTime,
                                                  &inPlayThrough);
    }

private:
    /*!
     * @return An AudioBufferList of inFrames frames of 32-bit float stereo, with all the samples
     *         set to inValue. The caller must free the mData pointer.
     */
    static AudioBufferList MakeStereoFloatABL(UInt32 inFrames, Float32 inValue)
    {
        AudioBufferList abl;
        abl.mNumberBuffers = 1;
        abl.mBuffers[0].mNumberChannels = 2;
        abl.mBuffers[0].mDataByteSize = inFrames * 2 * sizeof(Float32);
        abl.mBuffers[0].mData = calloc(1, abl.mBuffers[0].mDataByteSize);
        Float32* samples = static_cast<Float32*>(abl.mBuffers[0].mData);
        for(UInt32 i = 0; i < inFrames * 2; i++)
        {
            samples[i] = inValue;
        }
        return abl;
    }
};

@implementation BGMPlayThroughTests {
    BGMAudioDevice inputDevice;
    BGMAudioDevice outputDevice;

    // The unit tests use mock implementations of CAHALAudioObject and CAHALAudioDevice, which are
    // the superclasses of BGMAudioDevice. When BGMPlayThrough calls methods on inputDevice or
    // outputDevice, some of them will update these objects.
    std::shared_ptr<MockAudioDevice> mockInputDevice;
    std::shared_ptr<MockAudioDevice> mockOutputDevice;
}

- (void) setUp {
    [super setUp];

    // Set up the mocks.
    mockInputDevice = MockAudioObjects::CreateMockDevice(kBGMDeviceUID);
    mockOutputDevice = MockAudioObjects::CreateMockDevice("Mock Output Device");

    inputDevice = BGMAudioDevice(mockInputDevice->GetObjectID());
    outputDevice = BGMAudioDevice(mockOutputDevice->GetObjectID());
}

- (void) tearDown {
    [super tearDown];
    MockAudioObjects::DestroyMocks();
}

- (void) testActivate {
    // Set the mock output device's sample rate and IO buffer size.
    outputDevice.SetNominalSampleRate(12345.0);
    outputDevice.SetIOBufferSize(123);

    // Create an instance and activate it.
    BGMPlayThrough playThrough(inputDevice, outputDevice);
    playThrough.Activate();

    // It should set the input device's sample rate and IO buffer size to match the output device.
    XCTAssertEqual(12345.0, inputDevice.GetNominalSampleRate());
    XCTAssertEqual(123, inputDevice.GetIOBufferSize());

    // Property notifications come through BGMAudioDeviceManager, so we don't expect BGMPlayThrough to register any listeners.
    XCTAssert(mockInputDevice->mPropertiesWithListeners.empty());
}

- (void) testActivate_MixedSampleRateOutputDevicesSucceeds {
    auto secondOutputMock = MockAudioObjects::CreateMockDevice("Mock Second Output Device");
    BGMAudioDevice secondOutput(secondOutputMock->GetObjectID());

    // Give the two output devices different sample rates. (The mock default is 44100.)
    outputDevice.SetNominalSampleRate(48000.0);

    BGMPlayThrough playThrough(inputDevice, { outputDevice, secondOutput });

    // Activating with output devices that have different sample rates should now succeed: the
    // device whose rate differs from the ring buffer's gets resampled (see the header comment).
    bool threw = false;
    try {
        playThrough.Activate();
    } catch (const CAException& e) {
        threw = true;
    }

    XCTAssertFalse(threw, @"Activate should succeed with output devices of different sample rates.");
    XCTAssertTrue(BGMPlayThroughTestAccess::IsActive(playThrough));

    // The ring buffer runs at the first output device's rate (48000), so that device plays the
    // samples directly while the 44100 device is resampled.
    XCTAssertFalse(BGMPlayThroughTestAccess::NeedsResamplingForOutputDevice(playThrough,
                                                                            outputDevice.GetObjectID()));
    XCTAssertTrue(BGMPlayThroughTestAccess::NeedsResamplingForOutputDevice(playThrough,
                                                                           secondOutput.GetObjectID()));
}

- (void) testOutputDeviceIOProc_ResamplesMixedRateDevice {
    auto secondOutputMock = MockAudioObjects::CreateMockDevice("Mock Second Output Device");
    BGMAudioDevice secondOutput(secondOutputMock->GetObjectID());

    // First output at 48000 (sets the ring buffer's rate), second at 44100 (needs resampling).
    outputDevice.SetNominalSampleRate(48000.0);

    BGMPlayThrough playThrough(inputDevice, { outputDevice, secondOutput });
    playThrough.Activate();

    XCTAssertTrue(BGMPlayThroughTestAccess::NeedsResamplingForOutputDevice(playThrough,
                                                                           secondOutput.GetObjectID()));

    // Drive 44100-rate output from 48000-rate input through the resampler.
    playThrough.SetMasterVolume(1.0f);
    playThrough.SetOutputDeviceGain(secondOutput.GetObjectID(), 0.5f);  // expected level: 0.5

    AudioBufferList outABL;
    OSStatus err = BGMPlayThroughTestAccess::DriveResampler(playThrough,
                                                             inputDevice,
                                                             secondOutput.GetObjectID(),
                                                             /* input frames = */ 2000,
                                                             /* output frames = */ 200,
                                                             /* input value = */ 1.0f,
                                                             &outABL);
    XCTAssertEqual(noErr, err);

    const UInt32 numOutFrames = 200;
    const Float32* outSamples = static_cast<const Float32*>(outABL.mBuffers[0].mData);
    const Float32 expected = 0.5f;  // 1.0 (input) * 1.0 (master) * 0.5 (gain)
    const Float32 tolerance = 0.1f;

    // The IOProc must have overwritten the pre-filled garbage values (i.e. it produced output
    // rather than leaving the buffer untouched).
    for(UInt32 i = 0; i < numOutFrames * 2; i++)
    {
        XCTAssertNotEqual(-1234.5f, outSamples[i], @"Sample %u should be overwritten", i);
    }

    // The input is a DC signal, so after the resampler's short start-up transient the output
    // should settle to the input value scaled by the gain. Check the settled (second half of the
    // output) portion rather than the first few samples.
    UInt32 withinTolerance = 0;
    const UInt32 tailStart = numOutFrames / 2;
    for(UInt32 i = tailStart * 2; i < numOutFrames * 2; i++)
    {
        if(fabsf(outSamples[i] - expected) <= tolerance)
        {
            withinTolerance++;
        }
    }
    XCTAssertGreaterThan((int)withinTolerance, (int)((numOutFrames - tailStart) * 2 * 90 / 100),
                         @"The resampled output should be close to the input scaled by the gain");

    BGMPlayThroughTestAccess::StopIOProcs(playThrough);

    free(outABL.mBuffers[0].mData);
}

- (void) testActivate_SameSampleRateOutputDevicesSucceeds {
    auto secondOutputMock = MockAudioObjects::CreateMockDevice("Mock Second Output Device");
    BGMAudioDevice secondOutput(secondOutputMock->GetObjectID());

    // Both output devices have the same (mock default) sample rate, so Activate should succeed.
    BGMPlayThrough playThrough(inputDevice, { outputDevice, secondOutput });

    bool threw = false;
    try {
        playThrough.Activate();
    } catch (const CAException& e) {
        threw = true;
    }

    XCTAssertFalse(threw, @"Activate should succeed when the output devices share a sample rate.");
    XCTAssertTrue(BGMPlayThroughTestAccess::IsActive(playThrough));
}

- (void) testDeactivate {
    BGMPlayThrough playThrough(inputDevice, outputDevice);

    playThrough.Activate();
    XCTAssertTrue(BGMPlayThroughTestAccess::IsActive(playThrough));

    playThrough.Deactivate();

    // Deactivate stops playthrough and tears down the IOProcs.
    XCTAssertFalse(BGMPlayThroughTestAccess::IsActive(playThrough));
    XCTAssertFalse(BGMPlayThroughTestAccess::IsPlayingThrough(playThrough));
}

- (void) testIsRunningSomewhereOtherThanBGMApp_WrongCFTypeReturnsFalse {
    mockInputDevice->SetRunningSomewhereOtherThanBGMAppProperty(CFSTR("not a boolean"));

    XCTAssertFalse(BGMPlayThroughTestAccess::IsRunningSomewhereOtherThanBGMApp(inputDevice));
}

- (void) testSetMasterVolume {
    BGMPlayThrough playThrough(inputDevice, outputDevice);

    XCTAssertEqual(1.0f, playThrough.GetMasterVolume());

    playThrough.SetMasterVolume(0.3f);
    XCTAssertEqual(0.3f, playThrough.GetMasterVolume());

    playThrough.SetMasterVolume(0.0f);
    XCTAssertEqual(0.0f, playThrough.GetMasterVolume());
}

- (void) testSetOutputDeviceGain {
    BGMPlayThrough playThrough(inputDevice, outputDevice);

    // Gains default to 1.0.
    XCTAssertEqual(1.0f, BGMPlayThroughTestAccess::GainForOutputDevice(playThrough,
                                                                       outputDevice.GetObjectID()));

    playThrough.SetOutputDeviceGain(outputDevice.GetObjectID(), 0.25f);
    XCTAssertEqual(0.25f, BGMPlayThroughTestAccess::GainForOutputDevice(playThrough,
                                                                        outputDevice.GetObjectID()));

    // Setting the gain of a device that isn't an output device should have no effect.
    playThrough.SetOutputDeviceGain(12345, 0.5f);
    XCTAssertEqual(0.25f, BGMPlayThroughTestAccess::GainForOutputDevice(playThrough,
                                                                        outputDevice.GetObjectID()));
}

- (void) testOutputDeviceIOProc_ScalesAudioByMasterAndGain {
    BGMPlayThrough playThrough(inputDevice, outputDevice);
    playThrough.Activate();

    playThrough.SetMasterVolume(0.5f);
    playThrough.SetOutputDeviceGain(outputDevice.GetObjectID(), 0.25f);

    AudioBufferList outABL;
    OSStatus err = BGMPlayThroughTestAccess::DriveIOProcs(playThrough,
                                                           inputDevice,
                                                           outputDevice.GetObjectID(),
                                                           /* frames = */ 100,
                                                           /* input = */ 1.0f,
                                                           &outABL);
    XCTAssertEqual(noErr, err);

    // Every output sample should be the input sample (1.0) scaled by master (0.5) times the
    // device's gain (0.25). 0.125 is exactly representable in binary, so we can compare exactly.
    const Float32* outSamples = static_cast<const Float32*>(outABL.mBuffers[0].mData);
    for(UInt32 i = 0; i < 200; i++)
    {
        XCTAssertEqual(0.125f, outSamples[i], @"Sample %u", i);
    }

    BGMPlayThroughTestAccess::StopIOProcs(playThrough);

    free(outABL.mBuffers[0].mData);
}

- (void) testOutputDeviceIOProc_ZeroMasterProducesSilence {
    BGMPlayThrough playThrough(inputDevice, outputDevice);
    playThrough.Activate();

    // Master volume 0 should silence the output even though the device's gain is 1.0 (the default).
    playThrough.SetMasterVolume(0.0f);

    AudioBufferList outABL;
    OSStatus err = BGMPlayThroughTestAccess::DriveIOProcs(playThrough,
                                                           inputDevice,
                                                           outputDevice.GetObjectID(),
                                                           /* frames = */ 100,
                                                           /* input = */ 1.0f,
                                                           &outABL);
    XCTAssertEqual(noErr, err);

    // The output ABL was pre-filled with garbage values by DriveIOProcs, so this also checks that
    // the IOProc overwrote it.
    const Float32* outSamples = static_cast<const Float32*>(outABL.mBuffers[0].mData);
    for(UInt32 i = 0; i < 200; i++)
    {
        XCTAssertEqual(0.0f, outSamples[i], @"Sample %u", i);
    }

    BGMPlayThroughTestAccess::StopIOProcs(playThrough);

    free(outABL.mBuffers[0].mData);
}

- (void) testOutputDeviceIOProc_IndependentGainsPerOutputDevice {
    auto secondOutputMock = MockAudioObjects::CreateMockDevice("Mock Second Output Device");
    BGMAudioDevice secondOutput(secondOutputMock->GetObjectID());

    BGMPlayThrough playThrough(inputDevice, { outputDevice, secondOutput });
    playThrough.Activate();

    playThrough.SetMasterVolume(0.5f);
    playThrough.SetOutputDeviceGain(outputDevice.GetObjectID(), 0.25f);   // 0.5 * 0.25 = 0.125
    playThrough.SetOutputDeviceGain(secondOutput.GetObjectID(), 0.5f);    // 0.5 * 0.5 = 0.25

    AudioBufferList firstABL;
    AudioBufferList secondABL;
    XCTAssertEqual(noErr, BGMPlayThroughTestAccess::DriveIOProcs(playThrough,
                                                                  inputDevice,
                                                                  outputDevice.GetObjectID(),
                                                                  /* frames = */ 100,
                                                                  /* input = */ 1.0f,
                                                                  &firstABL));
    XCTAssertEqual(noErr, BGMPlayThroughTestAccess::DriveIOProcs(playThrough,
                                                                  inputDevice,
                                                                  secondOutput.GetObjectID(),
                                                                  /* frames = */ 100,
                                                                  /* input = */ 1.0f,
                                                                  &secondABL));

    // Each output device should be scaled by the master volume times its own gain.
    const Float32* firstSamples = static_cast<const Float32*>(firstABL.mBuffers[0].mData);
    for(UInt32 i = 0; i < 200; i++)
    {
        XCTAssertEqual(0.125f, firstSamples[i], @"First device, sample %u", i);
    }
    const Float32* secondSamples = static_cast<const Float32*>(secondABL.mBuffers[0].mData);
    for(UInt32 i = 0; i < 200; i++)
    {
        XCTAssertEqual(0.25f, secondSamples[i], @"Second device, sample %u", i);
    }

    BGMPlayThroughTestAccess::StopIOProcs(playThrough);

    free(firstABL.mBuffers[0].mData);
    free(secondABL.mBuffers[0].mData);
}

- (void) testSetOutputDevices_PreservesGains {
    auto secondOutputMock = MockAudioObjects::CreateMockDevice("Mock Second Output Device");
    BGMAudioDevice secondOutput(secondOutputMock->GetObjectID());

    BGMPlayThrough playThrough(inputDevice, { outputDevice, secondOutput });
    playThrough.SetOutputDeviceGain(outputDevice.GetObjectID(), 0.25f);
    playThrough.SetOutputDeviceGain(secondOutput.GetObjectID(), 0.5f);

    // Re-set the output devices (in a different order). The devices' gains should be preserved.
    playThrough.SetOutputDevices({ secondOutput, outputDevice });

    XCTAssertEqual(0.25f, BGMPlayThroughTestAccess::GainForOutputDevice(playThrough,
                                                                        outputDevice.GetObjectID()));
    XCTAssertEqual(0.5f, BGMPlayThroughTestAccess::GainForOutputDevice(playThrough,
                                                                       secondOutput.GetObjectID()));
}

@end

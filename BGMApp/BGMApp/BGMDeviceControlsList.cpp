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
//  BGMDeviceControlsList.cpp
//  BGMApp
//
//  Copyright © 2017 Kyle Neideck
//

// Self Include
#include "BGMDeviceControlsList.h"

// Local Includes
#include "BGM_Types.h"
#include "BGM_Utils.h"

// PublicUtility Includes
#include "CAPropertyAddress.h"
#include "CACFArray.h"


#pragma clang assume_nonnull begin

static const SInt64 kToggleDeviceInitialDelay = 50 * NSEC_PER_MSEC;
static const SInt64 kToggleDeviceBackDelay    = 500 * NSEC_PER_MSEC;
static const SInt64 kDisableNullDeviceDelay   = 500 * NSEC_PER_MSEC;
static const SInt64 kDisableNullDeviceTimeout = 5000 * NSEC_PER_MSEC;

#pragma mark Construction/Destruction

BGMDeviceControlsList::BGMDeviceControlsList(AudioObjectID inBGMDevice,
                                             CAHALAudioSystemObject inAudioSystem)
:
    mBGMDevice(inBGMDevice),
    mAudioSystem(inAudioSystem)
{
    BGMAssert((mBGMDevice.IsBGMDevice() || mBGMDevice.GetObjectID() == kAudioObjectUnknown),
              "BGMDeviceControlsList::BGMDeviceControlsList: Given device is not BGMDevice");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
    mCanToggleDeviceOnSystem = (&dispatch_block_wait &&
                                &dispatch_block_cancel &&
                                &dispatch_block_testcancel &&
                                &dispatch_queue_attr_make_with_qos_class);
#pragma clang diagnostic pop
}

BGMDeviceControlsList::~BGMDeviceControlsList()
{
    CAMutex::Locker locker(mMutex);

    if(!mDeviceTogglingInitialised)
    {
        return;
    }

    if(mListenerQueue && mListenerBlock)
    {
        BGMLogAndSwallowExceptions("BGMDeviceControlsList::~BGMDeviceControlsList", ([&] {
            mAudioSystem.RemovePropertyListenerBlock(
                    CAPropertyAddress(kAudioHardwarePropertyDevices),
                    mListenerQueue,
                    mListenerBlock);
        }));
    }

    // If we're in the middle of toggling the default device, block until we've finished.
    if(mDisableNullDeviceBlock && mDeviceToggleState != ToggleState::NotToggling)
    {
        DebugMsg("BGMDeviceControlsList::~BGMDeviceControlsList: Waiting for device toggle");

        // Copy the reference so we can unlock the mutex and allow any remaining blocks to run.
        dispatch_block_t disableNullDeviceBlock = mDisableNullDeviceBlock;

        CAMutex::Unlocker unlocker(mMutex);

        // Note that if mDisableNullDeviceBlock is currently running this will return after it
        // finishes and if it's already run this will return immediately. So we don't have to
        // worry about ending up waiting for mDisableNullDeviceBlock when it isn't queued.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
        long timedOut = dispatch_block_wait(disableNullDeviceBlock, kDisableNullDeviceTimeout);
#pragma clang diagnostic pop

        if(timedOut)
        {
            LogWarning("BGMDeviceControlsList::~BGMDeviceControlsList: Device toggle timed out");
        }
    }

    mDeviceToggleState = ToggleState::NotToggling;

    DestroyBlock(mDeviceToggleBlock);
    DestroyBlock(mDeviceToggleBackBlock);
    DestroyBlock(mDisableNullDeviceBlock);

    if(mListenerBlock)
    {
        Block_release(mListenerBlock);
    }

    if(mListenerQueue)
    {
        dispatch_release(BGM_Utils::NN(mListenerQueue));
    }
}

#pragma mark Accessors

void    BGMDeviceControlsList::SetBGMDevice(AudioObjectID inBGMDeviceID)
{
    CAMutex::Locker locker(mMutex);

    mBGMDevice = inBGMDeviceID;

    BGMAssert(mBGMDevice.IsBGMDevice(),
              "BGMDeviceControlsList::SetBGMDevice: Given device is not BGMDevice");
}

#pragma mark Update Controls List

bool    BGMDeviceControlsList::MatchControlsListOf(AudioObjectID inDeviceID)
{
    CAMutex::Locker locker(mMutex);

    if(!mBGMDevice.IsBGMDevice())
    {
        LogWarning("BGMDeviceControlsList::MatchControlsListOf: BGMDevice ID not set");
        return false;
    }

    // If the output device doesn't have a control that BGMDevice does, disable it on BGMDevice so
    // the system's audio UI isn't confusing.

    // No need to change input controls.
    AudioObjectPropertyScope inScope = kAudioObjectPropertyScopeOutput;

    // Check which of BGMDevice's controls are currently enabled. We need to know whether we're
    // actually enabling/disabling any controls so we know whether we need to call
    // PropagateControlListChange afterward.
    CFTypeRef __nullable enabledControlsRef =
        mBGMDevice.GetPropertyData_CFType(kBGMEnabledOutputControlsAddress);

    ThrowIf(!enabledControlsRef || (CFGetTypeID(enabledControlsRef) != CFArrayGetTypeID()),
            CAException(kAudioHardwareIllegalOperationError),
            "BGMDeviceControlsList::MatchControlsListOf: Expected a CFArray for "
            "kAudioDeviceCustomPropertyEnabledOutputControls");

    CACFArray enabledControls(static_cast<CFArrayRef>(enabledControlsRef), true);

    BGMAssert(enabledControls.GetNumberItems() == 2,
              "BGMDeviceControlsList::MatchControlsListOf: Expected 2 array elements for "
              "kAudioDeviceCustomPropertyEnabledOutputControls");

    bool volumeEnabled;
    bool didGetBool = enabledControls.GetBool(kBGMEnabledOutputControlsIndex_Volume, volumeEnabled);
    ThrowIf(!didGetBool,
            CAException(kAudioHardwareIllegalOperationError),
            "BGMDeviceControlsList::MatchControlsListOf: Expected volume element of "
            "kAudioDeviceCustomPropertyEnabledOutputControls to be a CFBoolean");

    bool muteEnabled;
    didGetBool = enabledControls.GetBool(kBGMEnabledOutputControlsIndex_Mute, muteEnabled);
    ThrowIf(!didGetBool,
            CAException(kAudioHardwareIllegalOperationError),
            "BGMDeviceControlsList::MatchControlsListOf: Expected mute element of "
            "kAudioDeviceCustomPropertyEnabledOutputControls to be a CFBoolean");

    DebugMsg("BGMDeviceControlsList::MatchControlsListOf: BGMDevice has volume %s, mute %s",
             (volumeEnabled ? "enabled" : "disabled"),
             (muteEnabled ? "enabled" : "disabled"));

    // Check which controls the other device has.
    BGMAudioDevice device(inDeviceID);
    bool hasMute = device.HasSettableMasterMute(inScope);

    bool hasVolume =
        device.HasSettableMasterVolume(inScope) || device.HasSettableVirtualMasterVolume(inScope);

    if(!hasVolume)
    {
        // Check for per-channel volume controls.
        UInt32 numChannels =
            device.GetTotalNumberChannels(inScope == kAudioObjectPropertyScopeInput);

        for(UInt32 channel = 1; channel <= numChannels; channel++)
        {
            BGMLogAndSwallowExceptionsMsg("BGMDeviceControlsList::MatchControlsListOf",
                                          "Checking for channel volume controls",
                                          ([&] {
                hasVolume =
                    (device.HasVolumeControl(inScope, channel)
                            && device.VolumeControlIsSettable(inScope, channel));
            }));

            if(hasVolume)
            {
                break;
            }
        }
    }

    // Tell BGMDevice to enable/disable its controls to match the output device.
    bool deviceUpdated = false;

    CACFArray newEnabledControls;
    newEnabledControls.SetCFMutableArrayFromCopy(enabledControls.GetCFArray());

    // Update volume.
    if(volumeEnabled != hasVolume)
    {
        DebugMsg("BGMDeviceControlsList::MatchControlsListOf: %s BGMDevice volume control.",
                 hasVolume ? "Enabling" : "Disabling");

        newEnabledControls.SetBool(kBGMEnabledOutputControlsIndex_Volume, hasVolume);
        deviceUpdated = true;
    }

    // Update mute.
    if(muteEnabled != hasMute)
    {
        DebugMsg("BGMDeviceControlsList::MatchControlsListOf: %s BGMDevice mute control.",
                 hasMute ? "Enabling" : "Disabling");

        newEnabledControls.SetBool(kBGMEnabledOutputControlsIndex_Mute, hasMute);
        deviceUpdated = true;
    }

    if(deviceUpdated)
    {
        mBGMDevice.SetPropertyData_CFType(kBGMEnabledOutputControlsAddress,
                                          newEnabledControls.GetCFMutableArray());
    }

    return deviceUpdated;
}

void    BGMDeviceControlsList::PropagateControlListChange()
{
    CAMutex::Locker locker(mMutex);

    if((mBGMDevice == kAudioObjectUnknown) || !mCanToggleDeviceOnSystem)
    {
        return;
    }

    InitDeviceToggling();

    // Leave the default device alone if the user has changed it since launching BGMApp.
    bool bgmDeviceIsDefault = true;

    BGMLogAndSwallowExceptions("BGMDeviceControlsList::PropagateControlListChange", ([&] {
        bgmDeviceIsDefault =
            (mBGMDevice.GetObjectID() == mAudioSystem.GetDefaultAudioDevice(false, false));
    }));

    if(bgmDeviceIsDefault)
    {
        mDeviceToggleState = ToggleState::SettingNullDeviceAsDefault;

        // We'll get a notification from the HAL after the Null Device is enabled. Then we can
        // temporarily make it the default device, which gets other programs to notice that
        // BGMDevice's controls have changed.
        try
        {
            CAMutex::Unlocker unlocker(mMutex);
            SetNullDeviceEnabled(true);
        }
        catch (...)
        {
            mDeviceToggleState = ToggleState::NotToggling;
            LogError("BGMDeviceControlsList::PropagateControlListChange: Could not enable the Null "
                     "Device");
            throw;
        }
    }
}

#pragma mark Implementation

void    BGMDeviceControlsList::InitDeviceToggling()
{
    CAMutex::Locker locker(mMutex);

    if(mDeviceTogglingInitialised || !mCanToggleDeviceOnSystem)
    {
        return;
    }

    BGMAssert(mBGMDevice.IsBGMDevice(),
              "BGMDeviceControlsList::InitDeviceToggling: mBGMDevice device is not set to "
              "BGMDevice's ID");

    // Register a listener to find out when the Null Device becomes available/unavailable. See
    // ToggleDefaultDevice.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
    dispatch_queue_attr_t attr =
        dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_DEFAULT, 0);
#pragma clang diagnostic pop
    mListenerQueue = dispatch_queue_create("com.bearisdriving.BGM.BGMDeviceControlsList", attr);

    auto listenerBlock = ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress* inAddresses) {
        // Ignore the notification if we're not toggling the default device, which would just mean
        // the default device has been changed for an unrelated reason.
        if(mDeviceToggleState == ToggleState::NotToggling)
        {
            return;
        }

        for(int i = 0; i < inNumberAddresses; i++)
        {
            switch(inAddresses[i].mSelector)
            {
                case kAudioHardwarePropertyDevices:
                    {
                        CAMutex::Locker innerLocker(mMutex);

                        DebugMsg("BGMDeviceControlsList::InitDeviceToggling: Got "
                                 "kAudioHardwarePropertyDevices");

                        // Cancel the previous block in case it hasn't run yet.
                        DestroyBlock(mDeviceToggleBlock);

                        mDeviceToggleBlock = CreateDeviceToggleBlock();

                        // Changing the default device too quickly after enabling the Null Device
                        // seems to cause problems with some programs. Not sure why.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
                        if(mDeviceToggleBlock)
                        {
                            dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                                         kToggleDeviceInitialDelay),
                                           dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                                           BGM_Utils::NN(mDeviceToggleBlock));
                        }
#pragma clang diagnostic pop
                    }
                    break;

                default:
                    break;
            }
        }
    };

    mListenerBlock = Block_copy(listenerBlock);

    BGMLogAndSwallowExceptions("BGMDeviceControlsList::InitDeviceToggling", [&] {
        mAudioSystem.AddPropertyListenerBlock(CAPropertyAddress(kAudioHardwarePropertyDevices),
                                              mListenerQueue,
                                              mListenerBlock);
    });

    mDeviceTogglingInitialised = true;
}

void    BGMDeviceControlsList::ToggleDefaultDevice()
{
    // Set the Null Device as the OS X default device.
    AudioObjectID nullDeviceID = mAudioSystem.GetAudioDeviceForUID(CFSTR(kBGMNullDeviceUID));

    if(nullDeviceID == kAudioObjectUnknown)
    {
        // It's unlikely, but we might have been notified about an unrelated device so just log a
        // warning.
        LogWarning("BGMDeviceControlsList::ToggleDefaultDevice: Null Device not found");
        return;
    }

    DebugMsg("BGMDeviceControlsList::ToggleDefaultDevice: Setting Null Device as default. "
             "nullDeviceID = %u", nullDeviceID);
    mAudioSystem.SetDefaultAudioDevice(false, false, nullDeviceID);

    mDeviceToggleState = ToggleState::SettingBGMDeviceAsDefault;

    // A small number of apps (e.g. Firefox) seem to have trouble with the default device being
    // changed back immediately, so for now we insert a short delay here and before disabling the
    // Null Device.

    // Cancel the previous block in case it hasn't run yet.
    DestroyBlock(mDeviceToggleBackBlock);

    mDeviceToggleBackBlock = CreateDeviceToggleBackBlock();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
    if(mDeviceToggleBackBlock)
    {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, kToggleDeviceBackDelay),
                       dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                       BGM_Utils::NN(mDeviceToggleBackBlock));
    }
#pragma clang diagnostic pop
}

void    BGMDeviceControlsList::SetNullDeviceEnabled(bool inEnabled)
{
    DebugMsg("BGMDeviceControlsList::SetNullDeviceEnabled: %s the null device",
             inEnabled ? "Enabling" : "Disabling");

    // Get the audio object for BGMDriver, which is the object the Null Device belongs to.
    AudioObjectID bgmDriverID = mAudioSystem.GetAudioPlugInForBundleID(CFSTR(kBGMDriverBundleID));

    if(bgmDriverID == kAudioObjectUnknown)
    {
        LogError("BGMDeviceControlsList::SetNullDeviceEnabled: BGMDriver plug-in audio object not "
                 "found");
        throw CAException(kAudioHardwareUnspecifiedError);
    }

    CAHALAudioObject bgmDriver(bgmDriverID);
    bgmDriver.SetPropertyData_CFType(CAPropertyAddress(kAudioPlugInCustomPropertyNullDeviceActive),
                                     (inEnabled ? kCFBooleanTrue : kCFBooleanFalse));
}

dispatch_block_t __nullable BGMDeviceControlsList::CreateDeviceToggleBlock()
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
    dispatch_block_t __nullable toggleBlock = dispatch_block_create((dispatch_block_flags_t)0, ^{
#pragma clang diagnostic pop
        CAMutex::Locker locker(mMutex);

        if(mDeviceToggleState == ToggleState::SettingNullDeviceAsDefault)
        {
            BGMLogAndSwallowExceptions("BGMDeviceControlsList::CreateDeviceToggleBlock",
                                       ([&] {
                ToggleDefaultDevice();
            }));
        }
    });

    if(!toggleBlock)
    {
        // Pretty sure this should never happen, but the docs aren't completely clear.
        LogError("BGMDeviceControlsList::CreateDeviceToggleBlock: !toggleBlock");
    }

    return toggleBlock;
}

dispatch_block_t __nullable BGMDeviceControlsList::CreateDeviceToggleBackBlock()
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
    dispatch_block_t __nullable toggleBackBlock =
            dispatch_block_create((dispatch_block_flags_t)0, ^{
#pragma clang diagnostic pop
        CAMutex::Locker locker(mMutex);

        if(mDeviceToggleState != ToggleState::SettingBGMDeviceAsDefault)
        {
            return;
        }

        // Set BGMDevice back as the default device.
        DebugMsg("BGMDeviceControlsList::ToggleDefaultDevice: Setting BGMDevice as default");
        BGMLogAndSwallowExceptions("BGMDeviceControlsList::CreateDeviceToggleBackBlock", ([&] {
            mAudioSystem.SetDefaultAudioDevice(false, false, mBGMDevice.GetObjectID());
        }));

        mDeviceToggleState = ToggleState::DisablingNullDevice;

        // Cancel the previous block in case it hasn't run yet.
        DestroyBlock(mDisableNullDeviceBlock);

        mDisableNullDeviceBlock = CreateDisableNullDeviceBlock();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
        if(mDisableNullDeviceBlock)
        {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, kDisableNullDeviceDelay),
                           dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0),
                           BGM_Utils::NN(mDisableNullDeviceBlock));
        }
#pragma clang diagnostic pop
    });

    if(!toggleBackBlock)
    {
        // Pretty sure this should never happen, but the docs aren't completely clear.
        LogError("BGMDeviceControlsList::CreateDeviceToggleBackBlock: !toggleBackBlock");
    }

    return toggleBackBlock;
}

dispatch_block_t __nullable BGMDeviceControlsList::CreateDisableNullDeviceBlock()
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
    dispatch_block_t __nullable disableNullDeviceBlock =
            dispatch_block_create((dispatch_block_flags_t)0, ^{
#pragma clang diagnostic pop
        CAMutex::Locker locker(mMutex);

        if(mDeviceToggleState != ToggleState::DisablingNullDevice)
        {
            return;
        }

        mDeviceToggleState = ToggleState::NotToggling;

        BGMLogAndSwallowExceptions("BGMDeviceControlsList::CreateDisableNullDeviceBlock",
                                   ([&] {
            CAMutex::Unlocker unlocker(mMutex);
            // Hide the null device from the user again.
            SetNullDeviceEnabled(false);
        }));

        BGMAssert(mBGMDevice.IsBGMDevice(), "BGMDevice's AudioObjectID changed");
    });

    if(!disableNullDeviceBlock)
    {
        // Pretty sure this should never happen, but the docs aren't completely clear.
        LogError("BGMDeviceControlsList::CreateDisableNullDeviceBlock: !disableNullDeviceBlock");
    }

    return disableNullDeviceBlock;
}

void    BGMDeviceControlsList::DestroyBlock(dispatch_block_t __nullable & block)
{
    if(!block)
    {
        return;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
    dispatch_block_t& blockNN = (dispatch_block_t&)block;
    if(!dispatch_block_testcancel(blockNN))
    {
        // Stop the block from running if it's currently queued.
        dispatch_block_cancel(blockNN);

        // Make sure the block isn't currently running. That should almost never be the case.
        while(!dispatch_block_testcancel(blockNN))
        {
            CAMutex::Unlocker unlocker(mMutex);
            usleep(10);
        }
        
        Block_release(block);
        block = nullptr;
    }
#pragma clang diagnostic pop
}

#pragma clang assume_nonnull end


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
//  BGM_PlugIn.h
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//  Copyright (C) 2013 Apple Inc. All Rights Reserved.
//
//  Based largely on SA_PlugIn.h from Apple's SimpleAudioDriver Plug-In sample code.
//  https://developer.apple.com/library/mac/samplecode/AudioDriverExamples
//

#ifndef __BGMDriver__BGM_PlugIn__
#define __BGMDriver__BGM_PlugIn__

// SuperClass Includes
#include "BGM_Object.h"

// Local Includes
#include "BGM_Types.h"

// PublicUtility Includes
#include "CAMutex.h"


class BGM_PlugIn
:
	public BGM_Object
{

#pragma mark Construction/Destruction
    
public:
    static BGM_PlugIn&				GetInstance();

protected:
                                    BGM_PlugIn();
	virtual							~BGM_PlugIn();
    virtual void					Deactivate();
    
private:
    static void						StaticInitializer();
    
#pragma mark Host Access
    
public:
	static void						SetHost(AudioServerPlugInHostRef inHost)	{ sHost = inHost; }
	
	static void						Host_PropertiesChanged(AudioObjectID inObjectID, UInt32 inNumberAddresses, const AudioObjectPropertyAddress inAddresses[])	{ if(sHost != NULL) { sHost->PropertiesChanged(sHost, inObjectID, inNumberAddresses, inAddresses); } }
	static void						Host_RequestDeviceConfigurationChange(AudioObjectID inDeviceObjectID, UInt64 inChangeAction, void* inChangeInfo)			{ if(sHost != NULL) { sHost->RequestDeviceConfigurationChange(sHost, inDeviceObjectID, inChangeAction, inChangeInfo); } }

#pragma mark Property Operations
    
public:
	virtual bool					HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	virtual bool					IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	virtual UInt32					GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const;
	virtual void					GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* outData) const;
	virtual void					SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData);

#pragma mark Implementation
    
public:
    const CFStringRef               GetBundleID() const { return CFSTR(kBGMDriverBundleID); }
    
private:
    CAMutex							mMutex;
    
    static pthread_once_t			sStaticInitializer;
    static BGM_PlugIn*				sInstance;
	static AudioServerPlugInHostRef	sHost;

};

#endif /* __BGMDriver__BGM_PlugIn__ */


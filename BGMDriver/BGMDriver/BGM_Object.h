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
//  BGM_Object.h
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//  Copyright (C) 2013 Apple Inc. All Rights Reserved.
//
//  Based largely on SA_Object.h from Apple's SimpleAudioDriver Plug-In sample code.
//  https://developer.apple.com/library/mac/samplecode/AudioDriverExamples
//
//  The base class for our classes that represent audio objects. (See AudioServerPlugIn.h for a
//  quick explanation of audio objects.)
// 
//  This is a stripped down version of SA_Object.h. Unlike the sample code plugin that SA_Object.h
//  is from, we have a fixed set of audio objects. So we've removed the code that handled
//  growing/shrinking that set.
//

#ifndef __BGMDriver__BGM_Object__
#define __BGMDriver__BGM_Object__

//	System Includes
#include <CoreAudio/AudioServerPlugIn.h>


//  TODO: Update the large comment below to reflect what was removed from SA_Object.h? It hasn't
//        been changed at all so far, except to replace "SA_Object" with "BGM_Object".

//==================================================================================================
//	BGM_Object
//
//	This is the base class for objects managed by BGM_ObjectMap. It's only job is to ensure that
//	objects of this type have the proper external semantics for a reference counted object. This
//	means that the destructor is protected so that these objects cannot be deleted directly. Also,
//	these objects many not make a copy of another object or be assigned from another object. Note
//	that the reference count of the object is tracked and owned by the BGM_ObjectMap.
//
//	These objects provide RTTI information tied to the constants describing the HAL's API class
//	hierarchy as described in the headers. The class ID and base class IDs passed in to the
//	constructor must operate with the semantics described in AudioObjectBase.h where the base class
//	has to always be one of the standard classes. The class ID can be a custom value or a standard
//	value however. If it is a standard value, the base class should be the proper standard base
//	class. So for example, a standard volume control object will say that it's class is
//	kAudioVolumeControlClassID and that its base class is kAudioLevelControlClassID. In the case of
//	a custom boolean control, it would say that it's class is a custom value like 'MYBL' and that
//	its base class is kAudioBooleanControlClassID.
//
//	Subclasses of this class must implement Activate(). This method is called after an object has
//	been constructed and inserted into the object map. Until Activate() is called, a constructed
//	object may not do anything active such as sending/receiving notifications or creating other
//	objects. Active operations may be performed in the Activate() method proper however. Note that
//	Activate() is called prior to any references to the object being handed out. As such, it does
//	not need to worry about being thread safe while Activate() is in progress.
//
//	Subclasses of this class must also implement Deactivate(). This method is called when the object
//	is at the end of it's lifecycle. Once Deactivate() has been called, the object may no longer
//	perform active opertions, including Deactivating other objects. This is based on the notion that
//	all the objects have a definite point at which they are considered dead to the outside world.
//	For example, an AudioDevice object is dead if it's hardware is unplugged. The point of death is
//	the notification the owner of the device gets to signal that it has been unplugged. Note that it
//	is both normal and expected that a dead object might still have outstanding references. Thus, an
//	object has to put in some care to do the right thing when these zombie references are used. The
//	best thing to do is to just have those queries return appropriate errors.
//
//	Deactivate() itself needs to be thread safe with respect to other operations taking place on the
//	object. This also means taking care to handle the Deactivation of owned objects. For example, an
//	AudioDevice object will almost always own one or more AudioStream objects. If the stream is in a
//	separate lock domain from it's owning device, then the device has to be very careful about how
//	it deactivates the stream such that it doesn't try to lock the stream's lock while holding the
//	device's lock which will inevitably lead to a deadlock situation. There are two reasonable
//	approaches to dealing with this kind of situation. The first is to just not get into it by
//	making the device share a lock domain with all it's owned objects like streams and controls. The
//	other approach is to use dispatch queues to make the work of Deactivating owned objects take
//	place outside of the device's lock domain. For example, if the device needs to deactivate a
//	stream, it can remove the stream from any tracking in the device object and then dispatch
//	asynchronously the Deactivate() call on the stream and the release of the reference the device
//	has on the stream.
//
//	Note that both Activate() and Deactivate() are called by objects at large. Typically,
//	Activate() is called by the creator of the object, usually right after the object has been
//	allocated. Deactivate() will usually be called by the owner of the object upon recognizing that
//	the object is dead to the outside world. Going back to the example of an AudioDevice getting
//	unplugged, the Deactivate() method will be called by whomever receives the notification about
//	the hardware going away, which is often the owner of the object.
//
//	This class also defines methods to implement the portion of the
//	AudioServerPlugInDriverInterface that deals with properties. The five methods all have the same
//	basic arguments and semantics. The class also provides the implementation for
//	the minimum required properties for all AudioObjects. There is a detailed commentary about each
//	specific property in the GetPropertyData() method.
//
//	It is important that a thread retain and hold a reference while it is using an BGM_Object and 
//	that the reference be released promptly when the thread is finished using the object. By
//	assuming this, a BGM_Object can minimize the amount of locking it needs to do. In particular,
//	purely static or invariant data can be handled without any locking at all.
//==================================================================================================

class BGM_Object
{

#pragma mark Construction/Destruction
    
public:
						BGM_Object(AudioObjectID inObjectID, AudioClassID inClassID, AudioClassID inBaseClassID, AudioObjectID inOwnerObjectID);
					
	virtual void		Activate();
	virtual void		Deactivate();

protected:
	virtual				~BGM_Object();

private:
						BGM_Object(const BGM_Object&);
	BGM_Object&			operator=(const BGM_Object&);

#pragma mark Attributes
    
public:
	AudioObjectID		GetObjectID() const			{ return mObjectID; }
	void*				GetObjectIDAsPtr() const	{ uintptr_t thePtr = mObjectID; return reinterpret_cast<void*>(thePtr); }
	AudioClassID		GetClassID() const			{ return mClassID; }
	AudioClassID		GetBaseClassID() const		{ return mBaseClassID; }
	AudioObjectID		GetOwnerObjectID() const	{ return mOwnerObjectID; }
	bool				IsActive() const			{ return mIsActive; }

#pragma mark Property Operations
    
public:
	virtual bool		HasProperty(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	virtual bool		IsPropertySettable(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress) const;
	virtual UInt32		GetPropertyDataSize(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData) const;
	virtual void		GetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, UInt32& outDataSize, void* outData) const;
	virtual void		SetPropertyData(AudioObjectID inObjectID, pid_t inClientPID, const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void* inQualifierData, UInt32 inDataSize, const void* inData);

#pragma mark Implementation
    
protected:
	AudioObjectID		mObjectID;
	AudioClassID		mClassID;
	AudioClassID		mBaseClassID;
	AudioObjectID		mOwnerObjectID;
	bool				mIsActive;

};

#endif /* __BGMDriver__BGM_Object__ */


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
//  CAMutex.h
//  PublicUtility
//
//  Copyright (C) 2014 Apple Inc. All Rights Reserved.
//  Copyright © 2020 Kyle Neideck
//
//  Original license header follows.
//

/*
     File: CAMutex.h
 Abstract: Part of CoreAudio Utility Classes
  Version: 1.1
 
 Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple
 Inc. ("Apple") in consideration of your agreement to the following
 terms, and your use, installation, modification or redistribution of
 this Apple software constitutes acceptance of these terms.  If you do
 not agree with these terms, please do not use, install, modify or
 redistribute this Apple software.
 
 In consideration of your agreement to abide by the following terms, and
 subject to these terms, Apple grants you a personal, non-exclusive
 license, under Apple's copyrights in this original Apple software (the
 "Apple Software"), to use, reproduce, modify and redistribute the Apple
 Software, with or without modifications, in source and/or binary forms;
 provided that if you redistribute the Apple Software in its entirety and
 without modifications, you must retain this notice and the following
 text and disclaimers in all such redistributions of the Apple Software.
 Neither the name, trademarks, service marks or logos of Apple Inc. may
 be used to endorse or promote products derived from the Apple Software
 without specific prior written permission from Apple.  Except as
 expressly stated in this notice, no other rights or licenses, express or
 implied, are granted by Apple herein, including but not limited to any
 patent rights that may be infringed by your derivative works or by other
 works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
 MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
 THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
 OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
 AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
 STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 
 Copyright (C) 2014 Apple Inc. All Rights Reserved.
 
*/
#ifndef __CAMutex_h__
#define __CAMutex_h__

//==================================================================================================
//	Includes
//==================================================================================================

#include "BGMThreadSafetyAnalysis.h"

//	System Includes
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreAudio/CoreAudioTypes.h>
#else
	#include <CoreAudioTypes.h>
#endif

#if TARGET_OS_MAC
	#include <pthread.h>
#elif TARGET_OS_WIN32
	#include <windows.h>
#else
	#error	Unsupported operating system
#endif

//==================================================================================================
//	A recursive mutex.
//==================================================================================================

class CAPABILITY("mutex") CAMutex
{
//	Construction/Destruction
public:
					CAMutex(const char* inName);
	virtual			~CAMutex();

//	Actions
public:
	virtual bool	Lock() ACQUIRE();
	virtual void	Unlock() RELEASE();
	virtual bool	Try(bool& outWasLocked) TRY_ACQUIRE(true);	// returns true if lock is free, false if not
	
	virtual bool	IsFree() const;
	virtual bool	IsOwnedByCurrentThread() const;
		
//	Implementation
protected:
	const char*		mName;
#if TARGET_OS_MAC
	pthread_t		mOwner;
	pthread_mutex_t	mMutex;
#elif TARGET_OS_WIN32
	UInt32			mOwner;
	HANDLE			mMutex;
#endif

//	Helper class to manage taking and releasing recursively
public:
	class SCOPED_CAPABILITY Locker
	{
	
	//	Construction/Destruction
	public:
					Locker(CAMutex& inMutex) ACQUIRE(inMutex) : mMutex(&inMutex), mNeedsRelease(false) { mNeedsRelease = mMutex->Lock(); }
					Locker(CAMutex* inMutex) ACQUIRE(inMutex) : mMutex(inMutex), mNeedsRelease(false) { mNeedsRelease = (mMutex != NULL && mMutex->Lock()); }
						// in this case the mutex can be null
					~Locker() RELEASE() { if(mNeedsRelease) { mMutex->Unlock(); } }
	
	
	private:
					Locker(const Locker&);
		Locker&		operator=(const Locker&);
	
	//	Implementation
	private:
		CAMutex*	mMutex;
		bool		mNeedsRelease;
	
	};

	// Clang's static analysis doesn't work for unlocker classes. See
	// <https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#no-alias-analysis>.
// Unlocker
	class Unlocker
	{
	public:
						Unlocker(CAMutex& inMutex);
						~Unlocker();
		
	private:
		CAMutex&	mMutex;
		bool		mNeedsLock;
		
		// Hidden definitions of copy ctor, assignment operator
		Unlocker(const Unlocker& copy);				// Not implemented
		Unlocker& operator=(const Unlocker& copy);	// Not implemented
	};
	
// you can use this with Try - if you take the lock in try, pass in the outWasLocked var
	class SCOPED_CAPABILITY Tryer {
	
	//	Construction/Destruction
	public:
		Tryer (CAMutex &mutex) TRY_ACQUIRE(true, mutex) : mMutex(mutex), mNeedsRelease(false), mHasLock(false) { mHasLock = mMutex.Try (mNeedsRelease); }
		~Tryer () RELEASE() { if (mNeedsRelease) mMutex.Unlock(); }
		
		bool HasLock () const { return mHasLock; }

	private:
					Tryer(const Tryer&);
		Tryer&		operator=(const Tryer&);

	//	Implementation
	private:
		CAMutex &		mMutex;
		bool			mNeedsRelease;
		bool			mHasLock;
	};
};


#endif // __CAMutex_h__

/*
     File: CAMutex.cpp 
 Abstract:  Part of CoreAudio Utility Classes  
  Version: 1.0.1 
  
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
  
 Copyright (C) 2013 Apple Inc. All Rights Reserved. 
  
*/
//==================================================================================================
//	Includes
//==================================================================================================

//	Self Include
#include "CAMutex.h"

#if TARGET_OS_MAC
	#include <errno.h>
#endif

//	PublicUtility Includes
#include "CADebugMacros.h"
#include "CAException.h"
#include "CAHostTimeBase.h"

//==================================================================================================
//	Logging
//==================================================================================================

#if CoreAudio_Debug
//	#define	Log_Ownership		1
//	#define	Log_Errors			1
//	#define Log_LongLatencies	1
//	#define LongLatencyThreshholdNS	1000000ULL	// nanoseconds
#endif

//==================================================================================================
//	CAMutex
//==================================================================================================

CAMutex::CAMutex(const char* inName)
:
	mName(inName),
	mOwner(0)
{
#if TARGET_OS_MAC
	OSStatus theError = pthread_mutex_init(&mMutex, NULL);
	ThrowIf(theError != 0, CAException(theError), "CAMutex::CAMutex: Could not init the mutex");
	
	#if	Log_Ownership
		DebugPrintfRtn(DebugPrintfFileComma "%p %.4f: CAMutex::CAMutex: creating %s, owner: %p\n", pthread_self(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), mName, mOwner);
	#endif
#elif TARGET_OS_WIN32
	mMutex = CreateMutex(NULL, false, NULL);
	ThrowIfNULL(mMutex, CAException(GetLastError()), "CAMutex::CAMutex: could not create the mutex.");
	
	#if	Log_Ownership
		DebugPrintfRtn(DebugPrintfFileComma "%lu %.4f: CAMutex::CAMutex: creating %s, owner: %lu\n", GetCurrentThreadId(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), mName, mOwner);
	#endif
#endif
}

CAMutex::~CAMutex()
{
#if TARGET_OS_MAC
	#if	Log_Ownership
		DebugPrintfRtn(DebugPrintfFileComma "%p %.4f: CAMutex::~CAMutex: destroying %s, owner: %p\n", pthread_self(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), mName, mOwner);
	#endif
	pthread_mutex_destroy(&mMutex);
#elif TARGET_OS_WIN32
	#if	Log_Ownership
		DebugPrintfRtn(DebugPrintfFileComma "%lu %.4f: CAMutex::~CAMutex: destroying %s, owner: %lu\n", GetCurrentThreadId(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), mName, mOwner);
	#endif
	if(mMutex != NULL)
	{
		CloseHandle(mMutex);
	}
#endif
}

bool	CAMutex::Lock()
{
	bool theAnswer = false;
	
#if TARGET_OS_MAC
	pthread_t theCurrentThread = pthread_self();
	if(!pthread_equal(theCurrentThread, mOwner))
	{
		#if	Log_Ownership
			DebugPrintfRtn(DebugPrintfFileComma "%p %.4f: CAMutex::Lock: thread %p is locking %s, owner: %p\n", theCurrentThread, ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), theCurrentThread, mName, mOwner);
		#endif
		
		#if Log_LongLatencies
			UInt64 lockTryTime = CAHostTimeBase::GetCurrentTimeInNanos();
		#endif
		
		OSStatus theError = pthread_mutex_lock(&mMutex);
		ThrowIf(theError != 0, CAException(theError), "CAMutex::Lock: Could not lock the mutex");
		mOwner = theCurrentThread;
		theAnswer = true;
	
		#if Log_LongLatencies
			UInt64 lockAcquireTime = CAHostTimeBase::GetCurrentTimeInNanos();
			if (lockAcquireTime - lockTryTime >= LongLatencyThresholdNS)
				DebugPrintfRtn(DebugPrintfFileComma "Thread %p took %.6fs to acquire the lock %s\n", theCurrentThread, (lockAcquireTime - lockTryTime) * 1.0e-9 /* nanos to seconds */, mName);
		#endif
		
		#if	Log_Ownership
			DebugPrintfRtn(DebugPrintfFileComma "%p %.4f: CAMutex::Lock: thread %p has locked %s, owner: %p\n", pthread_self(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), pthread_self(), mName, mOwner);
		#endif
	}
#elif TARGET_OS_WIN32
	if(mOwner != GetCurrentThreadId())
	{
		#if	Log_Ownership
			DebugPrintfRtn(DebugPrintfFileComma "%lu %.4f: CAMutex::Lock: thread %lu is locking %s, owner: %lu\n", GetCurrentThreadId(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), GetCurrentThreadId(), mName, mOwner);
		#endif

		OSStatus theError = WaitForSingleObject(mMutex, INFINITE);
		ThrowIfError(theError, CAException(theError), "CAMutex::Lock: could not lock the mutex");
		mOwner = GetCurrentThreadId();
		theAnswer = true;
	
		#if	Log_Ownership
			DebugPrintfRtn(DebugPrintfFileComma "%lu %.4f: CAMutex::Lock: thread %lu has locked %s, owner: %lu\n", GetCurrentThreadId(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), GetCurrentThreadId(), mName, mOwner);
		#endif
	}
#endif

	return theAnswer;
}

void	CAMutex::Unlock()
{
#if TARGET_OS_MAC
	if(pthread_equal(pthread_self(), mOwner))
	{
		#if	Log_Ownership
			DebugPrintfRtn(DebugPrintfFileComma "%p %.4f: CAMutex::Unlock: thread %p is unlocking %s, owner: %p\n", pthread_self(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), pthread_self(), mName, mOwner);
		#endif

		mOwner = 0;
		OSStatus theError = pthread_mutex_unlock(&mMutex);
		ThrowIf(theError != 0, CAException(theError), "CAMutex::Unlock: Could not unlock the mutex");
	
		#if	Log_Ownership
			DebugPrintfRtn(DebugPrintfFileComma "%p %.4f: CAMutex::Unlock: thread %p has unlocked %s, owner: %p\n", pthread_self(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), pthread_self(), mName, mOwner);
		#endif
	}
	else
	{
		DebugMessage("CAMutex::Unlock: A thread is attempting to unlock a Mutex it doesn't own");
	}
#elif TARGET_OS_WIN32
	if(mOwner == GetCurrentThreadId())
	{
		#if	Log_Ownership
			DebugPrintfRtn(DebugPrintfFileComma "%lu %.4f: CAMutex::Unlock: thread %lu is unlocking %s, owner: %lu\n", GetCurrentThreadId(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), GetCurrentThreadId(), mName, mOwner);
		#endif

		mOwner = 0;
		bool wasReleased = ReleaseMutex(mMutex);
		ThrowIf(!wasReleased, CAException(GetLastError()), "CAMutex::Unlock: Could not unlock the mutex");
	
		#if	Log_Ownership
			DebugPrintfRtn(DebugPrintfFileComma "%lu %.4f: CAMutex::Unlock: thread %lu has unlocked %s, owner: %lu\n", GetCurrentThreadId(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), GetCurrentThreadId(), mName, mOwner);
		#endif
	}
	else
	{
		DebugMessage("CAMutex::Unlock: A thread is attempting to unlock a Mutex it doesn't own");
	}
#endif
}

bool	CAMutex::Try(bool& outWasLocked)
{
	bool theAnswer = false;
	outWasLocked = false;

#if TARGET_OS_MAC
	pthread_t theCurrentThread = pthread_self();
	if(!pthread_equal(theCurrentThread, mOwner))
	{
		//	this means the current thread doesn't already own the lock
		#if	Log_Ownership
			DebugPrintfRtn(DebugPrintfFileComma "%p %.4f: CAMutex::Try: thread %p is try-locking %s, owner: %p\n", theCurrentThread, ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), theCurrentThread, mName, mOwner);
		#endif

		//	go ahead and call trylock to see if we can lock it.
		int theError = pthread_mutex_trylock(&mMutex);
		if(theError == 0)
		{
			//	return value of 0 means we successfully locked the lock
			mOwner = theCurrentThread;
			theAnswer = true;
			outWasLocked = true;
	
			#if	Log_Ownership
				DebugPrintfRtn(DebugPrintfFileComma "%p %.4f: CAMutex::Try: thread %p has locked %s, owner: %p\n", theCurrentThread, ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), theCurrentThread, mName, mOwner);
			#endif
		}
		else if(theError == EBUSY)
		{
			//	return value of EBUSY means that the lock was already locked by another thread
			theAnswer = false;
			outWasLocked = false;
	
			#if	Log_Ownership
				DebugPrintfRtn(DebugPrintfFileComma "%p %.4f: CAMutex::Try: thread %p failed to lock %s, owner: %p\n", theCurrentThread, ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), theCurrentThread, mName, mOwner);
			#endif
		}
		else
		{
			//	any other return value means something really bad happenned
			ThrowIfError(theError, CAException(theError), "CAMutex::Try: call to pthread_mutex_trylock failed");
		}
	}
	else
	{
		//	this means the current thread already owns the lock
		theAnswer = true;
		outWasLocked = false;
	}
#elif TARGET_OS_WIN32
	if(mOwner != GetCurrentThreadId())
	{
		//	this means the current thread doesn't own the lock
		#if	Log_Ownership
			DebugPrintfRtn(DebugPrintfFileComma "%lu %.4f: CAMutex::Try: thread %lu is try-locking %s, owner: %lu\n", GetCurrentThreadId(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), GetCurrentThreadId(), mName, mOwner);
		#endif
		
		//	try to acquire the mutex
		OSStatus theError = WaitForSingleObject(mMutex, 0);
		if(theError == WAIT_OBJECT_0)
		{
			//	this means we successfully locked the lock
			mOwner = GetCurrentThreadId();
			theAnswer = true;
			outWasLocked = true;
	
			#if	Log_Ownership
				DebugPrintfRtn(DebugPrintfFileComma "%lu %.4f: CAMutex::Try: thread %lu has locked %s, owner: %lu\n", GetCurrentThreadId(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), GetCurrentThreadId(), mName, mOwner);
			#endif
		}
		else if(theError == WAIT_TIMEOUT)
		{
			//	this means that the lock was already locked by another thread
			theAnswer = false;
			outWasLocked = false;
	
			#if	Log_Ownership
				DebugPrintfRtn(DebugPrintfFileComma "%lu %.4f: CAMutex::Try: thread %lu failed to lock %s, owner: %lu\n", GetCurrentThreadId(), ((Float64)(CAHostTimeBase::GetCurrentTimeInNanos()) / 1000000.0), GetCurrentThreadId(), mName, mOwner);
			#endif
		}
		else
		{
			//	any other return value means something really bad happenned
			ThrowIfError(theError, CAException(GetLastError()), "CAMutex::Try: call to lock the mutex failed");
		}
	}
	else
	{
		//	this means the current thread already owns the lock
		theAnswer = true;
		outWasLocked = false;
	}
#endif
	
	return theAnswer;
}

bool	CAMutex::IsFree() const
{
	return mOwner == 0;
}

bool	CAMutex::IsOwnedByCurrentThread() const
{
	bool theAnswer = true;
	
#if TARGET_OS_MAC
	theAnswer = pthread_equal(pthread_self(), mOwner);
#elif TARGET_OS_WIN32
	theAnswer = (mOwner == GetCurrentThreadId());
#endif

	return theAnswer;
}


CAMutex::Unlocker::Unlocker(CAMutex& inMutex)
:	mMutex(inMutex),
	mNeedsLock(false)
{
	Assert(mMutex.IsOwnedByCurrentThread(), "Major problem: Unlocker attempted to unlock a mutex not owned by the current thread!");

	mMutex.Unlock();
	mNeedsLock = true;
}

CAMutex::Unlocker::~Unlocker()
{
	if(mNeedsLock)
	{
		mMutex.Lock();
	}
}

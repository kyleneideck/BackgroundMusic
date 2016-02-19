/*
     File: CAPThread.h
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
#if !defined(__CAPThread_h__)
#define __CAPThread_h__

//==================================================================================================
//	Includes
//==================================================================================================

//	System Includes
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreFoundation/CFBase.h>
#else
	#include <CFBase.h>
#endif

#if TARGET_OS_MAC
	#include <pthread.h>
	#include <unistd.h>
#elif TARGET_OS_WIN32
	#include <windows.h>
#else
	#error	Unsupported operating system
#endif

//==================================================================================================
//	CAPThread
//
//	This class wraps a pthread and a Win32 thread.
//	caution: long-running fixed priority threads can make the system unresponsive
//==================================================================================================

class	CAPThread
{

//	Types
public:
	typedef void*			(*ThreadRoutine)(void* inParameter);

//	Constants
public:
	enum
	{
#if	TARGET_OS_MAC
							kMinThreadPriority = 1,
							kMaxThreadPriority = 63,
							kDefaultThreadPriority = 31,
							kMaxThreadNameLength = 64
#elif TARGET_OS_WIN32
							kMinThreadPriority = 1,
							kMaxThreadPriority = 31,
							kDefaultThreadPriority = THREAD_PRIORITY_NORMAL,
							kMaxThreadNameLength = 256
#endif
	};

//	Construction/Destruction
public:
							CAPThread(ThreadRoutine inThreadRoutine, void* inParameter, UInt32 inPriority = kDefaultThreadPriority, bool inFixedPriority=false, bool inAutoDelete=false, const char* inThreadName = NULL);
							CAPThread(ThreadRoutine inThreadRoutine, void* inParameter, UInt32 inPeriod, UInt32 inComputation, UInt32 inConstraint, bool inIsPreemptible, bool inAutoDelete=false, const char* inThreadName = NULL);
	virtual					~CAPThread();

//	Properties
public:
#if TARGET_OS_MAC
	typedef pthread_t		NativeThread;

	NativeThread			GetNativeThread() { return mPThread; }
	static NativeThread		GetCurrentThread() { return pthread_self(); }
	static bool				IsNativeThreadsEqual(NativeThread a, NativeThread b) { return (a==b); }

	bool					operator==(NativeThread b) { return pthread_equal(mPThread,b); }

	pthread_t				GetPThread() const { return mPThread; }
	bool					IsCurrentThread() const { return (0 != mPThread) && (pthread_self() == mPThread); }
	bool					IsRunning() const { return 0 != mPThread; }
    static UInt32			getScheduledPriority(pthread_t inThread, int inPriorityKind);
#elif TARGET_OS_WIN32
	typedef unsigned long	NativeThread;
	
	NativeThread			GetNativeThread() { return mThreadID; }
	static NativeThread		GetCurrentThread() { return GetCurrentThreadId(); }
	static bool				IsNativeThreadsEqual(NativeThread a, NativeThread b) { return (a==b); }

	bool					operator ==(NativeThread b) { return (mThreadID==b); }

	HANDLE					GetThreadHandle() const { return mThreadHandle; }
	UInt32					GetThreadID() const { return mThreadID; }
	bool					IsCurrentThread() const { return (0 != mThreadID) && (GetCurrentThreadId() == mThreadID); }
	bool					IsRunning() const { return 0 != mThreadID; }
#endif

	bool					IsTimeShareThread() const { return !mTimeConstraintSet; }
	bool					IsTimeConstraintThread() const { return mTimeConstraintSet; }

	UInt32					GetPriority() const { return mPriority; }
    UInt32					GetScheduledPriority();
	static UInt32			GetScheduledPriority(NativeThread thread);
    void					SetPriority(UInt32 inPriority, bool inFixedPriority=false);
	static void				SetPriority(NativeThread inThread, UInt32 inPriority, bool inFixedPriority = false);

	void					GetTimeConstraints(UInt32& outPeriod, UInt32& outComputation, UInt32& outConstraint, bool& outIsPreemptible) const { outPeriod = mPeriod; outComputation = mComputation; outConstraint = mConstraint; outIsPreemptible = mIsPreemptible; }
	void					SetTimeConstraints(UInt32 inPeriod, UInt32 inComputation, UInt32 inConstraint, bool inIsPreemptible);
	void					ClearTimeConstraints() { SetPriority(mPriority); }
	
	bool					WillAutoDelete() const { return mAutoDelete; }
	void					SetAutoDelete(bool b) { mAutoDelete = b; }
	
	void					SetName(const char* inThreadName);

#if CoreAudio_Debug	
	void					DebugPriority(const char *label);
#endif

//	Actions
public:
	virtual void			Start();

//	Implementation
protected:
#if TARGET_OS_MAC
	static void*			Entry(CAPThread* inCAPThread);
#elif TARGET_OS_WIN32
	static UInt32 WINAPI	Entry(CAPThread* inCAPThread);
#endif

#if	TARGET_OS_MAC
	pthread_t				mPThread;
    UInt32					mSpawningThreadPriority;
#elif TARGET_OS_WIN32
	HANDLE					mThreadHandle;
	unsigned long			mThreadID;
#endif
	ThreadRoutine			mThreadRoutine;
	void*					mThreadParameter;
	char					mThreadName[kMaxThreadNameLength];
	UInt32					mPriority;
	UInt32					mPeriod;
	UInt32					mComputation;
	UInt32					mConstraint;
	bool					mIsPreemptible;
	bool					mTimeConstraintSet;
    bool					mFixedPriority;
	bool					mAutoDelete;		// delete self when thread terminates
};

#endif

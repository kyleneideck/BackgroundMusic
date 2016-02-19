/*
     File: CAPThread.cpp
 Abstract: CAPThread.h
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
//=============================================================================
//	Includes
//=============================================================================

//	Self Include
#include "CAPThread.h"

//	PublicUtility Includes
#include "CADebugMacros.h"
#include "CAException.h"

//	System Includes
#if	TARGET_OS_MAC
	#include <mach/mach.h>
#endif

//	Standard Library Includes
#include <stdio.h>

//==================================================================================================
//	CAPThread
//==================================================================================================

// returns the thread's priority as it was last set by the API
#define CAPTHREAD_SET_PRIORITY				0
// returns the thread's priority as it was last scheduled by the Kernel
#define CAPTHREAD_SCHEDULED_PRIORITY		1

//#define	Log_SetPriority						1

CAPThread::CAPThread(ThreadRoutine inThreadRoutine, void* inParameter, UInt32 inPriority, bool inFixedPriority, bool inAutoDelete, const char* inThreadName)
:
#if TARGET_OS_MAC
	mPThread(0),
    mSpawningThreadPriority(getScheduledPriority(pthread_self(), CAPTHREAD_SET_PRIORITY)),
#elif TARGET_OS_WIN32
	mThreadHandle(NULL),
	mThreadID(0),
#endif
	mThreadRoutine(inThreadRoutine),
	mThreadParameter(inParameter),
	mPriority(inPriority),
	mPeriod(0),
	mComputation(0),
	mConstraint(0),
	mIsPreemptible(true),
	mTimeConstraintSet(false),
	mFixedPriority(inFixedPriority),
	mAutoDelete(inAutoDelete)
{
	if(inThreadName != NULL)
	{
		strlcpy(mThreadName, inThreadName, kMaxThreadNameLength);
	}
	else
	{
		memset(mThreadName, 0, kMaxThreadNameLength);
	}
}

CAPThread::CAPThread(ThreadRoutine inThreadRoutine, void* inParameter, UInt32 inPeriod, UInt32 inComputation, UInt32 inConstraint, bool inIsPreemptible, bool inAutoDelete, const char* inThreadName)
:
#if TARGET_OS_MAC
	mPThread(0),
    mSpawningThreadPriority(getScheduledPriority(pthread_self(), CAPTHREAD_SET_PRIORITY)),
#elif TARGET_OS_WIN32
	mThreadHandle(NULL),
	mThreadID(0),
#endif
	mThreadRoutine(inThreadRoutine),
	mThreadParameter(inParameter),
	mPriority(kDefaultThreadPriority),
	mPeriod(inPeriod),
	mComputation(inComputation),
	mConstraint(inConstraint),
	mIsPreemptible(inIsPreemptible),
	mTimeConstraintSet(true),
	mFixedPriority(false),
	mAutoDelete(inAutoDelete)
{
	if(inThreadName != NULL)
	{
		strlcpy(mThreadName, inThreadName, kMaxThreadNameLength);
	}
	else
	{
		memset(mThreadName, 0, kMaxThreadNameLength);
	}
}

CAPThread::~CAPThread()
{
}

UInt32	CAPThread::GetScheduledPriority()
{
#if TARGET_OS_MAC
    return CAPThread::getScheduledPriority( mPThread, CAPTHREAD_SCHEDULED_PRIORITY );
#elif TARGET_OS_WIN32
	UInt32 theAnswer = 0;
	if(mThreadHandle != NULL)
	{
		theAnswer = GetThreadPriority(mThreadHandle);
	}
	return theAnswer;
#endif
}

UInt32	CAPThread::GetScheduledPriority(NativeThread thread)
{
#if TARGET_OS_MAC
    return getScheduledPriority( thread, CAPTHREAD_SCHEDULED_PRIORITY );
#elif TARGET_OS_WIN32
	return 0;	// ???
#endif
}

void	CAPThread::SetPriority(UInt32 inPriority, bool inFixedPriority)
{
	mPriority = inPriority;
	mTimeConstraintSet = false;
	mFixedPriority = inFixedPriority;
#if TARGET_OS_MAC
	if(mPThread != 0)
	{
		SetPriority(mPThread, mPriority, mFixedPriority);
    } 
#elif TARGET_OS_WIN32
	if(mThreadID != NULL)
	{
		SetPriority(mThreadID, mPriority, mFixedPriority);
	}
#endif
}

void	CAPThread::SetPriority(NativeThread inThread, UInt32 inPriority, bool inFixedPriority)
{
#if TARGET_OS_MAC
	if(inThread != 0)
	{
		kern_return_t theError = 0;
		
		//	set whether or not this is a fixed priority thread
		if (inFixedPriority)
		{
			thread_extended_policy_data_t theFixedPolicy = { false };
			theError = thread_policy_set(pthread_mach_thread_np(inThread), THREAD_EXTENDED_POLICY, (thread_policy_t)&theFixedPolicy, THREAD_EXTENDED_POLICY_COUNT);
			AssertNoKernelError(theError, "CAPThread::SetPriority: failed to set the fixed-priority policy");
		}
		
		//	set the thread's absolute priority which is relative to the priority on which thread_policy_set() is called
		UInt32 theCurrentThreadPriority = getScheduledPriority(pthread_self(), CAPTHREAD_SET_PRIORITY);
        thread_precedence_policy_data_t thePrecedencePolicy = { static_cast<integer_t>(inPriority - theCurrentThreadPriority) };
		theError = thread_policy_set(pthread_mach_thread_np(inThread), THREAD_PRECEDENCE_POLICY, (thread_policy_t)&thePrecedencePolicy, THREAD_PRECEDENCE_POLICY_COUNT);
        AssertNoKernelError(theError, "CAPThread::SetPriority: failed to set the precedence policy");
		
		#if	Log_SetPriority
			DebugMessageN4("CAPThread::SetPriority: requsted: %lu spawning: %lu current: %lu assigned: %d", mPriority, mSpawningThreadPriority, theCurrentThreadPriority, thePrecedencePolicy.importance);
		#endif
    } 
#elif TARGET_OS_WIN32
	if(inThread != NULL)
	{
		HANDLE hThread = OpenThread(NULL, FALSE, inThread);
		if(hThread != NULL) {
			SetThreadPriority(hThread, inPriority);
			CloseHandle(hThread);
		}
	}
#endif
}

void	CAPThread::SetTimeConstraints(UInt32 inPeriod, UInt32 inComputation, UInt32 inConstraint, bool inIsPreemptible)
{
	mPeriod = inPeriod;
	mComputation = inComputation;
	mConstraint = inConstraint;
	mIsPreemptible = inIsPreemptible;
	mTimeConstraintSet = true;
#if TARGET_OS_MAC
	if(mPThread != 0)
	{
		thread_time_constraint_policy_data_t thePolicy;
		thePolicy.period = mPeriod;
		thePolicy.computation = mComputation;
		thePolicy.constraint = mConstraint;
		thePolicy.preemptible = mIsPreemptible;
		AssertNoError(thread_policy_set(pthread_mach_thread_np(mPThread), THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&thePolicy, THREAD_TIME_CONSTRAINT_POLICY_COUNT), "CAPThread::SetTimeConstraints: thread_policy_set failed");
	}
#elif TARGET_OS_WIN32
	if(mThreadHandle != NULL)
	{
		SetThreadPriority(mThreadHandle, THREAD_PRIORITY_TIME_CRITICAL);
	}
#endif
}

void	CAPThread::Start()
{
#if TARGET_OS_MAC
	Assert(mPThread == 0, "CAPThread::Start: can't start because the thread is already running");
	if(mPThread == 0)
	{
		OSStatus			theResult;
		pthread_attr_t		theThreadAttributes;
		
		theResult = pthread_attr_init(&theThreadAttributes);
		ThrowIf(theResult != 0, CAException(theResult), "CAPThread::Start: Thread attributes could not be created.");
		
		theResult = pthread_attr_setdetachstate(&theThreadAttributes, PTHREAD_CREATE_DETACHED);
		ThrowIf(theResult != 0, CAException(theResult), "CAPThread::Start: A thread could not be created in the detached state.");
		
		theResult = pthread_create(&mPThread, &theThreadAttributes, (ThreadRoutine)CAPThread::Entry, this);
		ThrowIf(theResult != 0 || !mPThread, CAException(theResult), "CAPThread::Start: Could not create a thread.");
		
		pthread_attr_destroy(&theThreadAttributes);
		
	}
#elif TARGET_OS_WIN32
	Assert(mThreadID == 0, "CAPThread::Start: can't start because the thread is already running");
	if(mThreadID == 0)
	{
		//	clean up the existing thread handle
		if(mThreadHandle != NULL)
		{
			CloseHandle(mThreadHandle);
			mThreadHandle = NULL;
		}
		
		//	create a new thread
		mThreadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Entry, this, 0, &mThreadID);
		ThrowIf(mThreadHandle == NULL, CAException(GetLastError()), "CAPThread::Start: Could not create a thread.");
	}
#endif
}

#if TARGET_OS_MAC

void*	CAPThread::Entry(CAPThread* inCAPThread)
{
	void* theAnswer = NULL;

#if TARGET_OS_MAC
	inCAPThread->mPThread = pthread_self();
#elif TARGET_OS_WIN32
	// do we need to do something here?
#endif
	
#if	!TARGET_OS_IPHONE && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
	if(inCAPThread->mThreadName[0] != 0)
	{
		pthread_setname_np(inCAPThread->mThreadName);
	}
#endif

	try 
	{
		if(inCAPThread->mTimeConstraintSet)
		{
			inCAPThread->SetTimeConstraints(inCAPThread->mPeriod, inCAPThread->mComputation, inCAPThread->mConstraint, inCAPThread->mIsPreemptible);
		}
		else
		{
			inCAPThread->SetPriority(inCAPThread->mPriority, inCAPThread->mFixedPriority);
		}

		if(inCAPThread->mThreadRoutine != NULL)
		{
			theAnswer = inCAPThread->mThreadRoutine(inCAPThread->mThreadParameter);
		}
	}
	catch (...)
	{
		// what should be done here?
	}
	inCAPThread->mPThread = 0;
	if (inCAPThread->mAutoDelete)
		delete inCAPThread;
	return theAnswer;
}

UInt32 CAPThread::getScheduledPriority(pthread_t inThread, int inPriorityKind)
{
    thread_basic_info_data_t			threadInfo;
	policy_info_data_t					thePolicyInfo;
	unsigned int						count;

	if (inThread == NULL)
		return 0;
    
    // get basic info
    count = THREAD_BASIC_INFO_COUNT;
    thread_info (pthread_mach_thread_np (inThread), THREAD_BASIC_INFO, (thread_info_t)&threadInfo, &count);
    
	switch (threadInfo.policy) {
		case POLICY_TIMESHARE:
			count = POLICY_TIMESHARE_INFO_COUNT;
			thread_info(pthread_mach_thread_np (inThread), THREAD_SCHED_TIMESHARE_INFO, (thread_info_t)&(thePolicyInfo.ts), &count);
            if (inPriorityKind == CAPTHREAD_SCHEDULED_PRIORITY) {
                return static_cast<UInt32>(thePolicyInfo.ts.cur_priority);
            }
            return static_cast<UInt32>(thePolicyInfo.ts.base_priority);
            break;
            
        case POLICY_FIFO:
			count = POLICY_FIFO_INFO_COUNT;
			thread_info(pthread_mach_thread_np (inThread), THREAD_SCHED_FIFO_INFO, (thread_info_t)&(thePolicyInfo.fifo), &count);
            if ( (thePolicyInfo.fifo.depressed) && (inPriorityKind == CAPTHREAD_SCHEDULED_PRIORITY) ) {
                return static_cast<UInt32>(thePolicyInfo.fifo.depress_priority);
            }
            return static_cast<UInt32>(thePolicyInfo.fifo.base_priority);
            break;
            
		case POLICY_RR:
			count = POLICY_RR_INFO_COUNT;
			thread_info(pthread_mach_thread_np (inThread), THREAD_SCHED_RR_INFO, (thread_info_t)&(thePolicyInfo.rr), &count);
			if ( (thePolicyInfo.rr.depressed) && (inPriorityKind == CAPTHREAD_SCHEDULED_PRIORITY) ) {
                return static_cast<UInt32>(thePolicyInfo.rr.depress_priority);
            }
            return static_cast<UInt32>(thePolicyInfo.rr.base_priority);
            break;
	}
    
    return 0;
}

#elif TARGET_OS_WIN32

UInt32 WINAPI	CAPThread::Entry(CAPThread* inCAPThread)
{
	UInt32 theAnswer = 0;

	try 
	{
		if(inCAPThread->mTimeConstraintSet)
		{
			inCAPThread->SetTimeConstraints(inCAPThread->mPeriod, inCAPThread->mComputation, inCAPThread->mConstraint, inCAPThread->mIsPreemptible);
		}
		else
		{
			inCAPThread->SetPriority(inCAPThread->mPriority, inCAPThread->mFixedPriority);
		}

		if(inCAPThread->mThreadRoutine != NULL)
		{
			theAnswer = reinterpret_cast<UInt32>(inCAPThread->mThreadRoutine(inCAPThread->mThreadParameter));
		}
		inCAPThread->mThreadID = 0;
	}
	catch (...)
	{
		// what should be done here?
	}
	CloseHandle(inCAPThread->mThreadHandle);
	inCAPThread->mThreadHandle = NULL;
	if (inCAPThread->mAutoDelete)
		delete inCAPThread;
	return theAnswer;
}

extern "C"
Boolean CompareAndSwap(UInt32 inOldValue, UInt32 inNewValue, UInt32* inOldValuePtr)
{
	return InterlockedCompareExchange((volatile LONG*)inOldValuePtr, inNewValue, inOldValue) == inOldValue;
}

#endif

void	CAPThread::SetName(const char* inThreadName)
{
	if(inThreadName != NULL)
	{
		strlcpy(mThreadName, inThreadName, kMaxThreadNameLength);
	}
	else
	{
		memset(mThreadName, 0, kMaxThreadNameLength);
	}
}

#if CoreAudio_Debug
void	CAPThread::DebugPriority(const char *label)
{
#if !TARGET_OS_WIN32
	if (mTimeConstraintSet)
		printf("CAPThread::%s %p: pri=<time constraint>, spawning pri=%d, scheduled pri=%d\n", label, this, 
		(int)mSpawningThreadPriority, (mPThread != NULL) ? (int)GetScheduledPriority() : -1);
	else
		printf("CAPThread::%s %p: pri=%d%s, spawning pri=%d, scheduled pri=%d\n", label, this, (int)mPriority, mFixedPriority ? " fixed" : "", 
		(int)mSpawningThreadPriority, (mPThread != NULL) ? (int)GetScheduledPriority() : -1);
#else
	if (mTimeConstraintSet)
	{
		printf("CAPThread::%s %p: pri=<time constraint>, spawning pri=%d, scheduled pri=%d\n", label, this, 
		(int)mPriority, (mThreadHandle != NULL) ? (int)GetScheduledPriority() : -1);
	}
	else
	{
		printf("CAPThread::%s %p: pri=%d%s, spawning pri=%d, scheduled pri=%d\n", label, this, (int)mPriority, mFixedPriority ? " fixed" : "", 
		(int)mPriority, (mThreadHandle != NULL) ? (int)GetScheduledPriority() : -1);
	}
#endif
}
#endif

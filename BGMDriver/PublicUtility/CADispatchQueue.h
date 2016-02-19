/*
     File: CADispatchQueue.h 
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
/*==================================================================================================
	CADispatchQueue.h
==================================================================================================*/
#if !defined(__CADispatchQueue_h__)
#define __CADispatchQueue_h__

//==================================================================================================
//	Includes
//==================================================================================================

//	System Includes
#include <CoreFoundation/CFString.h>
#include <dispatch/dispatch.h>

//	Standard Library Includes
#include <functional>
#include <vector>

/*==================================================================================================
	CADispatchQueue
	
	This class provides a wrapper for a libdispatch dispatch queue and several kinds of event
	sources such as MIG servers, port death notifications and other mach port related support. Being
	a libdispatch client, the unit of work is represented as either a function pointer and context
	pointer pair or as a Block.
	
	One thing to keep in mind when using a Block-based construct is that you get a copy of the
	pointer but you do not get a copy of the memory that the pointer is pointing at. The net effect
	is that if you have a task that frees the memory that is referenced by a subsequent task,
	this second task would crash when dereferencing the pointer. This means that every task
	needs to include some code to validate any pointers it is using before dereferencing them.
	
	A common example of this problem comes up with C++ objects. Suppose you have an instance method
	that creates a Block that calls other instance methods or accesses the object's fields. Suppose
	further that the Block is submitted to a dispatch queue for execution. If the object gets
	deallocated before the Block can run, the Block will crash. Thus, the Block needs to validate
	that the "this" pointer is still valid when it executes.
	
	Another place where this comes up often is when attempting to implment a function by dispatching
	the work asynchronously. Any arguments to the function that point to the stack will be invalid.
	This is particularly troublesome inside of a MIG function.
	
	Still another common issue with using Blocks and dispatch functions with C++ is that it is vital
	that no exceptions ever leave a Block or a dispatch function. If an exception was thrown out of
	a block, the result would be undefined and probably would result in the program calling
	the terminate() function in the standard C++ library (whose default implementation is to call
	abort(3)). Given that, all Blocks and dispatch functions that might end up throwing an exception
	need to catch those exceptions.
==================================================================================================*/

class CADispatchQueue
{

#pragma mark Construction/Destruction
public:
										CADispatchQueue(const char* inName);
										CADispatchQueue(CFStringRef inName);
										CADispatchQueue(CFStringRef inPattern, CFStringRef inName);
	virtual								~CADispatchQueue();

private:
										CADispatchQueue(const CADispatchQueue&);
	CADispatchQueue&					operator=(const CADispatchQueue&);
	
#pragma mark Execution Operations
public:
	void								Dispatch(bool inDoSync, dispatch_block_t inTask) const;
	void								Dispatch(UInt64 inNanoseconds, dispatch_block_t inTask) const;
	
	void								Dispatch(bool inDoSync, void* inTaskContext, dispatch_function_t inTask) const;
	void								Dispatch(UInt64 inNanoseconds, void* inTaskContext, dispatch_function_t inTask) const;

	static void							Dispatch_Global(dispatch_queue_priority_t inQueuePriority, bool inDoSync, dispatch_block_t inTask);
	static void							Dispatch_Global(dispatch_queue_priority_t inQueuePriority, UInt64 inNanoseconds, dispatch_block_t inTask);
	
	static void							Dispatch_Global(dispatch_queue_priority_t inQueuePriority, bool inDoSync, void* inTaskContext, dispatch_function_t inTask);
	static void							Dispatch_Global(dispatch_queue_priority_t inQueuePriority, UInt64 inNanoseconds, void* inTaskContext, dispatch_function_t inTask);

	static void							Dispatch_Main(bool inDoSync, dispatch_block_t inTask);
	static void							Dispatch_Main(UInt64 inNanoseconds, dispatch_block_t inTask);
	
	static void							Dispatch_Main(bool inDoSync, void* inTaskContext, dispatch_function_t inTask);
	static void							Dispatch_Main(UInt64 inNanoseconds, void* inTaskContext, dispatch_function_t inTask);

#pragma mark Event Sources
public:
	void								InstallMachPortDeathNotification(mach_port_t inMachPort, dispatch_block_t inNotificationTask);
	void								RemoveMachPortDeathNotification(mach_port_t inMachPort);
	
	void								InstallMachPortReceiver(mach_port_t inMachPort, dispatch_block_t inMessageTask);
	void								RemoveMachPortReceiver(mach_port_t inMachPort, dispatch_block_t inCompletionTask);
	void								RemoveMachPortReceiver(mach_port_t inMachPort, bool inDestroySendRight, bool inDestroyReceiveRight);

#pragma mark Implementation
public:
	dispatch_queue_t					GetDispatchQueue() const;
	
	static CADispatchQueue&				GetGlobalSerialQueue();
	
protected:	
	static void							InitializeGlobalSerialQueue(void*);

	struct								EventSource
	{
		dispatch_source_t				mDispatchSource;
		mach_port_t						mMachPort;
		
										EventSource();
										EventSource(dispatch_source_t inDispatchSource, mach_port_t inMachPort);
										EventSource(const EventSource& inEventSource);
		EventSource&					operator=(const EventSource& inEventSource);
										~EventSource();
		void							Retain();
		void							Release();
	};
	typedef std::vector<EventSource>	EventSourceList;
	
	dispatch_queue_t					mDispatchQueue;
	EventSourceList						mPortDeathList;
	EventSourceList						mMachPortReceiverList;

	static CADispatchQueue*				sGlobalSerialQueue;
	static dispatch_once_t				sGlobalSerialQueueInitialized;

};

//==================================================================================================
#pragma mark CADispatchQueue Inline Method Implementations
//==================================================================================================

inline dispatch_queue_t	CADispatchQueue::GetDispatchQueue() const
{
	return mDispatchQueue;
}

inline CADispatchQueue::EventSource::EventSource()
:
	mDispatchSource(NULL),
	mMachPort(MACH_PORT_NULL)
{
}

inline CADispatchQueue::EventSource::EventSource(dispatch_source_t inDispatchSource, mach_port_t inMachPort)
:
	mDispatchSource(inDispatchSource),
	mMachPort(inMachPort)
{
}

inline CADispatchQueue::EventSource::EventSource(const EventSource& inEventSource)
:
	mDispatchSource(inEventSource.mDispatchSource),
	mMachPort(inEventSource.mMachPort)
{
	Retain();
}

inline CADispatchQueue::EventSource&	CADispatchQueue::EventSource::operator=(const EventSource& inEventSource)
{
	Release();
	mDispatchSource = inEventSource.mDispatchSource;
	mMachPort = inEventSource.mMachPort;
	Retain();
	return *this;
}

inline CADispatchQueue::EventSource::~EventSource()
{
	Release();
}

inline void	CADispatchQueue::EventSource::Retain()
{
	if(mDispatchSource != NULL)
	{
		dispatch_retain(mDispatchSource);
	}
}

inline void	CADispatchQueue::EventSource::Release()
{
	if(mDispatchSource != NULL)
	{
		dispatch_release(mDispatchSource);
		mDispatchSource = NULL;
	}
}

#endif	//	__CADispatchQueue_h__

/*
     File: CADispatchQueue.cpp 
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
	CADispatchQueue.cpp
==================================================================================================*/

//==================================================================================================
//	Includes
//==================================================================================================

//	Self Include
#include "CADispatchQueue.h"

//	PublicUtility Includes
#include "CACFString.h"
#include "CADebugMacros.h"
#include "CAException.h"
#include "CAHostTimeBase.h"

//	System Includes
#include <mach/mach.h>

//	Standard Library Includes
#include <algorithm>

//==================================================================================================
//	CADispatchQueue
//==================================================================================================

CADispatchQueue::CADispatchQueue(const char* inName)
:
	mDispatchQueue(NULL),
	mPortDeathList(),
	mMachPortReceiverList()
{
	mDispatchQueue = dispatch_queue_create(inName, NULL);
	ThrowIfNULL(mDispatchQueue, CAException('what'), "CADispatchQueue::CADispatchQueue: failed to create the dispatch queue");
}

CADispatchQueue::CADispatchQueue(CFStringRef inName)
:
	mDispatchQueue(NULL),
	mPortDeathList(),
	mMachPortReceiverList()
{
	CACFString theCFName(inName, false);
	char theName[256];
	UInt32 theSize = 256;
	theCFName.GetCString(theName, theSize);
	mDispatchQueue = dispatch_queue_create(theName, NULL);
	ThrowIfNULL(mDispatchQueue, CAException('what'), "CADispatchQueue::CADispatchQueue: failed to create the dispatch queue");
}

CADispatchQueue::CADispatchQueue(CFStringRef inPattern, CFStringRef inName)
:
	mDispatchQueue(NULL),
	mPortDeathList(),
	mMachPortReceiverList()
{
	CACFString theCFName(CFStringCreateWithFormat(NULL, NULL, inPattern, inName), true);
	char theName[256];
	UInt32 theSize = 256;
	theCFName.GetCString(theName, theSize);
	mDispatchQueue = dispatch_queue_create(theName, NULL);
	ThrowIfNULL(mDispatchQueue, CAException('what'), "CADispatchQueue::CADispatchQueue: failed to create the dispatch queue");
}

CADispatchQueue::~CADispatchQueue()
{
	//	Clean up the port death watchers if any are still around. Note that we do this explicitly to
	//	be sure the source is cleaned up before the queue is released
	mPortDeathList.clear();
	Assert(mMachPortReceiverList.size() == 0, "CADispatchQueue::~CADispatchQueue: Implicitly removing the mach port receviers. It is best to explicitly call RemoveMachPortRecevier().");
	mMachPortReceiverList.clear();
	
	//	release the dispatch queue
	dispatch_release(mDispatchQueue);
}

void	CADispatchQueue::Dispatch(bool inDoSync, dispatch_block_t inTask) const
{
	if(inDoSync)
	{
		//	Executing a task synchronously while already on the dispatch queue will result in a deadlock
		dispatch_sync(mDispatchQueue, inTask);
	}
	else
	{
		dispatch_async(mDispatchQueue, inTask);
	}
}

void	CADispatchQueue::Dispatch(UInt64 inNanoseconds, dispatch_block_t inTask) const
{
	if(inNanoseconds == 0)
	{
		dispatch_async(mDispatchQueue, inTask);
	}
	else
	{
		dispatch_after(dispatch_time(0, static_cast<int64_t>(CAHostTimeBase::ConvertFromNanos(inNanoseconds))), mDispatchQueue, inTask);
	}
}

void	CADispatchQueue::Dispatch(bool inDoSync, void* inTaskContext, dispatch_function_t inTask) const
{
	if(inDoSync)
	{
		//	Executing a task synchronously while already on the dispatch queue will result in a deadlock
		dispatch_sync_f(mDispatchQueue, inTaskContext, inTask);
	}
	else
	{
		dispatch_async_f(mDispatchQueue, inTaskContext, inTask);
	}
}

void	CADispatchQueue::Dispatch(UInt64 inNanoseconds, void* inTaskContext, dispatch_function_t inTask) const
{
	if(inNanoseconds == 0)
	{
		dispatch_async_f(mDispatchQueue, inTaskContext, inTask);
	}
	else
	{
		dispatch_after_f(dispatch_time(0, static_cast<int64_t>(CAHostTimeBase::ConvertFromNanos(inNanoseconds))), mDispatchQueue, inTaskContext, inTask);
	}
}

void	CADispatchQueue::Dispatch_Global(dispatch_queue_priority_t inQueuePriority, bool inDoSync, dispatch_block_t inTask)
{
	dispatch_queue_t theDispatchQueue = dispatch_get_global_queue(inQueuePriority, 0);
	if(inDoSync)
	{
		//	Executing a task synchronously while already on the dispatch queue will result in a deadlock
		dispatch_sync(theDispatchQueue, inTask);
	}
	else
	{
		dispatch_async(theDispatchQueue, inTask);
	}
}

void	CADispatchQueue::Dispatch_Global(dispatch_queue_priority_t inQueuePriority, UInt64 inNanoseconds, dispatch_block_t inTask)
{
	dispatch_queue_t theDispatchQueue = dispatch_get_global_queue(inQueuePriority, 0);
	if(inNanoseconds == 0)
	{
		dispatch_async(theDispatchQueue, inTask);
	}
	else
	{
		dispatch_after(dispatch_time(0, static_cast<int64_t>(CAHostTimeBase::ConvertFromNanos(inNanoseconds))), theDispatchQueue, inTask);
	}
}

void	CADispatchQueue::Dispatch_Global(dispatch_queue_priority_t inQueuePriority, bool inDoSync, void* inTaskContext, dispatch_function_t inTask)
{
	dispatch_queue_t theDispatchQueue = dispatch_get_global_queue(inQueuePriority, 0);
	if(inDoSync)
	{
		//	Executing a task synchronously while already on the dispatch queue will result in a deadlock
		dispatch_sync_f(theDispatchQueue, inTaskContext, inTask);
	}
	else
	{
		dispatch_async_f(theDispatchQueue, inTaskContext, inTask);
	}
}

void	CADispatchQueue::Dispatch_Global(dispatch_queue_priority_t inQueuePriority, UInt64 inNanoseconds, void* inTaskContext, dispatch_function_t inTask)
{
	dispatch_queue_t theDispatchQueue = dispatch_get_global_queue(inQueuePriority, 0);
	if(inNanoseconds == 0)
	{
		dispatch_async_f(theDispatchQueue, inTaskContext, inTask);
	}
	else
	{
		dispatch_after_f(dispatch_time(0, static_cast<int64_t>(CAHostTimeBase::ConvertFromNanos(inNanoseconds))), theDispatchQueue, inTaskContext, inTask);
	}
}

void	CADispatchQueue::Dispatch_Main(bool inDoSync, dispatch_block_t inTask)
{
	dispatch_queue_t theDispatchQueue = dispatch_get_main_queue();
	if(inDoSync)
	{
		//	Executing a task synchronously while already on the dispatch queue will result in a deadlock
		dispatch_sync(theDispatchQueue, inTask);
	}
	else
	{
		dispatch_async(theDispatchQueue, inTask);
	}
}

void	CADispatchQueue::Dispatch_Main(UInt64 inNanoseconds, dispatch_block_t inTask)
{
	dispatch_queue_t theDispatchQueue = dispatch_get_main_queue();
	if(inNanoseconds == 0)
	{
		dispatch_async(theDispatchQueue, inTask);
	}
	else
	{
		dispatch_after(dispatch_time(0, static_cast<int64_t>(CAHostTimeBase::ConvertFromNanos(inNanoseconds))), theDispatchQueue, inTask);
	}
}

void	CADispatchQueue::Dispatch_Main(bool inDoSync, void* inTaskContext, dispatch_function_t inTask)
{
	dispatch_queue_t theDispatchQueue = dispatch_get_main_queue();
	if(inDoSync)
	{
		//	Executing a task synchronously while already on the dispatch queue will result in a deadlock
		dispatch_sync_f(theDispatchQueue, inTaskContext, inTask);
	}
	else
	{
		dispatch_async_f(theDispatchQueue, inTaskContext, inTask);
	}
}

void	CADispatchQueue::Dispatch_Main(UInt64 inNanoseconds, void* inTaskContext, dispatch_function_t inTask)
{
	dispatch_queue_t theDispatchQueue = dispatch_get_main_queue();
	if(inNanoseconds == 0)
	{
		dispatch_async_f(theDispatchQueue, inTaskContext, inTask);
	}
	else
	{
		dispatch_after_f(dispatch_time(0, static_cast<int64_t>(CAHostTimeBase::ConvertFromNanos(inNanoseconds))), theDispatchQueue, inTaskContext, inTask);
	}
}

void	CADispatchQueue::InstallMachPortDeathNotification(mach_port_t inMachPort, dispatch_block_t inNotificationTask)
{
	ThrowIf(inMachPort == MACH_PORT_NULL, CAException('nope'), "CADispatchQueue::InstallMachPortDeathNotification: a mach port is required");
	
	//	 look in the list to see if we've already created an event source for it
	bool wasFound = false;
	EventSourceList::iterator theIterator = mPortDeathList.begin();
	while(!wasFound && (theIterator != mPortDeathList.end()))
	{
		wasFound = theIterator->mMachPort == inMachPort;
		if(!wasFound)
		{
			++theIterator;
		}
	}
	
	//	create and install the event source for the port
	if(!wasFound)
	{
		//	create an event source for the mach port
		dispatch_source_t theDispatchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_SEND, inMachPort, DISPATCH_MACH_SEND_DEAD, mDispatchQueue);
		ThrowIfNULL(theDispatchSource, CAException('what'), "CADispatchQueue::InstallMachPortDeathNotification: failed to create the mach port event source");
		
		//	install the event handler
		dispatch_source_set_event_handler(theDispatchSource, inNotificationTask);
		
		//	put the info in the list
		mPortDeathList.push_back(EventSource(theDispatchSource, inMachPort));
		
		//	resume the event source so that it can start handling messages and also so that the source can be released
		dispatch_resume(theDispatchSource);
	}
}

void	CADispatchQueue::RemoveMachPortDeathNotification(mach_port_t inMachPort)
{
	bool wasFound = false;
	EventSourceList::iterator theIterator = mPortDeathList.begin();
	while(!wasFound && (theIterator != mPortDeathList.end()))
	{
		wasFound = theIterator->mMachPort == inMachPort;
		if(!wasFound)
		{
			++theIterator;
		}
	}
	if(wasFound)
	{
		if(theIterator->mDispatchSource != NULL)
		{
			dispatch_source_cancel(theIterator->mDispatchSource);
		}
		mPortDeathList.erase(theIterator);
	}
}

void	CADispatchQueue::InstallMachPortReceiver(mach_port_t inMachPort, dispatch_block_t inMessageTask)
{
	ThrowIf(inMachPort == MACH_PORT_NULL, CAException('nope'), "CADispatchQueue::InstallMachPortReceiver: a mach port is required");
	
	//	 look in the list to see if we've already created an event source for it
	bool wasFound = false;
	EventSourceList::iterator theIterator = mMachPortReceiverList.begin();
	while(!wasFound && (theIterator != mMachPortReceiverList.end()))
	{
		wasFound = theIterator->mMachPort == inMachPort;
		if(!wasFound)
		{
			++theIterator;
		}
	}
	
	//	create and install the event source for the port
	if(!wasFound)
	{
		//	create an event source for the mach port
		dispatch_source_t theDispatchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, inMachPort, 0, mDispatchQueue);
		ThrowIfNULL(theDispatchSource, CAException('what'), "CADispatchQueue::InstallMachPortReceiver: failed to create the mach port event source");
		
		//	install an event handler that maps the mach messages to the MIG server function
		dispatch_source_set_event_handler(theDispatchSource, inMessageTask);
		
		//	put the info in the list
		mMachPortReceiverList.push_back(EventSource(theDispatchSource, inMachPort));
		
		//	resume the event source so that it can start handling messages and also so that the source can be released
		dispatch_resume(theDispatchSource);
	}
}

void	CADispatchQueue::RemoveMachPortReceiver(mach_port_t inMachPort, dispatch_block_t inCompletionTask)
{
	bool wasFound = false;
	EventSourceList::iterator theIterator = mMachPortReceiverList.begin();
	while(!wasFound && (theIterator != mMachPortReceiverList.end()))
	{
		wasFound = theIterator->mMachPort == inMachPort;
		if(!wasFound)
		{
			++theIterator;
		}
	}
	if(wasFound)
	{
		if(theIterator->mDispatchSource != NULL)
		{
			//	Set the cancel handler to the completion block. Note that the mach port cannot be freed
			//	before the completion block runs due to a race condition. See the note in the comments
			//	dispatch_source_set_cancel_handler in <dispatch/source.h>.
			if(inCompletionTask != 0)
			{
				dispatch_source_set_cancel_handler(theIterator->mDispatchSource, inCompletionTask);
			}
		
			dispatch_source_cancel(theIterator->mDispatchSource);
		}
		mMachPortReceiverList.erase(theIterator);
	}
}

void	CADispatchQueue::RemoveMachPortReceiver(mach_port_t inMachPort, bool inDestroySendRight, bool inDestroyReceiveRight)
{
	RemoveMachPortReceiver(inMachPort,	^{
											if(inDestroySendRight)
											{
												kern_return_t theError = mach_port_deallocate(mach_task_self(), inMachPort);
												AssertNoKernelError(theError, "CADispatchQueue::RemoveMachPortReceiver: deallocating the send right failed");
											}
											if(inDestroyReceiveRight)
											{
												kern_return_t theError = mach_port_mod_refs(mach_task_self(), inMachPort, MACH_PORT_RIGHT_RECEIVE, -1);
												AssertNoKernelError(theError, "CADispatchQueue::RemoveMachPortReceiver: deallocating the receive right failed");
											}
										});
}

CADispatchQueue&	CADispatchQueue::GetGlobalSerialQueue()
{
	dispatch_once_f(&sGlobalSerialQueueInitialized, NULL, InitializeGlobalSerialQueue);
	ThrowIfNULL(sGlobalSerialQueue, CAException('nope'), "CADispatchQueue::GetGlobalSerialQueue: there is no global serial queue");
	return *sGlobalSerialQueue;
}

void	CADispatchQueue::InitializeGlobalSerialQueue(void*)
{
	try
	{
		sGlobalSerialQueue = new CADispatchQueue("com.apple.audio.CADispatchQueue.SerialQueue");
	}
	catch(...)
	{
		sGlobalSerialQueue = NULL;
	}
}

CADispatchQueue*	CADispatchQueue::sGlobalSerialQueue = NULL;
dispatch_once_t		CADispatchQueue::sGlobalSerialQueueInitialized = 0;

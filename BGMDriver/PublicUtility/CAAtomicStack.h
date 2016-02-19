/*
     File: CAAtomicStack.h
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
#ifndef __CAAtomicStack_h__
#define __CAAtomicStack_h__

#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <libkern/OSAtomic.h>
#else
	#include <CAAtomic.h>
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_4
	#include <CoreServices/CoreServices.h>
#endif

//  linked list LIFO or FIFO (pop_all_reversed) stack, elements are pushed and popped atomically
//  class T must implement T *& next().
template <class T>
class TAtomicStack {
public:
	TAtomicStack() : mHead(NULL) { }
	
	// non-atomic routines, for use when initializing/deinitializing, operate NON-atomically
	void	push_NA(T *item)
	{
		item->next() = mHead;
		mHead = item;
	}
	
	T *		pop_NA()
	{
		T *result = mHead;
		if (result)
			mHead = result->next();
		return result;
	}
	
	bool	empty() const { return mHead == NULL; }
	
	T *		head() { return mHead; }
	
	// atomic routines
	void	push_atomic(T *item)
	{
		T *head_;
		do {
			head_ = mHead;
			item->next() = head_;
		} while (!compare_and_swap(head_, item, &mHead));
	}
	
	void	push_multiple_atomic(T *item)
		// pushes entire linked list headed by item
	{
		T *head_, *p = item, *tail;
		// find the last one -- when done, it will be linked to head
		do {
			tail = p;
			p = p->next();
		} while (p);
		do {
			head_ = mHead;
			tail->next() = head_;
		} while (!compare_and_swap(head_, item, &mHead));
	}
	
	T *		pop_atomic_single_reader()
		// this may only be used when only one thread may potentially pop from the stack.
		// if multiple threads may pop, this suffers from the ABA problem.
		// <rdar://problem/4606346> TAtomicStack suffers from the ABA problem
	{
		T *result;
		do {
			if ((result = mHead) == NULL)
				break;
		} while (!compare_and_swap(result, result->next(), &mHead));
		return result;
	}
	
	T *		pop_atomic()
		// This is inefficient for large linked lists.
		// prefer pop_all() to a series of calls to pop_atomic.
		// push_multiple_atomic has to traverse the entire list.
	{
		T *result = pop_all();
		if (result) {
			T *next = result->next();
			if (next)
				// push all the remaining items back onto the stack
				push_multiple_atomic(next);
		}
		return result;
	}
	
	T *		pop_all()
	{
		T *result;
		do {
			if ((result = mHead) == NULL)
				break;
		} while (!compare_and_swap(result, NULL, &mHead));
		return result;
	}
	
	T*		pop_all_reversed()
	{
		TAtomicStack<T> reversed;
		T *p = pop_all(), *next;
		while (p != NULL) {
			next = p->next();
			reversed.push_NA(p);
			p = next;
		}
		return reversed.mHead;
	}
	
	static bool	compare_and_swap(T *oldvalue, T *newvalue, T **pvalue)
	{
#if TARGET_OS_MAC
	#if __LP64__
			return ::OSAtomicCompareAndSwap64Barrier(int64_t(oldvalue), int64_t(newvalue), (int64_t *)pvalue);
	#elif MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
			return ::OSAtomicCompareAndSwap32Barrier(int32_t(oldvalue), int32_t(newvalue), (int32_t *)pvalue);
	#else
			return ::CompareAndSwap(UInt32(oldvalue), UInt32(newvalue), (UInt32 *)pvalue);
	#endif
#else
			//return ::CompareAndSwap(UInt32(oldvalue), UInt32(newvalue), (UInt32 *)pvalue);
			return CAAtomicCompareAndSwap32Barrier(SInt32(oldvalue), SInt32(newvalue), (SInt32*)pvalue);
#endif
	}
	
protected:
	T *		mHead;
};

#if ((MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5) && !TARGET_OS_WIN32)
#include <libkern/OSAtomic.h>

class CAAtomicStack {
public:
	CAAtomicStack(size_t nextPtrOffset) : mNextPtrOffset(nextPtrOffset) {
		/*OSQueueHead h = OS_ATOMIC_QUEUE_INIT; mHead = h;*/
		mHead.opaque1 = 0; mHead.opaque2 = 0;
	}
	// a subset of the above
	void	push_atomic(void *p) { OSAtomicEnqueue(&mHead, p, mNextPtrOffset); }
	void	push_NA(void *p) { push_atomic(p); }

	void *	pop_atomic() { return OSAtomicDequeue(&mHead, mNextPtrOffset); }
	void *	pop_atomic_single_reader() { return pop_atomic(); }
	void *	pop_NA() { return pop_atomic(); }
	
private:
	OSQueueHead		mHead;
	size_t			mNextPtrOffset;
};

// a more efficient subset of TAtomicStack using OSQueue.
template <class T>
class TAtomicStack2 {
public:
	TAtomicStack2() {
		/*OSQueueHead h = OS_ATOMIC_QUEUE_INIT; mHead = h;*/
		mHead.opaque1 = 0; mHead.opaque2 = 0;
		mNextPtrOffset = -1;
	}
	void	push_atomic(T *item) {
		if (mNextPtrOffset < 0) {
			T **pnext = &item->next();	// hack around offsetof not working with C++
			mNextPtrOffset = (Byte *)pnext - (Byte *)item;
		}
		OSAtomicEnqueue(&mHead, item, mNextPtrOffset);
	}
	void	push_NA(T *item) { push_atomic(item); }

	T *		pop_atomic() { return (T *)OSAtomicDequeue(&mHead, mNextPtrOffset); }
	T *		pop_atomic_single_reader() { return pop_atomic(); }
	T *		pop_NA() { return pop_atomic(); }
	
	// caution: do not try to implement pop_all_reversed here. the writer could add new elements
	// while the reader is trying to pop old ones!
	
private:
	OSQueueHead		mHead;
	ssize_t			mNextPtrOffset;
};

#else

#define TAtomicStack2 TAtomicStack

#endif // MAC_OS_X_VERSION_MAX_ALLOWED && !TARGET_OS_WIN32

#endif // __CAAtomicStack_h__

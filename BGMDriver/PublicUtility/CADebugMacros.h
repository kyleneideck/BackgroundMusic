/*
     File: CADebugMacros.h 
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
#if !defined(__CADebugMacros_h__)
#define __CADebugMacros_h__

//=============================================================================
//	Includes
//=============================================================================

#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreAudio/CoreAudioTypes.h>
#else
	#include "CoreAudioTypes.h"
#endif

//=============================================================================
//	CADebugMacros
//=============================================================================

//#define	CoreAudio_StopOnFailure			1
//#define	CoreAudio_TimeStampMessages		1
//#define	CoreAudio_ThreadStampMessages	1
//#define	CoreAudio_FlushDebugMessages	1

#if TARGET_RT_BIG_ENDIAN
	#define	CA4CCToCString(the4CC)					{ ((char*)&the4CC)[0], ((char*)&the4CC)[1], ((char*)&the4CC)[2], ((char*)&the4CC)[3], 0 }
	#define CACopy4CCToCString(theCString, the4CC)	{ theCString[0] = ((char*)&the4CC)[0]; theCString[1] = ((char*)&the4CC)[1]; theCString[2] = ((char*)&the4CC)[2]; theCString[3] = ((char*)&the4CC)[3]; theCString[4] = 0; }
#else
	#define	CA4CCToCString(the4CC)					{ ((char*)&the4CC)[3], ((char*)&the4CC)[2], ((char*)&the4CC)[1], ((char*)&the4CC)[0], 0 }
	#define CACopy4CCToCString(theCString, the4CC)	{ theCString[0] = ((char*)&the4CC)[3]; theCString[1] = ((char*)&the4CC)[2]; theCString[2] = ((char*)&the4CC)[1]; theCString[3] = ((char*)&the4CC)[0]; theCString[4] = 0; }
#endif

//	This is a macro that does a sizeof and casts the result to a UInt32. This is useful for all the
//	places where -wshorten64-32 catches assigning a sizeof expression to a UInt32.
//	For want of a better place to park this, we'll park it here.
#define	SizeOf32(X)	((UInt32)sizeof(X))

//	This is a macro that does a offsetof and casts the result to a UInt32. This is useful for all the
//	places where -wshorten64-32 catches assigning an offsetof expression to a UInt32.
//	For want of a better place to park this, we'll park it here.
#define	OffsetOf32(X, Y)	((UInt32)offsetof(X, Y))

//	This macro casts the expression to a UInt32. It is called out specially to allow us to track casts
//	that have been added purely to avert -wshorten64-32 warnings on 64 bit platforms.
//	For want of a better place to park this, we'll park it here.
#define	ToUInt32(X)	((UInt32)(X))
#define	ToSInt32(X)	((SInt32)(X))

#pragma mark	Basic Definitions

#if	DEBUG || CoreAudio_Debug
	// can be used to break into debugger immediately, also see CADebugger
	#define BusError()		{ long* p=NULL; *p=0; }
	
	//	basic debugging print routines
	#if	TARGET_OS_MAC && !TARGET_API_MAC_CARBON
		extern void DebugStr(const unsigned char* debuggerMsg);
		#define	DebugMessage(msg)	DebugStr("\p"msg)
		#define DebugMessageN1(msg, N1)
		#define DebugMessageN2(msg, N1, N2)
		#define DebugMessageN3(msg, N1, N2, N3)
	#else
		#include "CADebugPrintf.h"
		
		#if	(CoreAudio_FlushDebugMessages && !CoreAudio_UseSysLog) || defined(CoreAudio_UseSideFile)
			#define	FlushRtn	,fflush(DebugPrintfFile)
		#else
			#define	FlushRtn
		#endif
		
		#if		CoreAudio_ThreadStampMessages
			#include <pthread.h>
			#include "CAHostTimeBase.h"
			#if TARGET_RT_64_BIT
				#define	DebugPrintfThreadIDFormat	"%16p"
			#else
				#define	DebugPrintfThreadIDFormat	"%8p"
			#endif
			#define	DebugMsg(inFormat, ...)	DebugPrintf("%17qd: " DebugPrintfThreadIDFormat " " inFormat, CAHostTimeBase::GetCurrentTimeInNanos(), pthread_self(), ## __VA_ARGS__) FlushRtn
		#elif	CoreAudio_TimeStampMessages
			#include "CAHostTimeBase.h"
			#define	DebugMsg(inFormat, ...)	DebugPrintf("%17qd: " inFormat, CAHostTimeBase::GetCurrentTimeInNanos(), ## __VA_ARGS__) FlushRtn
		#else
			#define	DebugMsg(inFormat, ...)	DebugPrintf(inFormat, ## __VA_ARGS__) FlushRtn
		#endif
	#endif
	void	DebugPrint(const char *fmt, ...);	// can be used like printf
	#ifndef DEBUGPRINT
		#define DEBUGPRINT(msg) DebugPrint msg		// have to double-parenthesize arglist (see Debugging.h)
	#endif
	#if VERBOSE
		#define vprint(msg) DEBUGPRINT(msg)
	#else
		#define vprint(msg)
	#endif
	
	// Original macro keeps its function of turning on and off use of CADebuggerStop() for both asserts and throws.
	// For backwards compat, it overrides any setting of the two sub-macros.
	#if	CoreAudio_StopOnFailure
		#include "CADebugger.h"
		#undef CoreAudio_StopOnAssert
		#define CoreAudio_StopOnAssert 1
		#undef CoreAudio_StopOnThrow
		#define CoreAudio_StopOnThrow 1
		#define STOP	CADebuggerStop()
	#else
		#define STOP
	#endif

	#if CoreAudio_StopOnAssert
		#if !CoreAudio_StopOnFailure
			#include "CADebugger.h"
			#define STOP
		#endif
		#define __ASSERT_STOP CADebuggerStop()
	#else
		#define __ASSERT_STOP
	#endif

	#if CoreAudio_StopOnThrow
		#if !CoreAudio_StopOnFailure
			#include "CADebugger.h"
			#define STOP
		#endif
		#define __THROW_STOP CADebuggerStop()
	#else
		#define __THROW_STOP
	#endif

#else
	#define	DebugMsg(inFormat, ...)
	#ifndef DEBUGPRINT
		#define DEBUGPRINT(msg)
	#endif
	#define vprint(msg)
	#define	STOP
	#define __ASSERT_STOP
	#define __THROW_STOP
#endif

//	Old-style numbered DebugMessage calls are implemented in terms of DebugMsg() now
#define	DebugMessage(msg)										DebugMsg(msg)
#define DebugMessageN1(msg, N1)									DebugMsg(msg, N1)
#define DebugMessageN2(msg, N1, N2)								DebugMsg(msg, N1, N2)
#define DebugMessageN3(msg, N1, N2, N3)							DebugMsg(msg, N1, N2, N3)
#define DebugMessageN4(msg, N1, N2, N3, N4)						DebugMsg(msg, N1, N2, N3, N4)
#define DebugMessageN5(msg, N1, N2, N3, N4, N5)					DebugMsg(msg, N1, N2, N3, N4, N5)
#define DebugMessageN6(msg, N1, N2, N3, N4, N5, N6)				DebugMsg(msg, N1, N2, N3, N4, N5, N6)
#define DebugMessageN7(msg, N1, N2, N3, N4, N5, N6, N7)			DebugMsg(msg, N1, N2, N3, N4, N5, N6, N7)
#define DebugMessageN8(msg, N1, N2, N3, N4, N5, N6, N7, N8)		DebugMsg(msg, N1, N2, N3, N4, N5, N6, N7, N8)
#define DebugMessageN9(msg, N1, N2, N3, N4, N5, N6, N7, N8, N9)	DebugMsg(msg, N1, N2, N3, N4, N5, N6, N7, N8, N9)

void	LogError(const char *fmt, ...);			// writes to syslog (and stderr if debugging)
void	LogWarning(const char *fmt, ...);		// writes to syslog (and stderr if debugging)

#define	NO_ACTION	(void)0

#if	DEBUG || CoreAudio_Debug

#pragma mark	Debug Macros

#define	Assert(inCondition, inMessage)													\
			if(!(inCondition))															\
			{																			\
				DebugMessage(inMessage);												\
				__ASSERT_STOP;																	\
			}

#define	AssertFileLine(inCondition, inMessage)											\
			if(!(inCondition))															\
			{																			\
				DebugMessageN3("%s, line %d: %s", __FILE__, __LINE__, inMessage);		\
				__ASSERT_STOP;															\
			}

#define	AssertNoError(inError, inMessage)												\
			{																			\
				SInt32 __Err = (inError);												\
				if(__Err != 0)															\
				{																		\
					char __4CC[5] = CA4CCToCString(__Err);								\
					DebugMessageN2(inMessage ", Error: %d (%s)", (int)__Err, __4CC);		\
					__ASSERT_STOP;														\
				}																		\
			}

#define	AssertNoKernelError(inError, inMessage)											\
			{																			\
				unsigned int __Err = (unsigned int)(inError);							\
				if(__Err != 0)															\
				{																		\
					DebugMessageN1(inMessage ", Error: 0x%X", __Err);					\
					__ASSERT_STOP;														\
				}																		\
			}

#define	AssertNotNULL(inPtr, inMessage)													\
			{																			\
				if((inPtr) == NULL)														\
				{																		\
					DebugMessage(inMessage);											\
					__ASSERT_STOP;														\
				}																		\
			}

#define	FailIf(inCondition, inHandler, inMessage)										\
			if(inCondition)																\
			{																			\
				DebugMessage(inMessage);												\
				STOP;																	\
				goto inHandler;															\
			}

#define	FailWithAction(inCondition, inAction, inHandler, inMessage)						\
			if(inCondition)																\
			{																			\
				DebugMessage(inMessage);												\
				STOP;																	\
				{ inAction; }															\
				goto inHandler;															\
			}

#define	FailIfNULL(inPointer, inAction, inHandler, inMessage)							\
			if((inPointer) == NULL)														\
			{																			\
				DebugMessage(inMessage);												\
				STOP;																	\
				{ inAction; }															\
				goto inHandler;															\
			}

#define	FailIfKernelError(inKernelError, inAction, inHandler, inMessage)				\
			{																			\
				unsigned int __Err = (inKernelError);									\
				if(__Err != 0)															\
				{																		\
					DebugMessageN1(inMessage ", Error: 0x%X", __Err);					\
					STOP;																\
					{ inAction; }														\
					goto inHandler;														\
				}																		\
			}

#define	FailIfError(inError, inAction, inHandler, inMessage)							\
			{																			\
				SInt32 __Err = (inError);												\
				if(__Err != 0)															\
				{																		\
					char __4CC[5] = CA4CCToCString(__Err);								\
					DebugMessageN2(inMessage ", Error: %ld (%s)", (long int)__Err, __4CC);	\
					STOP;																\
					{ inAction; }														\
					goto inHandler;														\
				}																		\
			}

#define	FailIfNoMessage(inCondition, inHandler, inMessage)								\
			if(inCondition)																\
			{																			\
				STOP;																	\
				goto inHandler;															\
			}

#define	FailWithActionNoMessage(inCondition, inAction, inHandler, inMessage)			\
			if(inCondition)																\
			{																			\
				STOP;																	\
				{ inAction; }															\
				goto inHandler;															\
			}

#define	FailIfNULLNoMessage(inPointer, inAction, inHandler, inMessage)					\
			if((inPointer) == NULL)														\
			{																			\
				STOP;																	\
				{ inAction; }															\
				goto inHandler;															\
			}

#define	FailIfKernelErrorNoMessage(inKernelError, inAction, inHandler, inMessage)		\
			{																			\
				unsigned int __Err = (inKernelError);									\
				if(__Err != 0)															\
				{																		\
					STOP;																\
					{ inAction; }														\
					goto inHandler;														\
				}																		\
			}

#define	FailIfErrorNoMessage(inError, inAction, inHandler, inMessage)					\
			{																			\
				SInt32 __Err = (inError);												\
				if(__Err != 0)															\
				{																		\
					STOP;																\
					{ inAction; }														\
					goto inHandler;														\
				}																		\
			}

#if defined(__cplusplus)

#define Throw(inException)  __THROW_STOP; throw (inException)

#define	ThrowIf(inCondition, inException, inMessage)									\
			if(inCondition)																\
			{																			\
				DebugMessage(inMessage);												\
				Throw(inException);														\
			}

#define	ThrowIfNULL(inPointer, inException, inMessage)									\
			if((inPointer) == NULL)														\
			{																			\
				DebugMessage(inMessage);												\
				Throw(inException);														\
			}

#define	ThrowIfKernelError(inKernelError, inException, inMessage)						\
			{																			\
				int __Err = (inKernelError);											\
				if(__Err != 0)															\
				{																		\
					DebugMessageN1(inMessage ", Error: 0x%X", __Err);					\
					Throw(inException);													\
				}																		\
			}

#define	ThrowIfError(inError, inException, inMessage)									\
			{																			\
				SInt32 __Err = (inError);												\
				if(__Err != 0)															\
				{																		\
					char __4CC[5] = CA4CCToCString(__Err);								\
					DebugMessageN2(inMessage ", Error: %d (%s)", (int)__Err, __4CC);	\
					Throw(inException);													\
				}																		\
			}

#if TARGET_OS_WIN32
#define	ThrowIfWinError(inError, inException, inMessage)								\
			{																			\
				HRESULT __Err = (inError);												\
				if(FAILED(__Err))														\
				{																		\
					DebugMessageN2(inMessage ", Code: %d, Facility: 0x%X", HRESULT_CODE(__Err), HRESULT_FACILITY(__Err));			\
					Throw(inException);													\
				}																		\
			}
#endif

#define	SubclassResponsibility(inMethodName, inException)								\
			{																			\
				DebugMessage(inMethodName": Subclasses must implement this method");	\
				Throw(inException);														\
			}

#endif	//	defined(__cplusplus)

#else

#pragma mark	Release Macros

#define	Assert(inCondition, inMessage)													\
			if(!(inCondition))															\
			{																			\
				__ASSERT_STOP;															\
			}

#define AssertFileLine(inCondition, inMessage) Assert(inCondition, inMessage)

#define	AssertNoError(inError, inMessage)												\
			{																			\
				SInt32 __Err = (inError);												\
				if(__Err != 0)															\
				{																		\
					__ASSERT_STOP;														\
				}																		\
			}

#define	AssertNoKernelError(inError, inMessage)											\
			{																			\
				unsigned int __Err = (unsigned int)(inError);							\
				if(__Err != 0)															\
				{																		\
					__ASSERT_STOP;														\
				}																		\
			}

#define	AssertNotNULL(inPtr, inMessage)													\
			{																			\
				if((inPtr) == NULL)														\
				{																		\
					__ASSERT_STOP;														\
				}																		\
			}

#define	FailIf(inCondition, inHandler, inMessage)										\
			if(inCondition)																\
			{																			\
				STOP;																	\
				goto inHandler;															\
			}

#define	FailWithAction(inCondition, inAction, inHandler, inMessage)						\
			if(inCondition)																\
			{																			\
				STOP;																	\
				{ inAction; }															\
				goto inHandler;															\
			}

#define	FailIfNULL(inPointer, inAction, inHandler, inMessage)							\
			if((inPointer) == NULL)														\
			{																			\
				STOP;																	\
				{ inAction; }															\
				goto inHandler;															\
			}

#define	FailIfKernelError(inKernelError, inAction, inHandler, inMessage)				\
			if((inKernelError) != 0)													\
			{																			\
				STOP;																	\
				{ inAction; }															\
				goto inHandler;															\
			}

#define	FailIfError(inError, inAction, inHandler, inMessage)							\
			if((inError) != 0)															\
			{																			\
				STOP;																	\
				{ inAction; }															\
				goto inHandler;															\
			}

#define	FailIfNoMessage(inCondition, inHandler, inMessage)								\
			if(inCondition)																\
			{																			\
				STOP;																	\
				goto inHandler;															\
			}

#define	FailWithActionNoMessage(inCondition, inAction, inHandler, inMessage)			\
			if(inCondition)																\
			{																			\
				STOP;																	\
				{ inAction; }															\
				goto inHandler;															\
			}

#define	FailIfNULLNoMessage(inPointer, inAction, inHandler, inMessage)					\
			if((inPointer) == NULL)														\
			{																			\
				STOP;																	\
				{ inAction; }															\
				goto inHandler;															\
			}

#define	FailIfKernelErrorNoMessage(inKernelError, inAction, inHandler, inMessage)		\
			{																			\
				unsigned int __Err = (inKernelError);									\
				if(__Err != 0)															\
				{																		\
					STOP;																\
					{ inAction; }														\
					goto inHandler;														\
				}																		\
			}

#define	FailIfErrorNoMessage(inError, inAction, inHandler, inMessage)					\
			{																			\
				SInt32 __Err = (inError);												\
				if(__Err != 0)															\
				{																		\
					STOP;																\
					{ inAction; }														\
					goto inHandler;														\
				}																		\
			}

#if defined(__cplusplus)

#define Throw(inException)  __THROW_STOP; throw (inException)

#define	ThrowIf(inCondition, inException, inMessage)									\
			if(inCondition)																\
			{																			\
				Throw(inException);														\
			}

#define	ThrowIfNULL(inPointer, inException, inMessage)									\
			if((inPointer) == NULL)														\
			{																			\
				Throw(inException);														\
			}

#define	ThrowIfKernelError(inKernelError, inException, inMessage)						\
			{																			\
				unsigned int __Err = (inKernelError);									\
				if(__Err != 0)															\
				{																		\
					Throw(inException);													\
				}																		\
			}

#define	ThrowIfError(inError, inException, inMessage)									\
			{																			\
				SInt32 __Err = (inError);												\
				if(__Err != 0)															\
				{																		\
					Throw(inException);													\
				}																		\
			}

#if TARGET_OS_WIN32
#define	ThrowIfWinError(inError, inException, inMessage)								\
			{																			\
				HRESULT __Err = (inError);												\
				if(FAILED(__Err))														\
				{																		\
					Throw(inException);													\
				}																		\
			}
#endif

#define	SubclassResponsibility(inMethodName, inException)								\
			{																			\
				Throw(inException);														\
			}

#endif	//	defined(__cplusplus)

#endif  //  DEBUG || CoreAudio_Debug

#endif

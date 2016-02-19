/*
     File: CADebugPrintf.h
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
#if !defined(__CADebugPrintf_h__)
#define __CADebugPrintf_h__

//=============================================================================
//	Includes
//=============================================================================

#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreAudio/CoreAudioTypes.h>
#else
	#include "CoreAudioTypes.h"
#endif

//=============================================================================
//	Macros to redirect debugging output to various logging services
//=============================================================================

//#define	CoreAudio_UseSysLog		1
//#define	CoreAudio_UseSideFile	"/CoreAudio-%d.txt"

#if	DEBUG || CoreAudio_Debug
	
	#if	TARGET_OS_WIN32
		#if defined(__cplusplus)
		extern "C"
		#endif
		extern int CAWin32DebugPrintf(char* inFormat, ...);
		#define	DebugPrintfRtn			CAWin32DebugPrintf
		#define	DebugPrintfFile			
		#define	DebugPrintfLineEnding	"\n"
		#define	DebugPrintfFileComma
	#else
		#if	CoreAudio_UseSysLog
			#include <sys/syslog.h>
			#define	DebugPrintfRtn	syslog
			#define	DebugPrintfFile	LOG_NOTICE
			#define	DebugPrintfLineEnding	""
			#define	DebugPrintfFileComma	DebugPrintfFile,
		#elif defined(CoreAudio_UseSideFile)
			#include <stdio.h>
			#if defined(__cplusplus)
			extern "C"
			#endif
			void OpenDebugPrintfSideFile();
			extern FILE* sDebugPrintfSideFile;
			#define	DebugPrintfRtn	fprintf
			#define	DebugPrintfFile	((sDebugPrintfSideFile != NULL) ? sDebugPrintfSideFile : stderr)
			#define	DebugPrintfLineEnding	"\n"
			#define	DebugPrintfFileComma	DebugPrintfFile,
		#else
			#include <stdio.h>
			#define	DebugPrintfRtn	fprintf
			#define	DebugPrintfFile	stderr
			#define	DebugPrintfLineEnding	"\n"
			#define	DebugPrintfFileComma	DebugPrintfFile,
		#endif
	#endif

	#define	DebugPrintf(inFormat, ...)	DebugPrintfRtn(DebugPrintfFileComma inFormat DebugPrintfLineEnding, ## __VA_ARGS__)
#else
	#define	DebugPrintfRtn	
	#define	DebugPrintfFile	
	#define	DebugPrintfLineEnding	
	#define	DebugPrintfFileComma
	#define	DebugPrintf(inFormat, ...)
#endif


#endif

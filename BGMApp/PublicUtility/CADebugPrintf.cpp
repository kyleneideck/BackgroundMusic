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
//  CADebugPrintf.cpp
//  PublicUtility
//
//  Copyright (C) 2014 Apple Inc. All Rights Reserved.
//  Copyright © 2020 Kyle Neideck
//
//  Original license header follows.
//

/*
     File: CADebugPrintf.cpp
 Abstract: CADebugPrintf.h
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
//==================================================================================================
//	Includes
//==================================================================================================

//	Self Include
#include "CADebugPrintf.h"

#if	TARGET_OS_WIN32
	#include <stdarg.h>
	#include <stdio.h>
	#include <Windows.h>
	extern "C"
	int	CAWin32DebugPrintf(char* inFormat, ...)
	{
		if (BGMDebugLoggingIsEnabled()) {
			char theMessage[1024];
			va_list theArguments;
			va_start(theArguments, inFormat);
			_vsnprintf(theMessage, 1024, inFormat, theArguments);
			va_end(theArguments);
			OutputDebugString(theMessage);
		}

		return 0;
	}
#endif

#if defined(CoreAudio_UseSideFile)
	#include <unistd.h>
	FILE* sDebugPrintfSideFile = NULL;
	extern "C"
	void OpenDebugPrintfSideFile()
	{
		if(sDebugPrintfSideFile == NULL)
		{
			char theFileName[1024];
			snprintf(theFileName, sizeof(theFileName), CoreAudio_UseSideFile, getpid());
			sDebugPrintfSideFile = fopen(theFileName, "a+");
			DebugPrintfRtn(DebugPrintfFileComma "\n------------------------------\n");
		}
	}
#endif


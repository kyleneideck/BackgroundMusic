/*
     File: CACFNumber.cpp
 Abstract: CACFNumber.h
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

#include "CACFNumber.h"

//=============================================================================
//	CACFNumber
//=============================================================================

Float32	CACFNumber::GetFixed32() const
{
	SInt32 theFixedValue = GetSInt32();
	
	//	this is a 16.16 value so convert it to a float
	Float32 theSign = theFixedValue < 0 ? -1.0f : 1.0f;
	theFixedValue *= (SInt32)theSign;
	Float32 theWholePart = (theFixedValue & 0x7FFF0000) >> 16;
	Float32 theFractPart = theFixedValue & 0x0000FFFF;
	theFractPart /= 65536.0f;
	
	return theSign * (theWholePart + theFractPart);
}

Float64	CACFNumber::GetFixed64() const
{
	SInt64 theFixedValue = GetSInt64();
	
	//	this is a 32.32 value so convert it to a double
	Float64 theSign = theFixedValue < 0 ? -1.0 : 1.0;
	theFixedValue *= (SInt64)theSign;
	Float64 theWholePart = (theFixedValue & 0x7FFFFFFF00000000LL) >> 32;
	Float64 theFractPart = theFixedValue & 0x00000000FFFFFFFFLL;
	theFractPart /= 4294967296.0;
	
	return theSign * (theWholePart + theFractPart);
}

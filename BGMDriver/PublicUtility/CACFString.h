/*
     File: CACFString.h 
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
#if !defined(__CACFString_h__)
#define __CACFString_h__

//=============================================================================
//	Includes
//=============================================================================

#include "CADebugMacros.h"

#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreAudio/CoreAudioTypes.h>
	#include <CoreFoundation/CFString.h>
#else
	#include <CoreAudioTypes.h>
	#include <CFString.h>
#endif

//=============================================================================
//	CACFString
//
//	Notes
//	-	Using the AssignWithoutRetain() method will fool the static analyzer into thinking that the
//		CFString being assigned will be leaked. This is because the static analyzer is not smart
//		enough to understand that the destructor will release the object.
//=============================================================================

class	CACFString
{
//	Construction/Destruction
public:
						CACFString() : mCFString(NULL), mWillRelease(true) {}
						CACFString(CFStringRef inCFString, bool inWillRelease = true) : mCFString(inCFString), mWillRelease(inWillRelease) {}
						CACFString(const char* inCString, bool inWillRelease = true) : mCFString(CFStringCreateWithCString(NULL, inCString, kCFStringEncodingASCII)), mWillRelease(inWillRelease) {}
						CACFString(const char* inCString, CFStringEncoding inCStringEncoding, bool inWillRelease = true) : mCFString(CFStringCreateWithCString(NULL, inCString, inCStringEncoding)), mWillRelease(inWillRelease) {}
						~CACFString() { Release(); }
						CACFString(const CACFString& inString) : mCFString(inString.mCFString), mWillRelease(inString.mWillRelease) { Retain(); }
	CACFString&			operator=(const CACFString& inString) { if (inString.mCFString != mCFString) { Release(); mCFString = inString.mCFString; mWillRelease = inString.mWillRelease; Retain(); } return *this; }
	CACFString&			operator=(CFStringRef inCFString) { if (inCFString != mCFString) { Release(); mCFString = inCFString; } mWillRelease = true; Retain(); return *this; }
	void				AssignWithoutRetain(CFStringRef inCFString) { if (inCFString != mCFString) { Release(); mCFString = inCFString; } mWillRelease = true; }

private:
	void				Retain() { if(mWillRelease && (mCFString != NULL)) { CFRetain(mCFString); } }
	void				Release() { if(mWillRelease && (mCFString != NULL)) { CFRelease(mCFString); } }
	
	CFStringRef			mCFString;
	bool				mWillRelease;

//	Operations
public:
	void				AllowRelease() { mWillRelease = true; }
	void				DontAllowRelease() { mWillRelease = false; }
	bool				IsValid() const { return mCFString != NULL; }
	bool				IsEqualTo(CFStringRef inString) const { bool theAnswer = false; if(mCFString != NULL) { theAnswer = CFStringCompare(inString, mCFString, 0) == kCFCompareEqualTo; } return theAnswer; }
	bool				StartsWith(CFStringRef inString) const { bool theAnswer = false; if(mCFString != NULL) { theAnswer = CFStringHasPrefix(mCFString, inString); } return theAnswer; }
	bool				EndsWith(CFStringRef inString) const { bool theAnswer = false; if(mCFString != NULL) { theAnswer = CFStringHasSuffix(mCFString, inString); } return theAnswer; }

//	Value Access
public:
	CFStringRef			GetCFString() const { return mCFString; }
	CFStringRef			CopyCFString() const { if(mCFString != NULL) { CFRetain(mCFString); } return mCFString; }
	const CFStringRef*	GetPointerToStorage() const	{ return &mCFString; }
	CFStringRef&		GetStorage() { Release(); mWillRelease = true; return mCFString; }
	UInt32				GetLength() const { UInt32 theAnswer = 0; if(mCFString != NULL) { theAnswer = ToUInt32(CFStringGetLength(mCFString)); } return theAnswer; }
	UInt32				GetByteLength(CFStringEncoding inEncoding = kCFStringEncodingUTF8) const { UInt32 theAnswer = 0; if(mCFString != NULL) { theAnswer = GetStringByteLength(mCFString, inEncoding); } return theAnswer; }
	void				GetCString(char* outString, UInt32& ioStringSize, CFStringEncoding inEncoding = kCFStringEncodingUTF8) const { GetCString(mCFString, outString, ioStringSize, inEncoding); }
	void				GetUnicodeString(UInt16* outString, UInt32& ioStringSize) const { GetUnicodeString(mCFString, outString, ioStringSize); }
	SInt32				GetAsInteger() { return GetAsInteger(mCFString); }
	Float64				GetAsFloat64() { return GetAsFloat64(mCFString); }

	static UInt32		GetStringLength(CFStringRef inCFString)  { UInt32 theAnswer = 0; if(inCFString != NULL) { theAnswer = ToUInt32(CFStringGetLength(inCFString)); } return theAnswer; }
	static UInt32		GetStringByteLength(CFStringRef inCFString, CFStringEncoding inEncoding = kCFStringEncodingUTF8);
	static void			GetCString(CFStringRef inCFString, char* outString, UInt32& ioStringSize, CFStringEncoding inEncoding = kCFStringEncodingUTF8);
	static void			GetUnicodeString(CFStringRef inCFString, UInt16* outString, UInt32& ioStringSize);
	static SInt32		GetAsInteger(CFStringRef inCFString) { SInt32 theAnswer = 0; if(inCFString != NULL){ theAnswer = CFStringGetIntValue(inCFString); } return theAnswer; }
	static Float64		GetAsFloat64(CFStringRef inCFString) { Float64 theAnswer = 0; if(inCFString != NULL){ theAnswer = CFStringGetDoubleValue(inCFString); } return theAnswer; }
	
};

inline bool	operator<(const CACFString& x, const CACFString& y) { return CFStringCompare(x.GetCFString(), y.GetCFString(), 0) == kCFCompareLessThan; }
inline bool	operator==(const CACFString& x, const CACFString& y) { return CFStringCompare(x.GetCFString(), y.GetCFString(), 0) == kCFCompareEqualTo; }
inline bool	operator!=(const CACFString& x, const CACFString& y) { return !(x == y); }
inline bool	operator<=(const CACFString& x, const CACFString& y) { return (x < y) || (x == y); }
inline bool	operator>=(const CACFString& x, const CACFString& y) { return !(x < y); }
inline bool	operator>(const CACFString& x, const CACFString& y) { return !((x < y) || (x == y)); }

//=============================================================================
//	CACFMutableString
//=============================================================================

class	CACFMutableString
{
//	Construction/Destruction
public:
						CACFMutableString() : mCFMutableString(NULL), mWillRelease(true) {}
						CACFMutableString(CFMutableStringRef inCFMutableString, bool inWillRelease = true) : mCFMutableString(inCFMutableString), mWillRelease(inWillRelease) {}
						CACFMutableString(CFStringRef inStringToCopy, bool /*inMakeCopy*/, bool inWillRelease = true) : mCFMutableString(CFStringCreateMutableCopy(NULL, 0, inStringToCopy)), mWillRelease(inWillRelease) {}
						CACFMutableString(const char* inCString, bool inWillRelease = true) : mCFMutableString(NULL), mWillRelease(inWillRelease) { CACFString theString(inCString); mCFMutableString = CFStringCreateMutableCopy(NULL, 0, theString.GetCFString()); }
						CACFMutableString(const char* inCString, CFStringEncoding inCStringEncoding, bool inWillRelease = true) : mCFMutableString(NULL), mWillRelease(inWillRelease) { CACFString theString(inCString, inCStringEncoding); mCFMutableString = CFStringCreateMutableCopy(NULL, 0, theString.GetCFString()); }
						~CACFMutableString() { Release(); }
						CACFMutableString(const CACFMutableString& inString) : mCFMutableString(inString.mCFMutableString), mWillRelease(inString.mWillRelease) { Retain(); }
	CACFMutableString&	operator=(const CACFMutableString& inString) { Release(); mCFMutableString = inString.mCFMutableString; mWillRelease = inString.mWillRelease; Retain(); return *this; }
	CACFMutableString&	operator=(CFMutableStringRef inCFMutableString) { Release(); mCFMutableString = inCFMutableString; mWillRelease = true; return *this; }

private:
	void				Retain() { if(mWillRelease && (mCFMutableString != NULL)) { CFRetain(mCFMutableString); } }
	void				Release() { if(mWillRelease && (mCFMutableString != NULL)) { CFRelease(mCFMutableString); } }
	
	CFMutableStringRef	mCFMutableString;
	bool				mWillRelease;

//	Operations
public:
	void				AllowRelease() { mWillRelease = true; }
	void				DontAllowRelease() { mWillRelease = false; }
	bool				IsValid() { return mCFMutableString != NULL; }
	bool				IsEqualTo(CFStringRef inString) const { bool theAnswer = false; if(mCFMutableString != NULL) { theAnswer = CFStringCompare(inString, mCFMutableString, 0) == kCFCompareEqualTo; } return theAnswer; }
	bool				StartsWith(CFStringRef inString) const { bool theAnswer = false; if(mCFMutableString != NULL) { theAnswer = CFStringHasPrefix(mCFMutableString, inString); } return theAnswer; }
	bool				EndsWith(CFStringRef inString) const { bool theAnswer = false; if(mCFMutableString != NULL) { theAnswer = CFStringHasSuffix(mCFMutableString, inString); } return theAnswer; }
	void				Append(CFStringRef inString) { if(mCFMutableString != NULL) { CFStringAppend(mCFMutableString, inString); } }

//	Value Access
public:
	CFMutableStringRef	GetCFMutableString() const { return mCFMutableString; }
	CFMutableStringRef	CopyCFMutableString() const { if(mCFMutableString != NULL) { CFRetain(mCFMutableString); } return mCFMutableString; }
	UInt32				GetLength() const { UInt32 theAnswer = 0; if(mCFMutableString != NULL) { theAnswer = ToUInt32(CFStringGetLength(mCFMutableString)); } return theAnswer; }
	UInt32				GetByteLength(CFStringEncoding inEncoding = kCFStringEncodingUTF8) const { UInt32 theAnswer = 0; if(mCFMutableString != NULL) { theAnswer = CACFString::GetStringByteLength(mCFMutableString, inEncoding); } return theAnswer; }
	void				GetCString(char* outString, UInt32& ioStringSize, CFStringEncoding inEncoding = kCFStringEncodingUTF8) const { CACFString::GetCString(mCFMutableString, outString, ioStringSize, inEncoding); }
	void				GetUnicodeString(UInt16* outString, UInt32& ioStringSize) const { CACFString::GetUnicodeString(mCFMutableString, outString, ioStringSize); }
	SInt32				GetAsInteger() { return CACFString::GetAsInteger(mCFMutableString); }
	Float64				GetAsFloat64() { return CACFString::GetAsFloat64(mCFMutableString); }

};

#endif

/*
     File: CACFArray.h
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
#if !defined(__CACFArray_h__)
#define __CACFArray_h__

//=============================================================================
//	Includes
//=============================================================================

//	System Includes
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreAudio/CoreAudioTypes.h>
	#include <CoreFoundation/CoreFoundation.h>
#else
	#include <CoreAudioTypes.h>
	#include <CoreFoundation.h>
#endif

#include "CADebugMacros.h"

//=============================================================================
//	Types
//=============================================================================

class	CACFDictionary;
class	CACFString;

//=============================================================================
//	CACFArray
//=============================================================================

class CACFArray
{

//	Construction/Destruction
public:
						CACFArray()																	: mCFArray(CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks)), mRelease(true), mMutable(true) {}
	explicit			CACFArray(bool inRelease)													: mCFArray(CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks)), mRelease(inRelease), mMutable(true) {}
						CACFArray(UInt32 inMaxNumberItems, bool inRelease)							: mCFArray(CFArrayCreateMutable(NULL, static_cast<CFIndex>(inMaxNumberItems), &kCFTypeArrayCallBacks)), mRelease(inRelease), mMutable(true) {}
						CACFArray(CFArrayRef inCFArray, bool inRelease)								: mCFArray(const_cast<CFMutableArrayRef>(inCFArray)), mRelease(inRelease), mMutable(false) {}
						CACFArray(CFMutableArrayRef inCFArray, bool inRelease)						: mCFArray(inCFArray), mRelease(inRelease), mMutable(true) {}
						CACFArray(const CACFArray& inArray)											: mCFArray(inArray.mCFArray), mRelease(inArray.mRelease), mMutable(inArray.mMutable) { Retain(); }
	CACFArray&			operator=(const CACFArray& inArray)											{ Release(); mCFArray = inArray.mCFArray; mRelease = inArray.mRelease; mMutable = inArray.mMutable; Retain(); return *this; }
	CACFArray&			operator=(CFArrayRef inCFArray)												{ Release(); mCFArray = const_cast<CFMutableArrayRef>(inCFArray); mMutable = false; Retain(); return *this; }
	CACFArray&			operator=(CFMutableArrayRef inCFArray)										{ Release(); mCFArray = inCFArray; mMutable = true; Retain(); return *this; }
						~CACFArray()																{ Release(); }

private:
	void				Retain()																	{ if(mRelease && (mCFArray != NULL)) { CFRetain(mCFArray); } }
	void				Release()																	{ if(mRelease && (mCFArray != NULL)) { CFRelease(mCFArray); } }
		
//	Attributes
public:
	bool				IsValid() const																{ return mCFArray != NULL; }
	bool				IsMutable() const															{ return mMutable; }
	bool				CanModify() const															{ return mMutable && (mCFArray != NULL); }
	
	bool				WillRelease() const															{ return mRelease; }
	void				ShouldRelease(bool inRelease)												{ mRelease = inRelease; }
	
	CFTypeID			GetTypeID() const															{ return CFGetTypeID(mCFArray); }
	
	CFArrayRef			GetCFArray() const															{ return mCFArray; }
	CFArrayRef			CopyCFArray() const															{ if(mCFArray != NULL) { CFRetain(mCFArray); } return mCFArray; }
	
	CFMutableArrayRef	GetCFMutableArray() const													{ return mCFArray; }
	CFMutableArrayRef	CopyCFMutableArray() const													{ if(mCFArray != NULL) { CFRetain(mCFArray); } return mCFArray; }
	CFPropertyListRef   AsPropertyList() const														{ return mCFArray; }

	void				SetCFMutableArrayFromCopy(CFArrayRef inArray, bool inRelease = true)		{ Release(); mCFArray = CFArrayCreateMutableCopy(NULL, 0, inArray); mMutable = true; mRelease = inRelease; }

//	Item Operations
public:
	UInt32				GetNumberItems() const														{ UInt32 theAnswer = 0; if(mCFArray != NULL) { theAnswer = ToUInt32(CFArrayGetCount(mCFArray)); } return theAnswer; }
	bool				HasItem(const void* inItem) const;
	void				RemoveItem(const void* inItem)												{ UInt32 theIndex; if(CanModify() && GetIndexOfItem(inItem, theIndex)) { RemoveItemAtIndex(theIndex); } }
	bool				GetIndexOfItem(const void* inItem, UInt32& outIndex) const;
	void				RemoveItemAtIndex(UInt32 inIndex)											{ if(CanModify()) { CFArrayRemoveValueAtIndex(mCFArray, static_cast<CFIndex>(inIndex)); } }
	void				Clear()																		{ if(CanModify()) { CFArrayRemoveAllValues(mCFArray); } }
	void				Sort(CFComparatorFunction inCompareFunction)								{ if(CanModify()) { CFRange theRange = { 0, CFArrayGetCount(mCFArray) }; CFArraySortValues(mCFArray, theRange, inCompareFunction, NULL); } }
	void				SortNumbers()																{ Sort((CFComparatorFunction)CFNumberCompare); }
	void				SortStrings()																{ Sort((CFComparatorFunction)CFStringCompare); }
	
	bool				GetBool(UInt32 inIndex, bool& outValue) const;
	bool				GetSInt32(UInt32 inIndex, SInt32& outItem) const;
	bool				GetUInt32(UInt32 inIndex, UInt32& outItem) const;
	bool				GetSInt64(UInt32 inIndex, SInt64& outItem) const;
	bool				GetUInt64(UInt32 inIndex, UInt64& outItem) const;
	bool				GetFloat32(UInt32 inIndex, Float32& outItem) const;
	bool				GetFloat64(UInt32 inIndex, Float64& outItem) const;
	bool				Get4CC(UInt32 inIndex, UInt32& outValue) const;
	bool				GetString(UInt32 inIndex, CFStringRef& outItem) const;
	bool				GetArray(UInt32 inIndex, CFArrayRef& outItem) const;
	bool				GetDictionary(UInt32 inIndex, CFDictionaryRef& outItem) const;
	bool				GetData(UInt32 inIndex, CFDataRef& outItem) const;
	bool				GetUUID(UInt32 inIndex, CFUUIDRef& outItem) const;
	bool				GetCFType(UInt32 inIndex, CFTypeRef& outItem) const;
	
	void				GetCACFString(UInt32 inIndex, CACFString& outItem) const;
	void				GetCACFArray(UInt32 inIndex, CACFArray& outItem) const;
	void				GetCACFDictionary(UInt32 inIndex, CACFDictionary& outItem) const;
	
	bool				AppendBool(bool inItem);
	bool				AppendSInt32(SInt32 inItem);
	bool				AppendUInt32(UInt32 inItem);
	bool				AppendSInt64(SInt64 inItem);
	bool				AppendUInt64(UInt64 inItem);
	bool				AppendFloat32(Float32 inItem);
	bool				AppendFloat64(Float64 inItem);
	bool				AppendString(const CFStringRef inItem);
	bool				AppendArray(const CFArrayRef inItem);
	bool				AppendDictionary(const CFDictionaryRef inItem);
	bool				AppendData(const CFDataRef inItem);
	bool				AppendCFType(const CFTypeRef inItem);
	
	bool				InsertBool(UInt32 inIndex, bool inItem);
	bool				InsertSInt32(UInt32 inIndex, SInt32 inItem);
	bool				InsertUInt32(UInt32 inIndex, UInt32 inItem);
	bool				InsertSInt64(UInt32 inIndex, SInt64 inItem);
	bool				InsertUInt64(UInt32 inIndex, UInt64 inItem);
	bool				InsertFloat32(UInt32 inIndex, Float32 inItem);
	bool				InsertFloat64(UInt32 inIndex, Float64 inItem);
	bool				InsertString(UInt32 inIndex, const CFStringRef inItem);
	bool				InsertArray(UInt32 inIndex, const CFArrayRef inItem);
	bool				InsertDictionary(UInt32 inIndex, const CFDictionaryRef inItem);
	bool				InsertData(UInt32 inIndex, const CFDataRef inItem);
	bool				InsertCFType(UInt32 inIndex, const CFTypeRef inItem);
	
	bool				SetBool(UInt32 inIndex, bool inItem);
	bool				SetSInt32(UInt32 inIndex, SInt32 inItem);
	bool				SetUInt32(UInt32 inIndex, UInt32 inItem);
	bool				SetSInt64(UInt32 inIndex, SInt64 inItem);
	bool				SetUInt64(UInt32 inIndex, UInt64 inItem);
	bool				SetFloat32(UInt32 inIndex, Float32 inItem);
	bool				SetFloat64(UInt32 inIndex, Float64 inItem);
	bool				SetString(UInt32 inIndex, const CFStringRef inItem);
	bool				SetArray(UInt32 inIndex, const CFArrayRef inItem);
	bool				SetDictionary(UInt32 inIndex, const CFDictionaryRef inItem);
	bool				SetData(UInt32 inIndex, const CFDataRef inItem);
	bool				SetCFType(UInt32 inIndex, const CFTypeRef inItem);

//	Implementation
private:
	CFMutableArrayRef	mCFArray;
	bool				mRelease;
	bool				mMutable;
	
						CACFArray(const void*);	// prevent accidental instantiation with a pointer via bool constructor
};

#endif

/*
     File: CAPropertyAddress.h
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
#if !defined(__CAPropertyAddress_h__)
#define __CAPropertyAddress_h__

//==================================================================================================
//	Includes
//==================================================================================================

//	PublicUtility Includes
#include "CADebugMacros.h"

//	System Includes
#include <CoreAudio/AudioHardware.h>

//  Standard Library Includes
#include <algorithm>
#include <functional>
#include <vector>

//==================================================================================================
//	CAPropertyAddress
//
//  CAPropertyAddress extends the AudioObjectPropertyAddress structure to C++ including constructors
//  and other utility operations. Note that there is no defined operator< or operator== because the
//  presence of wildcards for the fields make comparisons ambiguous without specifying whether or
//  not to take the wildcards into account. Consequently, if you want to use this struct in an STL
//  data structure, you'll need to specify the approriate function object explicitly in the template
//  declaration.
//==================================================================================================

struct CAPropertyAddress
:
	public AudioObjectPropertyAddress
{

//	Construction/Destruction
public:
						CAPropertyAddress()																													: AudioObjectPropertyAddress() { mSelector = 0; mScope = kAudioObjectPropertyScopeGlobal; mElement = kAudioObjectPropertyElementMaster; }
						CAPropertyAddress(AudioObjectPropertySelector inSelector)																			: AudioObjectPropertyAddress() { mSelector = inSelector; mScope = kAudioObjectPropertyScopeGlobal; mElement = kAudioObjectPropertyElementMaster; }
						CAPropertyAddress(AudioObjectPropertySelector inSelector, AudioObjectPropertyScope inScope)											: AudioObjectPropertyAddress() { mSelector = inSelector; mScope = inScope; mElement = kAudioObjectPropertyElementMaster; }
						CAPropertyAddress(AudioObjectPropertySelector inSelector, AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement)   : AudioObjectPropertyAddress() { mSelector = inSelector; mScope = inScope; mElement = inElement; }
						CAPropertyAddress(const AudioObjectPropertyAddress& inAddress)																		: AudioObjectPropertyAddress(inAddress){}
						CAPropertyAddress(const CAPropertyAddress& inAddress)																				: AudioObjectPropertyAddress(inAddress){}
	CAPropertyAddress&  operator=(const AudioObjectPropertyAddress& inAddress)																				{ AudioObjectPropertyAddress::operator=(inAddress); return *this; }
	CAPropertyAddress&  operator=(const CAPropertyAddress& inAddress)																						{ AudioObjectPropertyAddress::operator=(inAddress); return *this; }
	
//  Operations
public:
	static bool			IsSameAddress(const AudioObjectPropertyAddress& inAddress1, const AudioObjectPropertyAddress& inAddress2)							{ return (inAddress1.mScope == inAddress2.mScope) && (inAddress1.mSelector == inAddress2.mSelector) && (inAddress1.mElement == inAddress2.mElement); }
	static bool			IsLessThanAddress(const AudioObjectPropertyAddress& inAddress1, const AudioObjectPropertyAddress& inAddress2)						{ bool theAnswer = false; if(inAddress1.mScope != inAddress2.mScope) { theAnswer = inAddress1.mScope < inAddress2.mScope; } else if(inAddress1.mSelector != inAddress2.mSelector) { theAnswer = inAddress1.mSelector < inAddress2.mSelector; } else { theAnswer = inAddress1.mElement < inAddress2.mElement; } return theAnswer; }
	static bool			IsCongruentSelector(AudioObjectPropertySelector inSelector1, AudioObjectPropertySelector inSelector2)								{ return (inSelector1 == inSelector2) || (inSelector1 == kAudioObjectPropertySelectorWildcard) || (inSelector2 == kAudioObjectPropertySelectorWildcard); }
	static bool			IsCongruentScope(AudioObjectPropertyScope inScope1, AudioObjectPropertyScope inScope2)												{ return (inScope1 == inScope2) || (inScope1 == kAudioObjectPropertyScopeWildcard) || (inScope2 == kAudioObjectPropertyScopeWildcard); }
	static bool			IsCongruentElement(AudioObjectPropertyElement inElement1, AudioObjectPropertyElement inElement2)									{ return (inElement1 == inElement2) || (inElement1 == kAudioObjectPropertyElementWildcard) || (inElement2 == kAudioObjectPropertyElementWildcard); }
	static bool			IsCongruentAddress(const AudioObjectPropertyAddress& inAddress1, const AudioObjectPropertyAddress& inAddress2)						{ return IsCongruentScope(inAddress1.mScope, inAddress2.mScope) && IsCongruentSelector(inAddress1.mSelector, inAddress2.mSelector) && IsCongruentElement(inAddress1.mElement, inAddress2.mElement); }
	static bool			IsCongruentLessThanAddress(const AudioObjectPropertyAddress& inAddress1, const AudioObjectPropertyAddress& inAddress2)				{ bool theAnswer = false; if(!IsCongruentScope(inAddress1.mScope, inAddress2.mScope)) { theAnswer = inAddress1.mScope < inAddress2.mScope; } else if(!IsCongruentSelector(inAddress1.mSelector, inAddress2.mSelector)) { theAnswer = inAddress1.mSelector < inAddress2.mSelector; } else if(!IsCongruentElement(inAddress1.mElement, inAddress2.mElement)) { theAnswer = inAddress1.mElement < inAddress2.mElement; } return theAnswer; }

//  STL Helpers
public:
	struct EqualTo : public std::binary_function<AudioObjectPropertyAddress, AudioObjectPropertyAddress, bool>
	{
		bool	operator()(const AudioObjectPropertyAddress& inAddress1, const AudioObjectPropertyAddress& inAddress2) const								{ return IsSameAddress(inAddress1, inAddress2); }
	};

	struct LessThan : public std::binary_function<AudioObjectPropertyAddress, AudioObjectPropertyAddress, bool>
	{
		bool	operator()(const AudioObjectPropertyAddress& inAddress1, const AudioObjectPropertyAddress& inAddress2) const								{ return IsLessThanAddress(inAddress1, inAddress2); }
	};

	struct CongruentEqualTo : public std::binary_function<AudioObjectPropertyAddress, AudioObjectPropertyAddress, bool>
	{
		bool	operator()(const AudioObjectPropertyAddress& inAddress1, const AudioObjectPropertyAddress& inAddress2) const								{ return IsCongruentAddress(inAddress1, inAddress2); }
	};

	struct CongruentLessThan : public std::binary_function<AudioObjectPropertyAddress, AudioObjectPropertyAddress, bool>
	{
		bool	operator()(const AudioObjectPropertyAddress& inAddress1, const AudioObjectPropertyAddress& inAddress2) const								{ return IsCongruentLessThanAddress(inAddress1, inAddress2); }
	};

};

//==================================================================================================
//  CAPropertyAddressList
//
//  An auto-resizing array of CAPropertyAddress structures.
//==================================================================================================

class   CAPropertyAddressList
{

//	Construction/Destruction
public:
											CAPropertyAddressList()																	: mAddressList(), mToken(NULL) {}
	explicit								CAPropertyAddressList(void* inToken)													: mAddressList(), mToken(inToken) {}
	explicit								CAPropertyAddressList(uintptr_t inToken)												: mAddressList(), mToken(reinterpret_cast<void*>(inToken)) {}
											CAPropertyAddressList(const CAPropertyAddressList& inAddressList)						: mAddressList(inAddressList.mAddressList), mToken(inAddressList.mToken) {}
	CAPropertyAddressList&					operator=(const CAPropertyAddressList& inAddressList)									{ mAddressList = inAddressList.mAddressList; mToken = inAddressList.mToken; return *this; }
											~CAPropertyAddressList()																{}

//	Operations
public:
	void*									GetToken() const																		{ return mToken; }
	void									SetToken(void* inToken)																	{ mToken = inToken; }
	
	uintptr_t								GetIntToken() const																		{ return reinterpret_cast<uintptr_t>(mToken); }
	void									SetIntToken(uintptr_t inToken)															{ mToken = reinterpret_cast<void*>(inToken); }
	
	AudioObjectID							GetAudioObjectIDToken() const															{ return static_cast<AudioObjectID>(reinterpret_cast<uintptr_t>(mToken)); }
	
	bool									IsEmpty() const																			{ return mAddressList.empty(); }
	UInt32									GetNumberItems() const																	{ return ToUInt32(mAddressList.size()); }
	void									GetItemByIndex(UInt32 inIndex, AudioObjectPropertyAddress& outAddress) const			{ if(inIndex < mAddressList.size()) { outAddress = mAddressList.at(inIndex); } }
	const AudioObjectPropertyAddress*		GetItems() const																		{ return &(*mAddressList.begin()); }
	AudioObjectPropertyAddress*				GetItems()																				{ return &(*mAddressList.begin()); }
	
	bool									HasItem(const AudioObjectPropertyAddress& inAddress) const								{ AddressList::const_iterator theIterator = std::find_if(mAddressList.begin(), mAddressList.end(), std::bind1st(CAPropertyAddress::CongruentEqualTo(), inAddress)); return theIterator != mAddressList.end(); }
	bool									HasExactItem(const AudioObjectPropertyAddress& inAddress) const							{ AddressList::const_iterator theIterator = std::find_if(mAddressList.begin(), mAddressList.end(), std::bind1st(CAPropertyAddress::EqualTo(), inAddress)); return theIterator != mAddressList.end(); }

	void									AppendItem(const AudioObjectPropertyAddress& inAddress)									{ mAddressList.push_back(inAddress); }
	void									AppendUniqueItem(const AudioObjectPropertyAddress& inAddress)							{ if(!HasItem(inAddress)) { mAddressList.push_back(inAddress); } }
	void									AppendUniqueExactItem(const AudioObjectPropertyAddress& inAddress)						{ if(!HasExactItem(inAddress)) { mAddressList.push_back(inAddress); } }
	void									InsertItemAtIndex(UInt32 inIndex, const AudioObjectPropertyAddress& inAddress)			{ if(inIndex < mAddressList.size()) { AddressList::iterator theIterator = mAddressList.begin(); std::advance(theIterator, static_cast<int>(inIndex)); mAddressList.insert(theIterator, inAddress); } else { mAddressList.push_back(inAddress); } }
	void									EraseExactItem(const AudioObjectPropertyAddress& inAddress)								{ AddressList::iterator theIterator = std::find_if(mAddressList.begin(), mAddressList.end(), std::bind1st(CAPropertyAddress::EqualTo(), inAddress)); if(theIterator != mAddressList.end()) { mAddressList.erase(theIterator); } }
	void									EraseItemAtIndex(UInt32 inIndex)														{ if(inIndex < mAddressList.size()) { AddressList::iterator theIterator = mAddressList.begin(); std::advance(theIterator, static_cast<int>(inIndex)); mAddressList.erase(theIterator); } }
	void									EraseAllItems()																			{ mAddressList.clear(); }

//  Implementation
private:
	typedef std::vector<CAPropertyAddress>  AddressList;

	AddressList								mAddressList;
	void*									mToken;
	
};

//==================================================================================================
//  CAPropertyAddressListVector
//
//  An auto-resizing array of CAPropertyAddressList objects.
//==================================================================================================

class	CAPropertyAddressListVector
{

//	Construction/Destruction
public:
												CAPropertyAddressListVector()														: mAddressListVector() {}
												CAPropertyAddressListVector(const CAPropertyAddressListVector& inAddressListVector)	: mAddressListVector(inAddressListVector.mAddressListVector) {}
	CAPropertyAddressListVector&				operator=(const CAPropertyAddressListVector& inAddressListVector)					{ mAddressListVector = inAddressListVector.mAddressListVector; return *this; }
												~CAPropertyAddressListVector()														{}

//	Operations
public:
	bool										IsEmpty() const																		{ return mAddressListVector.empty(); }
	bool										HasAnyNonEmptyItems() const;
	bool										HasAnyItemsWithAddress(const AudioObjectPropertyAddress& inAddress) const;
	bool										HasAnyItemsWithExactAddress(const AudioObjectPropertyAddress& inAddress) const;

	UInt32										GetNumberItems() const																{ return ToUInt32(mAddressListVector.size()); }
	const CAPropertyAddressList&				GetItemByIndex(UInt32 inIndex) const												{ return mAddressListVector.at(inIndex); }
	CAPropertyAddressList&						GetItemByIndex(UInt32 inIndex)														{ return mAddressListVector.at(inIndex); }
	const CAPropertyAddressList*				GetItemByToken(void* inToken) const;
	CAPropertyAddressList*						GetItemByToken(void* inToken);
	const CAPropertyAddressList*				GetItemByIntToken(uintptr_t inToken) const;
	CAPropertyAddressList*						GetItemByIntToken(uintptr_t inToken);
	
	void										AppendItem(const CAPropertyAddressList& inAddressList)								{ mAddressListVector.push_back(inAddressList); }
	void										EraseAllItems()																		{ mAddressListVector.clear(); }
	
//  Implementation
private:
	typedef std::vector<CAPropertyAddressList>	AddressListVector;

	AddressListVector							mAddressListVector;

};

inline bool	CAPropertyAddressListVector::HasAnyNonEmptyItems() const
{
	bool theAnswer = false;
	for(AddressListVector::const_iterator theIterator = mAddressListVector.begin(); !theAnswer && (theIterator != mAddressListVector.end()); ++theIterator)
	{
		theAnswer = !theIterator->IsEmpty();
	}
	return theAnswer;
}

inline bool	CAPropertyAddressListVector::HasAnyItemsWithAddress(const AudioObjectPropertyAddress& inAddress) const
{
	bool theAnswer = false;
	for(AddressListVector::const_iterator theIterator = mAddressListVector.begin(); !theAnswer && (theIterator != mAddressListVector.end()); ++theIterator)
	{
		theAnswer = theIterator->HasItem(inAddress);
	}
	return theAnswer;
}

inline bool	CAPropertyAddressListVector::HasAnyItemsWithExactAddress(const AudioObjectPropertyAddress& inAddress) const
{
	bool theAnswer = false;
	for(AddressListVector::const_iterator theIterator = mAddressListVector.begin(); !theAnswer && (theIterator != mAddressListVector.end()); ++theIterator)
	{
		theAnswer = theIterator->HasExactItem(inAddress);
	}
	return theAnswer;
}

inline const CAPropertyAddressList*	CAPropertyAddressListVector::GetItemByToken(void* inToken) const
{
	const CAPropertyAddressList* theAnswer = NULL;
	bool wasFound = false;
	for(AddressListVector::const_iterator theIterator = mAddressListVector.begin(); !wasFound && (theIterator != mAddressListVector.end()); ++theIterator)
	{
		if(theIterator->GetToken() == inToken)
		{
			wasFound = true;
			theAnswer = &(*theIterator);
		}
	}
	return theAnswer;
}

inline CAPropertyAddressList*	CAPropertyAddressListVector::GetItemByToken(void* inToken)
{
	CAPropertyAddressList* theAnswer = NULL;
	bool wasFound = false;
	for(AddressListVector::iterator theIterator = mAddressListVector.begin(); !wasFound && (theIterator != mAddressListVector.end()); ++theIterator)
	{
		if(theIterator->GetToken() == inToken)
		{
			wasFound = true;
			theAnswer = &(*theIterator);
		}
	}
	return theAnswer;
}

inline const CAPropertyAddressList*	CAPropertyAddressListVector::GetItemByIntToken(uintptr_t inToken) const
{
	const CAPropertyAddressList* theAnswer = NULL;
	bool wasFound = false;
	for(AddressListVector::const_iterator theIterator = mAddressListVector.begin(); !wasFound && (theIterator != mAddressListVector.end()); ++theIterator)
	{
		if(theIterator->GetIntToken() == inToken)
		{
			wasFound = true;
			theAnswer = &(*theIterator);
		}
	}
	return theAnswer;
}

inline CAPropertyAddressList*	CAPropertyAddressListVector::GetItemByIntToken(uintptr_t inToken)
{
	CAPropertyAddressList* theAnswer = NULL;
	bool wasFound = false;
	for(AddressListVector::iterator theIterator = mAddressListVector.begin(); !wasFound && (theIterator != mAddressListVector.end()); ++theIterator)
	{
		if(theIterator->GetIntToken() == inToken)
		{
			wasFound = true;
			theAnswer = &(*theIterator);
		}
	}
	return theAnswer;
}

#endif

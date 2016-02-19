/*
     File: CAVolumeCurve.h 
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
#if !defined(__CAVolumeCurve_h__)
#define __CAVolumeCurve_h__

//=============================================================================
//	Includes
//=============================================================================

#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreAudio/CoreAudioTypes.h>
#else
	#include <CoreAudioTypes.h>
#endif
#include <map>

//=============================================================================
//	Types
//=============================================================================

struct CARawPoint
{
	SInt32	mMinimum;
	SInt32	mMaximum;
	
	CARawPoint() : mMinimum(0), mMaximum(0) {}
	CARawPoint(const CARawPoint& inPoint) : mMinimum(inPoint.mMinimum), mMaximum(inPoint.mMaximum) {}
	CARawPoint(SInt32 inMinimum, SInt32 inMaximum) : mMinimum(inMinimum), mMaximum(inMaximum) {}
	CARawPoint&	operator=(const CARawPoint& inPoint) { mMinimum = inPoint.mMinimum; mMaximum = inPoint.mMaximum; return *this; }
	
	static bool	Overlap(const CARawPoint& x, const CARawPoint& y) { return (x.mMinimum < y.mMaximum) && (x.mMaximum > y.mMinimum); }
};

inline bool	operator<(const CARawPoint& x, const CARawPoint& y) { return x.mMinimum < y.mMinimum; }
inline bool	operator==(const CARawPoint& x, const CARawPoint& y) { return (x.mMinimum == y.mMinimum) && (x.mMaximum == y.mMaximum); }
inline bool	operator!=(const CARawPoint& x, const CARawPoint& y) { return !(x == y); }
inline bool	operator<=(const CARawPoint& x, const CARawPoint& y) { return (x < y) || (x == y); }
inline bool	operator>=(const CARawPoint& x, const CARawPoint& y) { return !(x < y); }
inline bool	operator>(const CARawPoint& x, const CARawPoint& y) { return !((x < y) || (x == y)); }

struct CADBPoint
{
	Float32	mMinimum;
	Float32	mMaximum;
	
	CADBPoint() : mMinimum(0), mMaximum(0) {}
	CADBPoint(const CADBPoint& inPoint) : mMinimum(inPoint.mMinimum), mMaximum(inPoint.mMaximum) {}
	CADBPoint(Float32 inMinimum, Float32 inMaximum) : mMinimum(inMinimum), mMaximum(inMaximum) {}
	CADBPoint&	operator=(const CADBPoint& inPoint) { mMinimum = inPoint.mMinimum; mMaximum = inPoint.mMaximum; return *this; }
	
	static bool	Overlap(const CADBPoint& x, const CADBPoint& y) { return (x.mMinimum < y.mMaximum) && (x.mMaximum >= y.mMinimum); }
};

inline bool	operator<(const CADBPoint& x, const CADBPoint& y) { return x.mMinimum < y.mMinimum; }
inline bool	operator==(const CADBPoint& x, const CADBPoint& y) { return (x.mMinimum == y.mMinimum) && (x.mMaximum == y.mMaximum); }
inline bool	operator!=(const CADBPoint& x, const CADBPoint& y) { return !(x == y); }
inline bool	operator<=(const CADBPoint& x, const CADBPoint& y) { return (x < y) || (x == y); }
inline bool	operator>=(const CADBPoint& x, const CADBPoint& y) { return !(x < y); }
inline bool	operator>(const CADBPoint& x, const CADBPoint& y) { return !((x < y) || (x == y)); }

//=============================================================================
//	CAVolumeCurve
//=============================================================================

class CAVolumeCurve
{

//	Constants
public:
	enum
	{
					kLinearCurve		= 0,
					kPow1Over3Curve		= 1,
					kPow1Over2Curve		= 2,
					kPow3Over4Curve		= 3,
					kPow3Over2Curve		= 4,
					kPow2Over1Curve		= 5,
					kPow3Over1Curve		= 6,
					kPow4Over1Curve		= 7,
					kPow5Over1Curve		= 8,
					kPow6Over1Curve		= 9,
					kPow7Over1Curve		= 10,
					kPow8Over1Curve		= 11,
					kPow9Over1Curve		= 12,
					kPow10Over1Curve	= 13,
					kPow11Over1Curve	= 14,
					kPow12Over1Curve	= 15
	};

//	Construction/Destruction
public:
					CAVolumeCurve();
	virtual			~CAVolumeCurve();

//	Attributes
public:
	UInt32			GetTag() const			{ return mTag; }
	void			SetTag(UInt32 inTag)	{ mTag = inTag; }
	SInt32			GetMinimumRaw() const;
	SInt32			GetMaximumRaw() const;
	Float32			GetMinimumDB() const;
	Float32			GetMaximumDB() const;
	
	void			SetIsApplyingTransferFunction(bool inIsApplyingTransferFunction)  { mIsApplyingTransferFunction = inIsApplyingTransferFunction; }
	UInt32			GetTransferFunction() const { return mTransferFunction; }
	void			SetTransferFunction(UInt32 inTransferFunction);

//	Operations
public:
	void			AddRange(SInt32 mMinRaw, SInt32 mMaxRaw, Float32 inMinDB, Float32 inMaxDB);
	void			ResetRange();
	bool			CheckForContinuity() const;
	
	SInt32			ConvertDBToRaw(Float32 inDB) const;
	Float32			ConvertRawToDB(SInt32 inRaw) const;
	Float32			ConvertRawToScalar(SInt32 inRaw) const;
	Float32			ConvertDBToScalar(Float32 inDB) const;
	SInt32			ConvertScalarToRaw(Float32 inScalar) const;
	Float32			ConvertScalarToDB(Float32 inScalar) const;

//	Implementation
private:
	typedef	std::map<CARawPoint, CADBPoint>	CurveMap;
	
	UInt32			mTag;
	CurveMap		mCurveMap;
	bool			mIsApplyingTransferFunction;
	UInt32			mTransferFunction;
	Float32			mRawToScalarExponentNumerator;
	Float32			mRawToScalarExponentDenominator;

};

#endif

/*
     File: CAVolumeCurve.cpp 
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
//=============================================================================
//	Includes
//=============================================================================

#include "CAVolumeCurve.h"
#include "CADebugMacros.h"
#include <math.h>

//=============================================================================
//	CAVolumeCurve
//=============================================================================

CAVolumeCurve::CAVolumeCurve()
:
	mTag(0),
	mCurveMap(),
	mIsApplyingTransferFunction(true),
	mTransferFunction(kPow2Over1Curve),
	mRawToScalarExponentNumerator(2.0f),
	mRawToScalarExponentDenominator(1.0f)
{
}

CAVolumeCurve::~CAVolumeCurve()
{
}

SInt32	CAVolumeCurve::GetMinimumRaw() const
{
	SInt32 theAnswer = 0;
	
	if(!mCurveMap.empty())
	{
		CurveMap::const_iterator theIterator = mCurveMap.begin();
		theAnswer = theIterator->first.mMinimum;
	}
	
	return theAnswer;
}

SInt32	CAVolumeCurve::GetMaximumRaw() const
{
	SInt32 theAnswer = 0;
	
	if(!mCurveMap.empty())
	{
		CurveMap::const_iterator theIterator = mCurveMap.begin();
		std::advance(theIterator, static_cast<int>(mCurveMap.size() - 1));
		theAnswer = theIterator->first.mMaximum;
	}
	
	return theAnswer;
}

Float32	CAVolumeCurve::GetMinimumDB() const
{
	Float32 theAnswer = 0;
	
	if(!mCurveMap.empty())
	{
		CurveMap::const_iterator theIterator = mCurveMap.begin();
		theAnswer = theIterator->second.mMinimum;
	}
	
	return theAnswer;
}

Float32	CAVolumeCurve::GetMaximumDB() const
{
	Float32 theAnswer = 0;
	
	if(!mCurveMap.empty())
	{
		CurveMap::const_iterator theIterator = mCurveMap.begin();
		std::advance(theIterator, static_cast<int>(mCurveMap.size() - 1));
		theAnswer = theIterator->second.mMaximum;
	}
	
	return theAnswer;
}

void	CAVolumeCurve::SetTransferFunction(UInt32 inTransferFunction)
{
	mTransferFunction = inTransferFunction;
	
	//	figure out the co-efficients
	switch(inTransferFunction)
	{
		case kLinearCurve:
			mIsApplyingTransferFunction = false;
			mRawToScalarExponentNumerator = 1.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
			
		case kPow1Over3Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 1.0f;
			mRawToScalarExponentDenominator = 3.0f;
			break;
			
		case kPow1Over2Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 1.0f;
			mRawToScalarExponentDenominator = 2.0f;
			break;
			
		case kPow3Over4Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 3.0f;
			mRawToScalarExponentDenominator = 4.0f;
			break;
			
		case kPow3Over2Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 3.0f;
			mRawToScalarExponentDenominator = 2.0f;
			break;
			
		case kPow2Over1Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 2.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
			
		case kPow3Over1Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 3.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
		
		case kPow4Over1Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 4.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
		
		case kPow5Over1Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 5.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
		
		case kPow6Over1Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 6.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
		
		case kPow7Over1Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 7.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
		
		case kPow8Over1Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 8.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
		
		case kPow9Over1Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 9.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
		
		case kPow10Over1Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 10.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
		
		case kPow11Over1Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 11.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
		
		case kPow12Over1Curve:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 12.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
		
		default:
			mIsApplyingTransferFunction = true;
			mRawToScalarExponentNumerator = 2.0f;
			mRawToScalarExponentDenominator = 1.0f;
			break;
	};
}

void	CAVolumeCurve::AddRange(SInt32 inMinRaw, SInt32 inMaxRaw, Float32 inMinDB, Float32 inMaxDB)
{
	CARawPoint theRaw(inMinRaw, inMaxRaw);
	CADBPoint theDB(inMinDB, inMaxDB);
	
	bool isOverlapped = false;
	bool isDone = false;
	CurveMap::iterator theIterator = mCurveMap.begin();
	while((theIterator != mCurveMap.end()) && !isOverlapped && !isDone)
	{
		isOverlapped = CARawPoint::Overlap(theRaw, theIterator->first);
		isDone = theRaw >= theIterator->first;
		
		if(!isOverlapped && !isDone)
		{
			std::advance(theIterator, 1);
		}
	}
	
	if(!isOverlapped)
	{
		mCurveMap.insert(CurveMap::value_type(theRaw, theDB));
	}
	else
	{
		DebugMessage("CAVolumeCurve::AddRange: new point overlaps");
	}
}

void	CAVolumeCurve::ResetRange()
{
	mCurveMap.clear();
}

bool	CAVolumeCurve::CheckForContinuity() const
{
	bool theAnswer = true;
	
	CurveMap::const_iterator theIterator = mCurveMap.begin();
	if(theIterator != mCurveMap.end())
	{
		SInt32 theRaw = theIterator->first.mMinimum;
		Float32 theDB = theIterator->second.mMinimum;
		do
		{
			SInt32 theRawMin = theIterator->first.mMinimum;
			SInt32 theRawMax = theIterator->first.mMaximum;
			SInt32 theRawRange = theRawMax - theRawMin;
			
			Float32 theDBMin = theIterator->second.mMinimum;
			Float32 theDBMax = theIterator->second.mMaximum;
			Float32 theDBRange = theDBMax - theDBMin;

			theAnswer = theRaw == theRawMin;
			theAnswer = theAnswer && (theDB == theDBMin);
			
			theRaw += theRawRange;
			theDB += theDBRange;
			
			std::advance(theIterator, 1);
		}
		while((theIterator != mCurveMap.end()) && theAnswer);
	}
	
	return theAnswer;
}

SInt32	CAVolumeCurve::ConvertDBToRaw(Float32 inDB) const
{
	//	clamp the value to the dB range
	Float32 theOverallDBMin = GetMinimumDB();
	Float32 theOverallDBMax = GetMaximumDB();
	
	if(inDB < theOverallDBMin) inDB = theOverallDBMin;
	if(inDB > theOverallDBMax) inDB = theOverallDBMax;
	
	//	get the first entry in the curve map;
	CurveMap::const_iterator theIterator = mCurveMap.begin();
	
	//	initialize the answer to the minimum raw of the first item in the curve map
	SInt32 theAnswer = theIterator->first.mMinimum;
	
	//	iterate through the curve map until we run out of dB
	bool isDone = false;
	while(!isDone && (theIterator != mCurveMap.end()))
	{
		SInt32 theRawMin = theIterator->first.mMinimum;
		SInt32 theRawMax = theIterator->first.mMaximum;
		SInt32 theRawRange = theRawMax - theRawMin;
		
		Float32 theDBMin = theIterator->second.mMinimum;
		Float32 theDBMax = theIterator->second.mMaximum;
		Float32 theDBRange = theDBMax - theDBMin;
		
		Float32 theDBPerRaw = theDBRange / static_cast<Float32>(theRawRange);
		
		//	figure out how many steps we are into this entry in the curve map
		if(inDB > theDBMax)
		{
			//	we're past the end of this one, so add in the whole range for this entry
			theAnswer += theRawRange;
		}
		else
		{
			//	it's somewhere within the current entry
			//	figure out how many steps it is
			Float32 theNumberRawSteps = inDB - theDBMin;
			theNumberRawSteps /= theDBPerRaw;
			
			//	only move in whole steps
			theNumberRawSteps = roundf(theNumberRawSteps);
			
			//	add this many steps to the answer
			theAnswer += static_cast<SInt32>(theNumberRawSteps);
			
			//	mark that we are done
			isDone = true;
		}
		
		//	go to the next entry in the curve map
		std::advance(theIterator, 1);
	}
	
	return theAnswer;
}

Float32	CAVolumeCurve::ConvertRawToDB(SInt32 inRaw) const
{
	Float32 theAnswer = 0;
	
	//	clamp the raw value
	SInt32 theOverallRawMin = GetMinimumRaw();
	SInt32 theOverallRawMax = GetMaximumRaw();
	
	if(inRaw < theOverallRawMin) inRaw = theOverallRawMin;
	if(inRaw > theOverallRawMax) inRaw = theOverallRawMax;
	
	//	figure out how many raw steps need to be taken from the first one
	SInt32 theNumberRawSteps = inRaw - theOverallRawMin;

	//	get the first item in the curve map
	CurveMap::const_iterator theIterator = mCurveMap.begin();
	
	//	initialize the answer to the minimum dB of the first item in the curve map
	theAnswer = theIterator->second.mMinimum;
	
	//	iterate through the curve map until we run out of steps
	while((theNumberRawSteps > 0) && (theIterator != mCurveMap.end()))
	{
		//	compute some values
		SInt32 theRawMin = theIterator->first.mMinimum;
		SInt32 theRawMax = theIterator->first.mMaximum;
		SInt32 theRawRange = theRawMax - theRawMin;
		
		Float32 theDBMin = theIterator->second.mMinimum;
		Float32 theDBMax = theIterator->second.mMaximum;
		Float32 theDBRange = theDBMax - theDBMin;
		
		Float32 theDBPerRaw = theDBRange / static_cast<Float32>(theRawRange);
		
		//	there might be more steps than the current map entry accounts for
		SInt32 theRawStepsToAdd = std::min(theRawRange, theNumberRawSteps);
		
		//	add this many steps worth of db to the answer;
		theAnswer += theRawStepsToAdd * theDBPerRaw;
		
		//	figure out how many steps are left
		theNumberRawSteps -= theRawStepsToAdd;
		
		//	go to the next map entry
		std::advance(theIterator, 1);
	}
	
	return theAnswer;
}

Float32	CAVolumeCurve::ConvertRawToScalar(SInt32 inRaw) const
{
	//	get some important values
	Float32	theDBMin = GetMinimumDB();
	Float32	theDBMax = GetMaximumDB();
	Float32	theDBRange = theDBMax - theDBMin;
	SInt32	theRawMin = GetMinimumRaw();
	SInt32	theRawMax = GetMaximumRaw();
	SInt32	theRawRange = theRawMax - theRawMin;
	
	//	range the raw value
	if(inRaw < theRawMin) inRaw = theRawMin;
	if(inRaw > theRawMax) inRaw = theRawMax;

	//	calculate the distance in the range inRaw is
	Float32 theAnswer = static_cast<Float32>(inRaw - theRawMin) / static_cast<Float32>(theRawRange);

	//	only apply a curve to the scalar values if the dB range is greater than 30
	if(mIsApplyingTransferFunction && (theDBRange > 30.0f))
	{
		theAnswer = powf(theAnswer, mRawToScalarExponentNumerator / mRawToScalarExponentDenominator);
	}

	return theAnswer;
}

Float32	CAVolumeCurve::ConvertDBToScalar(Float32 inDB) const
{
	SInt32 theRawValue = ConvertDBToRaw(inDB);
	Float32 theAnswer = ConvertRawToScalar(theRawValue);
	return theAnswer;
}

SInt32	CAVolumeCurve::ConvertScalarToRaw(Float32 inScalar) const
{
	//	range the scalar value
	inScalar = std::min(1.0f, std::max(0.0f, inScalar));
	
	//	get some important values
	Float32	theDBMin = GetMinimumDB();
	Float32	theDBMax = GetMaximumDB();
	Float32	theDBRange = theDBMax - theDBMin;
	SInt32	theRawMin = GetMinimumRaw();
	SInt32	theRawMax = GetMaximumRaw();
	SInt32	theRawRange = theRawMax - theRawMin;
	
	//	have to undo the curve if the dB range is greater than 30
	if(mIsApplyingTransferFunction && (theDBRange > 30.0f))
	{
		inScalar = powf(inScalar, mRawToScalarExponentDenominator / mRawToScalarExponentNumerator);
	}
	
	//	now we can figure out how many raw steps this is
	Float32 theNumberRawSteps = inScalar * static_cast<Float32>(theRawRange);
	theNumberRawSteps = roundf(theNumberRawSteps);
	
	//	the answer is the minimum raw value plus the number of raw steps
	SInt32 theAnswer = theRawMin + static_cast<SInt32>(theNumberRawSteps);
	
	return theAnswer;
}

Float32	CAVolumeCurve::ConvertScalarToDB(Float32 inScalar) const
{
	SInt32 theRawValue = ConvertScalarToRaw(inScalar);
	Float32 theAnswer = ConvertRawToDB(theRawValue);
	return theAnswer;
}

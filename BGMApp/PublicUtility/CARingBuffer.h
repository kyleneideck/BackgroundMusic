/*
     File: CARingBuffer.h
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
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreAudio/CoreAudioTypes.h>
#else
	#include <CoreAudioTypes.h>
#endif


#ifndef CARingBuffer_Header
#define CARingBuffer_Header

enum {
	kCARingBufferError_OK = 0,
	kCARingBufferError_TooMuch = 3, // fetch start time is earlier than buffer start time and fetch end time is later than buffer end time
	kCARingBufferError_CPUOverload = 4 // the reader is unable to get enough CPU cycles to capture a consistent snapshot of the time bounds
};

typedef SInt32 CARingBufferError;

const UInt32 kGeneralRingTimeBoundsQueueSize = 32;
const UInt32 kGeneralRingTimeBoundsQueueMask = kGeneralRingTimeBoundsQueueSize - 1;

class CARingBuffer {
public:
	typedef SInt64 SampleTime;

	CARingBuffer();
	~CARingBuffer();
	
	void					Allocate(int nChannels, UInt32 bytesPerFrame, UInt32 capacityFrames);
								// capacityFrames will be rounded up to a power of 2
	void					Deallocate();
	
	CARingBufferError	Store(const AudioBufferList *abl, UInt32 nFrames, SampleTime frameNumber);
							// Copy nFrames of data into the ring buffer at the specified sample time.
							// The sample time should normally increase sequentially, though gaps
							// are filled with zeroes. A sufficiently large gap effectively empties
							// the buffer before storing the new data. 
							
							// If frameNumber is less than the previous frame number, the behavior is undefined.
							
							// Return false for failure (buffer not large enough).
				
	CARingBufferError	Fetch(AudioBufferList *abl, UInt32 nFrames, SampleTime frameNumber);
								// will alter mNumDataBytes of the buffers
	
	CARingBufferError	GetTimeBounds(SampleTime &startTime, SampleTime &endTime);
	
protected:

	int						FrameOffset(SampleTime frameNumber) { return (frameNumber & mCapacityFramesMask) * mBytesPerFrame; }

	CARingBufferError		ClipTimeBounds(SampleTime& startRead, SampleTime& endRead);
	
	// these should only be called from Store.
	SampleTime				StartTime() const { return mTimeBoundsQueue[mTimeBoundsQueuePtr & kGeneralRingTimeBoundsQueueMask].mStartTime; }
	SampleTime				EndTime()   const { return mTimeBoundsQueue[mTimeBoundsQueuePtr & kGeneralRingTimeBoundsQueueMask].mEndTime; }
	void					SetTimeBounds(SampleTime startTime, SampleTime endTime);
	
protected:
	Byte **					mBuffers;				// allocated in one chunk of memory
	int						mNumberChannels;
	UInt32					mBytesPerFrame;			// within one deinterleaved channel
	UInt32					mCapacityFrames;		// per channel, must be a power of 2
	UInt32					mCapacityFramesMask;
	UInt32					mCapacityBytes;			// per channel
	
	// range of valid sample time in the buffer
	typedef struct {
		volatile SampleTime		mStartTime;
		volatile SampleTime		mEndTime;
		volatile UInt32			mUpdateCounter;
	} TimeBounds;
	
	CARingBuffer::TimeBounds mTimeBoundsQueue[kGeneralRingTimeBoundsQueueSize];
	UInt32 mTimeBoundsQueuePtr;
};



#endif

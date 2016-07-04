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
//  BGM_DeviceTests.mm
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//

// Unit Include
#include "BGM_Device.h"

// Local Includes
#include "BGM_TestUtils.h"

// BGMDriver Includes
#include "BGM_Types.h"

// PublicUtility Includes
#include "CAException.h"


@interface BGM_DeviceTests : XCTestCase

@end


@implementation BGM_DeviceTests

- (void) setUp {
    [super setUp];
}

- (void) tearDown {
    // Reminder: add code here, above the super call
    [super tearDown];
}

- (void) testCustomPropertyMusicPlayerBundleID {
    BGM_Device& device = BGM_Device::GetInstance();
    
    // Convenience wrappers
    auto getBundleID = [&](UInt32 inDataSize = sizeof(CFStringRef)){
        CFStringRef bundleID = NULL;
        UInt32 outDataSize;
        
        device.GetPropertyData(/* inObjectID = */ kObjectID_Device,
                               /* inClientPID = */ 3,
                               /* inAddress = */ kBGMMusicPlayerBundleIDAddress,
                               /* inQualifierDataSize = */ 0,
                               /* inQualifierData = */ NULL,
                               /* inDataSize = */ inDataSize,
                               /* outDataSize = */ outDataSize,
                               /* outData = */ reinterpret_cast<void* __nonnull>(&bundleID));
        
        // This isn't technically required, but we're unlikely to ever want to return any more/less data from GetPropertyData.
        XCTAssertEqual(outDataSize, sizeof(CFStringRef));
        
        return (__bridge_transfer NSString*)bundleID;
    };
    
    auto setBundleID = [&](const CFStringRef* __nullable bundleID, UInt32 dataSize = sizeof(CFStringRef)){
        device.SetPropertyData(/* inObjectID = */ kObjectID_Device,
                               /* inClientPID = */ 1234,
                               /* inAddress = */ kBGMMusicPlayerBundleIDAddress,
                               /* inQualifierDataSize = */ 0,
                               /* inQualifierData = */ NULL,
                               /* inDataSize = */ dataSize,
                               /* inData = */ reinterpret_cast<const void* __nonnull>(bundleID));
    };
    
    // Should be set to the empty string by default.
    XCTAssertEqualObjects(getBundleID(), @"");
    
    // Should be able to set the property to an arbitrary string. (Purposefully not using CFSTR for this one just in case it
    // makes a difference.)
    CFStringRef newID = CFStringCreateWithCString(kCFAllocatorDefault, "test.bundle.ID", kCFStringEncodingUTF8);
    setBundleID(&newID);
    CFRelease(newID);
    XCTAssertEqualObjects(getBundleID(), @"test.bundle.ID");
    
    // Should be able to set the property back to the empty string.
    newID = CFSTR("");
    setBundleID(&newID);
    XCTAssertEqualObjects(getBundleID(), @"");
    
    // Arguments should be null-checked.
    BGMShouldThrow<BGM_RuntimeException>(self, [&](){
        UInt32 outDataSize;
        device.GetPropertyData(kObjectID_Device, 0, kBGMMusicPlayerBundleIDAddress, 0, NULL, sizeof(CFStringRef),
                               outDataSize, /* outData = */ reinterpret_cast<void* __nonnull>(NULL));
    });
    BGMShouldThrow<BGM_RuntimeException>(self, [&](){
        setBundleID(NULL);
    });
    
    // Invalid data should be rejected.
    BGMShouldThrow<CAException>(self, [&](){
        setBundleID((CFStringRef*)&kCFNull);
    });
    BGMShouldThrow<CAException>(self, [&](){
        CFStringRef nullRef = NULL;
        setBundleID(&nullRef);
    });
    BGMShouldThrow<CAException>(self, [&](){
        CFArrayRef array = (__bridge_retained CFArrayRef)@[ @1, @2 ];
        setBundleID((CFStringRef*)&array);
    });
    
    // Should throw if not given enough space for the return data.
    BGMShouldThrow<CAException>(self, [&](){
        getBundleID(/* inDataSize = */ 0);
    });
    
    newID = CFSTR("bundle");
    
    // Passing more data than needed should be fine as long as it starts with a CFStringRef.
    setBundleID(&newID, sizeof(CFStringRef) * 2);
    
    // Should throw if not enough data is passed.
    BGMShouldThrow<CAException>(self, [&](){
        setBundleID(&newID, sizeof(CFStringRef) - 1);
    });
}

// TODO: Performance tests?
- (void) testPerformanceExample {
    // This is an example of a performance test case.
    [self measureBlock:^{
        // Put the code you want to measure the time of here.
    }];
}

@end


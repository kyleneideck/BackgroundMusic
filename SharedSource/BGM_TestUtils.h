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
//  BGM_TestUtils.h
//  SharedSource
//
//  Copyright © 2016 Kyle Neideck
//

#ifndef __SharedSource__BGM_TestUtils__
#define __SharedSource__BGM_TestUtils__

// Test Framework
#import <XCTest/XCTest.h>

#if defined(__cplusplus)

// STL Includes
#include <functional>


// Fails the test if f doesn't throw ExpectedException when run.
// (The "self" argument is required by XCTFail, presumably so it can report the context.)
template<typename ExpectedException>
void BGMShouldThrow(XCTestCase* self, const std::function<void()>& f)
{
    try
    {
        f();
        XCTFail();
    }
    catch (ExpectedException)
    { }
}

#endif /* defined(__cplusplus) */

#endif /* __SharedSource__BGM_TestUtils__ */


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
//  BGMMusicPlayer.m
//  BGMApp
//
//  Copyright © 2016-2018 Kyle Neideck
//

// Self Include
#import "BGMMusicPlayer.h"

// PublicUtility Includes
#import "CADebugMacros.h"


#pragma clang assume_nonnull begin

@implementation BGMMusicPlayerBase

@synthesize musicPlayerID = _musicPlayerID;
@synthesize name = _name;
@synthesize bundleID = _bundleID;
@synthesize pid = _pid;
@synthesize selected = _selected;

- (instancetype) initWithMusicPlayerID:(NSUUID*)musicPlayerID
                                  name:(NSString*)name
                              bundleID:(NSString* __nullable)bundleID {
    return [self initWithMusicPlayerID:musicPlayerID name:name bundleID:bundleID pid:nil];
}

- (instancetype) initWithMusicPlayerID:(NSUUID*)musicPlayerID
                                  name:(NSString*)name
                              bundleID:(NSString* __nullable)bundleID
                                   pid:(NSNumber* __nullable)pid {
    if ((self = [super init])) {
        NSAssert(musicPlayerID, @"BGMMusicPlayerBase::initWithMusicPlayerID: !musicPlayerID");
        
        NSAssert([self conformsToProtocol:@protocol(BGMMusicPlayer)],
                 @"BGMMusicPlayerBase::initWithMusicPlayerID: !conformsToProtocol");
        
        _musicPlayerID = musicPlayerID;
        _name = name;
        _bundleID = bundleID;
        _pid = pid;
        _selected = NO;
    }

    return self;
}

+ (NSUUID*) makeID:(NSString*)musicPlayerIDString {
    NSUUID* __nullable musicPlayerID = [[NSUUID alloc] initWithUUIDString:musicPlayerIDString];
    NSAssert(musicPlayerID, @"BGMMusicPlayerBase::makeID: !musicPlayerID");
    
    return (NSUUID*)musicPlayerID;
}

#pragma mark BGMMusicPlayer default implementations

+ (NSArray<id<BGMMusicPlayer>>*) createInstances {
    return @[ [self new] ];
}

- (NSImage* __nullable) icon {
    NSString* __nullable bundleID = self.bundleID;
    NSString* __nullable bundlePath =
        (!bundleID ? nil : [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:(NSString*)bundleID]);
    
    return (!bundlePath ? nil : [[NSWorkspace sharedWorkspace] iconForFile:(NSString*)bundlePath]);
}

- (void) onSelect {
    _selected = YES;
}

- (void) onDeselect {
    _selected = NO;
}

@end

#pragma clang assume_nonnull end


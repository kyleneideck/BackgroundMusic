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
//  BGMUserDefaults.m
//  BGMApp
//
//  Copyright © 2016-2018 Kyle Neideck
//

// Self Include
#import "BGMUserDefaults.h"

// Local Includes
#import "BGM_Utils.h"


#pragma clang assume_nonnull begin

// Keys
static NSString* const BGMDefaults_AutoPauseMusicEnabled = @"AutoPauseMusicEnabled";
static NSString* const BGMDefaults_SelectedMusicPlayerID = @"SelectedMusicPlayerID";
static NSString* const BGMDefaults_PreferredDeviceUIDs = @"PreferredDeviceUIDs";

@implementation BGMUserDefaults {
    // The defaults object wrapped by this object.
    NSUserDefaults* defaults;
    // When we're not persisting defaults, settings are stored in this dictionary instead. This
    // var should only be accessed if 'defaults' is nil.
    NSMutableDictionary<NSString*,id>* transientDefaults;
}

- (instancetype) initWithDefaults:(NSUserDefaults* __nullable)inDefaults {
    if ((self = [super init])) {
        defaults = inDefaults;

        // Register the settings defaults.
        //
        // iTunes is the default music player, but we don't set BGMDefaults_SelectedMusicPlayerID
        // here so we know when it's never been set. (If it hasn't, we try using BGMDevice's
        // kAudioDeviceCustomPropertyMusicPlayerBundleID property to tell which music player should
        // be selected. See BGMMusicPlayers.)
        NSDictionary* defaultsDict = @{ BGMDefaults_AutoPauseMusicEnabled: @YES };

        if (defaults) {
            [defaults registerDefaults:defaultsDict];
        } else {
            transientDefaults = [defaultsDict mutableCopy];
        }
    }

    return self;
}

- (NSString* __nullable) selectedMusicPlayerID {
    return [self get:BGMDefaults_SelectedMusicPlayerID];
}

- (void) setSelectedMusicPlayerID:(NSString* __nullable)selectedMusicPlayerID {
    [self set:BGMDefaults_SelectedMusicPlayerID to:selectedMusicPlayerID];
}

- (BOOL) autoPauseMusicEnabled {
    return [self getBool:BGMDefaults_AutoPauseMusicEnabled];
}

- (void) setAutoPauseMusicEnabled:(BOOL)autoPauseMusicEnabled {
    [self setBool:BGMDefaults_AutoPauseMusicEnabled to:autoPauseMusicEnabled];
}

- (NSArray<NSString*>*) preferredDeviceUIDs {
    NSArray<NSString*>* __nullable uids = [self get:BGMDefaults_PreferredDeviceUIDs];
    return uids ? BGMNN(uids) : @[];
}

- (void) setPreferredDeviceUIDs:(NSArray<NSString*>*)devices {
    [self set:BGMDefaults_PreferredDeviceUIDs to:devices];
}

#pragma mark Implementation

- (id __nullable) get:(NSString*)key {
    return defaults ? [defaults objectForKey:key] : transientDefaults[key];
}

- (void) set:(NSString*)key to:(NSObject<NSCopying,NSSecureCoding>* __nullable)value {
    if (defaults) {
        [defaults setObject:value forKey:key];
    } else {
        transientDefaults[key] = value;
    }
}

- (BOOL) getBool:(NSString*)key {
    return defaults ? [defaults boolForKey:key] : [transientDefaults[key] boolValue];
}

- (void) setBool:(NSString*)key to:(BOOL)value {
    if (defaults) {
        [defaults setBool:value forKey:key];
    } else {
        transientDefaults[key] = [NSNumber numberWithBool:value];
    }
}

@end

#pragma clang assume_nonnull end


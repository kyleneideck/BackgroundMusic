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
//  Copyright © 2016 Kyle Neideck
//

// Self include
#import "BGMUserDefaults.h"


#pragma clang assume_nonnull begin

// Keys
static NSString* const BGMDefaults_AutoPauseMusicEnabled = @"AutoPauseMusicEnabled";
static NSString* const BGMDefaults_SelectedMusicPlayerID = @"SelectedMusicPlayerID";

@implementation BGMUserDefaults

- (void) registerDefaults {
    // iTunes is the default music player, but we don't set BGMDefaults_SelectedMusicPlayerID here so we know when
    // it's never been set. (If it hasn't, we try using BGMDevice's kAudioDeviceCustomPropertyMusicPlayerBundleID
    // property to tell which music player should be selected. See BGMMusicPlayers.)
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{ BGMDefaults_AutoPauseMusicEnabled: @YES }];
}

- (NSString* __nullable) selectedMusicPlayerID {
    return [[NSUserDefaults standardUserDefaults] stringForKey:BGMDefaults_SelectedMusicPlayerID];
}

- (void) setSelectedMusicPlayerID:(NSString* __nullable)selectedMusicPlayerID {
    [[NSUserDefaults standardUserDefaults] setObject:selectedMusicPlayerID
                                              forKey:BGMDefaults_SelectedMusicPlayerID];
}

- (BOOL) autoPauseMusicEnabled {
    return [[NSUserDefaults standardUserDefaults] boolForKey:BGMDefaults_AutoPauseMusicEnabled];
}

- (void) setAutoPauseMusicEnabled:(BOOL)autoPauseMusicEnabled {
    [[NSUserDefaults standardUserDefaults] setBool:autoPauseMusicEnabled
                                            forKey:BGMDefaults_AutoPauseMusicEnabled];
}

@end

#pragma clang assume_nonnull end


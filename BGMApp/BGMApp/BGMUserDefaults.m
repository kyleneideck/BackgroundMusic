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
//  Copyright © 2016-2019 Kyle Neideck
//

// Self Include
#import "BGMUserDefaults.h"

// Local Includes
#import "BGM_Utils.h"


#pragma clang assume_nonnull begin

// Keys
static NSString* const kDefaultKeyAutoPauseMusicEnabled = @"AutoPauseMusicEnabled";
static NSString* const kDefaultKeySelectedMusicPlayerID = @"SelectedMusicPlayerID";
static NSString* const kDefaultKeyPreferredDeviceUIDs   = @"PreferredDeviceUIDs";
static NSString* const kDefaultKeyStatusBarIcon         = @"StatusBarIcon";

// Labels for Keychain Data
static NSString* const kKeychainLabelGPMDPAuthCode =
    @"app.backgroundmusic: Google Play Music Desktop Player permanent auth code";

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
        // iTunes is the default music player, but we don't set kDefaultKeySelectedMusicPlayerID
        // here so we know when it's never been set. (If it hasn't, we try using BGMDevice's
        // kAudioDeviceCustomPropertyMusicPlayerBundleID property to tell which music player should
        // be selected. See BGMMusicPlayers.)
        NSDictionary* defaultsDict = @{ kDefaultKeyAutoPauseMusicEnabled: @YES };

        if (defaults) {
            [defaults registerDefaults:defaultsDict];
        } else {
            transientDefaults = [defaultsDict mutableCopy];
        }
    }

    return self;
}

#pragma mark Selected Music Player

- (NSString* __nullable) selectedMusicPlayerID {
    return [self get:kDefaultKeySelectedMusicPlayerID];
}

- (void) setSelectedMusicPlayerID:(NSString* __nullable)selectedMusicPlayerID {
    [self set:kDefaultKeySelectedMusicPlayerID to:selectedMusicPlayerID];
}

#pragma mark Auto-pause

- (BOOL) autoPauseMusicEnabled {
    return [self getBool:kDefaultKeyAutoPauseMusicEnabled];
}

- (void) setAutoPauseMusicEnabled:(BOOL)autoPauseMusicEnabled {
    [self setBool:kDefaultKeyAutoPauseMusicEnabled to:autoPauseMusicEnabled];
}

- (NSArray<NSString*>*) preferredDeviceUIDs {
    NSArray<NSString*>* __nullable uids = [self get:kDefaultKeyPreferredDeviceUIDs];
    return uids ? BGMNN(uids) : @[];
}

- (void) setPreferredDeviceUIDs:(NSArray<NSString*>*)devices {
    [self set:kDefaultKeyPreferredDeviceUIDs to:devices];
}

- (BGMStatusBarIcon) statusBarIcon {
    NSInteger icon = [self getInt:kDefaultKeyStatusBarIcon or:kBGMStatusBarIconDefaultValue];

    // Just in case we get an invalid value somehow.
    if ((icon < kBGMStatusBarIconMinValue) || (icon > kBGMStatusBarIconMaxValue)) {
        NSLog(@"BGMUserDefaults::statusBarIcon: Unknown BGMStatusBarIcon: %ld", (long)icon);
        icon = kBGMStatusBarIconDefaultValue;
    }

    return (BGMStatusBarIcon)icon;
}

- (void) setStatusBarIcon:(BGMStatusBarIcon)icon {
    [self setInt:kDefaultKeyStatusBarIcon to:icon];
}

#pragma mark Google Play Music Desktop Player

- (NSString* __nullable) googlePlayMusicDesktopPlayerPermanentAuthCode {
    // Try to read the permanent auth code from the user's keychain.
    NSDictionary<NSString*, NSObject*>* query = @{
        (__bridge NSString*)kSecClass: (__bridge NSString*)kSecClassGenericPassword,
        (__bridge NSString*)kSecAttrLabel: kKeychainLabelGPMDPAuthCode,
        (__bridge NSString*)kSecMatchLimit: (__bridge NSString*)kSecMatchLimitOne,
        (__bridge NSString*)kSecReturnData: @YES
    };

    CFTypeRef result = nil;
    OSStatus err = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);

    NSString* __nullable authCode = nil;

    // Check the return status, null check and check the type.
    if ((err == errSecSuccess) && result && (CFGetTypeID(result) == CFDataGetTypeID())) {
        // Convert it to a string.
        CFStringRef __nullable code =
                CFStringCreateFromExternalRepresentation(kCFAllocatorDefault,
                                                         result,
                                                         kCFStringEncodingUTF8);
        authCode = (__bridge_transfer NSString* __nullable)code;
    } else if (err != errSecItemNotFound) {
        NSString* __nullable errMsg =
                (__bridge_transfer NSString* __nullable)SecCopyErrorMessageString(err, nil);
        NSLog(@"Failed to read GPMDP auth code from keychain: %d, %@", err, errMsg);
    }

    // Release the data we read.
    if (result) {
        CFRelease(result);
    }

    return authCode;
}

- (void) setGooglePlayMusicDesktopPlayerPermanentAuthCode:(NSString* __nullable)authCode {
    if (authCode) {
        // Convert it to an NSData so we can store it in the user's keychain.
        NSData* authCodeData = [authCode dataUsingEncoding:NSUTF8StringEncoding];

        // Delete the old code if necessary. (There's an update function, but this takes less code.)
        if (self.googlePlayMusicDesktopPlayerPermanentAuthCode) {
            [self deleteGPMDPPermanentAuthCode];
        }

        // Store the code.
        [self addGPMDPPermanentAuthCode:authCodeData];
    } else {
        [self deleteGPMDPPermanentAuthCode];
    }
}

- (void) addGPMDPPermanentAuthCode:(NSData*)authCodeData {
    NSDictionary<NSString*, NSObject*>* attributes = @{
        (__bridge NSString*)kSecClass: (__bridge NSString*)kSecClassGenericPassword,
        (__bridge NSString*)kSecAttrLabel: kKeychainLabelGPMDPAuthCode,
        (__bridge NSString*)kSecValueData: authCodeData
    };

    OSStatus err = SecItemAdd((__bridge CFDictionaryRef)attributes, nil);

    // Just log an error if it failed.
    if (err != errSecSuccess) {
        NSString* errMsg = (__bridge_transfer NSString*)SecCopyErrorMessageString(err, nil);
        NSLog(@"Failed to store GPMDP auth code in keychain: %d, %@", err, errMsg);
    }
}

- (void) deleteGPMDPPermanentAuthCode {
    NSDictionary<NSString*, NSObject*>* query = @{
        (__bridge NSString*)kSecClass: (__bridge NSString*)kSecClassGenericPassword,
        (__bridge NSString*)kSecAttrLabel: kKeychainLabelGPMDPAuthCode
    };

    OSStatus err = SecItemDelete((__bridge CFDictionaryRef)query);

    // Just log an error if it failed.
    if (err != errSecSuccess) {
        NSString* errMsg = (__bridge_transfer NSString*)SecCopyErrorMessageString(err, nil);
        NSLog(@"Failed to delete GPMDP auth code from keychain: %d, %@", err, errMsg);
    }
}

#pragma mark General Accessors

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

// TODO: This method should have a default value param.
- (BOOL) getBool:(NSString*)key {
    return defaults ? [defaults boolForKey:key] : [transientDefaults[key] boolValue];
}

- (void) setBool:(NSString*)key to:(BOOL)value {
    if (defaults) {
        [defaults setBool:value forKey:key];
    } else {
        transientDefaults[key] = @(value);
    }
}

- (NSInteger) getInt:(NSString*)key or:(NSInteger)valueIfNil
{
    if (defaults) {
        if ([defaults objectForKey:key]) {
            return [defaults integerForKey:key];
        } else {
            return valueIfNil;
        }
    } else {
        if (transientDefaults[key]) {
            return [transientDefaults[key] intValue];
        } else {
            return valueIfNil;
        }
    }
}

- (void) setInt:(NSString*)key to:(NSInteger)value {
    if (defaults) {
        [defaults setInteger:value forKey:key];
    } else {
        transientDefaults[key] = @(value);
    }
}

@end

#pragma clang assume_nonnull end


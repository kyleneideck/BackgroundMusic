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
//  BGMAboutPanel.m
//  BGMApp
//
//  Copyright © 2016, 2024 Kyle Neideck
//

// Self Include
#import "BGMAboutPanel.h"

// Local Includes
#import "BGM_Types.h"

// PublicUtility Includes
#include "CADebugMacros.h"


NS_ASSUME_NONNULL_BEGIN

static NSInteger const kVersionLabelTag = 1;
static NSInteger const kCopyrightLabelTag = 2;
static NSInteger const kProjectWebsiteLabelTag = 3;
static NSInteger const kContributorsLabelTag = 4;

@implementation BGMAboutPanel {
    NSPanel* aboutPanel;
    
    NSTextField* versionLabel;
    NSTextField* copyrightLabel;
    NSTextField* websiteLabel;
    NSTextField* contributorsLabel;
    
    NSTextView* licenseView;
}

- (instancetype)initWithPanel:(NSPanel*)inAboutPanel licenseView:(NSTextView*)inLicenseView {
    if ((self = [super init])) {
        aboutPanel = inAboutPanel;
        
        versionLabel = [[aboutPanel contentView] viewWithTag:kVersionLabelTag];
        copyrightLabel = [[aboutPanel contentView] viewWithTag:kCopyrightLabelTag];
        websiteLabel = [[aboutPanel contentView] viewWithTag:kProjectWebsiteLabelTag];
        contributorsLabel = [[aboutPanel contentView] viewWithTag:kContributorsLabelTag];
        
        licenseView = inLicenseView;
        
        [self initAboutPanel];
    }
    
    return self;
}

- (void) initAboutPanel {
    // Set up the About Background Music window
    
    NSBundle* bundle = [NSBundle mainBundle];
    
    if (bundle == nil) {
        NSLog(@"Background Music: BGMAboutPanel::initAboutPanel: Could not find main bundle");
    } else {
        // Version number label
        NSString* __nullable version =
            [[bundle infoDictionary] objectForKey:@"CFBundleShortVersionString"];
        
        if (version) {
            versionLabel.stringValue = [NSString stringWithFormat:@"Version %@", version];
        }
        
        // Copyright notice label
        NSString* __nullable copyrightNotice =
            [[bundle infoDictionary] objectForKey:@"NSHumanReadableCopyright"];
        
        if (copyrightNotice) {
            // Remove the part that we replace with a link.
            copyrightLabel.stringValue =
                [((NSString*)copyrightNotice) stringByReplacingOccurrencesOfString:contributorsLabel.stringValue
                                                                        withString:@""];
        }
        
        // Project website link label
        websiteLabel.selectable = YES;
        websiteLabel.allowsEditingTextAttributes = YES;
        
        NSString* projectURL = [NSString stringWithUTF8String:kBGMProjectURL];
        NSFont* linkFont = websiteLabel.font ? websiteLabel.font : [NSFont labelFontOfSize:0.0];
        websiteLabel.attributedStringValue =
            [[NSAttributedString alloc] initWithString:projectURL
                                            attributes:@{ NSLinkAttributeName: projectURL,
                                                          NSFontAttributeName: linkFont }];
        
        // Contributors link label
        // TODO: Proper credits (i.e. in the app instead of just a link)
        contributorsLabel.selectable = YES;
        contributorsLabel.allowsEditingTextAttributes = YES;
        
        NSString* contributorsURL = [NSString stringWithUTF8String:kBGMContributorsURL];
        NSFont* cLinkFont = contributorsLabel.font ? contributorsLabel.font : [NSFont labelFontOfSize:0.0];
        contributorsLabel.attributedStringValue =
            [[NSAttributedString alloc] initWithString:contributorsLabel.stringValue
                                            attributes:@{ NSLinkAttributeName: contributorsURL,
                                                          NSFontAttributeName: cLinkFont }];
        
        // Load the text of the license into the text view
        NSString* __nullable licensePath = [bundle pathForResource:@"LICENSE" ofType:nil];
        
        NSError* err;
        NSString* __nullable licenseStr = (!licensePath ? nil :
            [NSString stringWithContentsOfFile:(NSString*)licensePath
                                      encoding:NSASCIIStringEncoding
                                         error:&err]);
        
        if (err || !licenseStr || [licenseStr isEqualToString:@""]) {
            NSLog(@"Error loading license file: %@", err);
            licenseStr = @"Error: could not open license file.";
        }
        
        licenseView.string = (NSString*)licenseStr;
        
        NSFont* __nullable font = [NSFont fontWithName:@"Andale Mono" size:0.0];
        if (font) {
            licenseView.textStorage.font = font;
        }
    }
}

- (void) show {
    DebugMsg("BGMAboutPanel::showAboutPanel: Opening \"About Background Music\" panel");
    
    // We have to make aboutPanel visible before calling [NSApp activateIgnoringOtherApps:YES]
    // or the app won't be activated the first time (not sure why it only happens the first
    // time) and aboutPanel won't open. WindowServer logs this explanation:
    // 0[SetFrontProcessWithInfo]: CPS: Rejecting the request for pid 1234 due to the activation count being 0; launch ts=19302059379458, current time=19314267188375, window count = 0.
    [aboutPanel setIsVisible:YES];
    [aboutPanel makeKeyAndOrderFront:self];
    
    // On macOS 14.4, aboutPanel needs "Release When Closed" unchecked in MainMenu.xib. Otherwise,
    // aboutPanel will never open again if you click the close button.
    
    // This is deprecated for NSApplication.activate, but that stops aboutPanel from ever being shown.
    [NSApp activateIgnoringOtherApps:YES];
    
    DebugMsg("BGMAboutPanel::showAboutPanel: Finished opening panel. "
             "aboutPanel.isVisible %d, aboutPanel.isKeyWindow %d, NSApp.isActive %d",
             aboutPanel.isVisible,
             aboutPanel.isKeyWindow,
             NSApp.isActive);
}

@end

@implementation BGMLinkField

- (void) resetCursorRects {
    // Change the mouse cursor when hovering over the link. (It does change by default, but only after
    // you've clicked it once.)
    [self addCursorRect:self.bounds cursor:[NSCursor pointingHandCursor]];
}

@end

NS_ASSUME_NONNULL_END


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
//  Copyright © 2016 Kyle Neideck
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

@implementation BGMAboutPanel {
    NSPanel* aboutPanel;
    
    NSTextField* versionLabel;
    NSTextField* copyrightLabel;
    NSTextField* websiteLabel;
    
    NSTextView* licenseView;
}

- (instancetype)initWithPanel:(NSPanel*)inAboutPanel licenseView:(NSTextView*)inLicenseView {
    if ((self = [super init])) {
        aboutPanel = inAboutPanel;
        
        versionLabel = [[aboutPanel contentView] viewWithTag:kVersionLabelTag];
        copyrightLabel = [[aboutPanel contentView] viewWithTag:kCopyrightLabelTag];
        websiteLabel = [[aboutPanel contentView] viewWithTag:kProjectWebsiteLabelTag];
        
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
            copyrightLabel.stringValue = (NSString*)copyrightNotice;
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
    [NSApp activateIgnoringOtherApps:YES];
    [aboutPanel setIsVisible:YES];
    [aboutPanel makeKeyAndOrderFront:self];
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


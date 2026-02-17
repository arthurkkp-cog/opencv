/* The file is the modified version of window_cocoa.mm from opencv-cocoa project by Andre Cohen */

/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                         License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2010, Willow Garage Inc., all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/
#include "precomp.hpp"
#include "opencv2/imgproc.hpp"

#import <TargetConditionals.h>

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
/*** begin IPhone OS Stubs ***/
// When highgui functions are referred to on iPhone OS, they will fail silently.
//*** end IphoneOS Stubs ***/
#else

#import <Cocoa/Cocoa.h>

#include <iostream>

const int MIN_SLIDER_WIDTH=200;

static NSApplication *application = nil;
static NSMutableDictionary *windows = nil;
static bool wasInitialized = false;

@interface CVView : NSView
@property(retain) NSView *imageView;
@property(retain) NSImage *image;
@property int sliderHeight;
- (void)setImageData:(CvArr *)arr;
@end

@interface CVSlider : NSView {
    NSSlider *slider;
    NSTextField *name;
    NSString *initialName;
    int *value;
    void *userData;
    CvTrackbarCallback callback;
    CvTrackbarCallback2 callback2;
}
@property(retain) NSSlider *slider;
@property(retain) NSTextField *name;
@property(retain) NSString *initialName;
@property(assign) int *value;
@property(assign) void *userData;
@property(assign) CvTrackbarCallback callback;
@property(assign) CvTrackbarCallback2 callback2;
@end

@interface CVWindow : NSWindow {
    NSMutableDictionary *sliders;
    NSMutableArray *slidersKeys;
    CvMouseCallback mouseCallback;
    void *mouseParam;
    BOOL autosize;
    BOOL firstContent;
    int status;
    int x0, y0;
}
@property(assign) CvMouseCallback mouseCallback;
@property(assign) void *mouseParam;
@property(assign) BOOL autosize;
@property(assign) BOOL firstContent;
@property(assign) int x0;
@property(assign) int y0;
@property(retain) NSMutableDictionary *sliders;
@property(retain) NSMutableArray *slidersKeys;
@property(readwrite) int status;
- (CVView *)contentView;
- (void)cvSendMouseEvent:(NSEvent *)event type:(int)type flags:(int)flags;
- (void)cvMouseEvent:(NSEvent *)event;
- (void)createSliderWithName:(const char *)name maxValue:(int)max value:(int *)value callback:(CvTrackbarCallback)callback;
@end

static CVWindow *cvGetWindow(const char *name) {
    CVWindow *retval = nil;
    @autoreleasepool{
        NSString *cvname = [NSString stringWithFormat:@"%s", name];
        retval = (CVWindow*) [windows valueForKey:cvname];
        if (retval != nil) {
            [retval retain];
        }
    }
    return [retval autorelease];
}

static NSSize constrainAspectRatio(NSSize base, NSSize constraint) {
    CGFloat heightDiff = (base.height / constraint.height);
    CGFloat widthDiff = (base.width / constraint.width);
    if (heightDiff == 0) heightDiff = widthDiff;
    if (widthDiff == heightDiff) {
        return base;
    }
    else if (widthDiff > heightDiff) {
        NSSize out = { constraint.width / constraint.height * base.height, base.height };
        return out;
    }
    else {
        NSSize out = { base.width, constraint.height / constraint.width * base.width };
        return out;
    }
}

@implementation CVWindow

@synthesize mouseCallback;
@synthesize mouseParam;
@synthesize autosize;
@synthesize firstContent;
@synthesize x0;
@synthesize y0;
@synthesize sliders;
@synthesize slidersKeys;
@synthesize status;

- (void)cvSendMouseEvent:(NSEvent *)event type:(int)type flags:(int)flags {
    (void)event;
    NSPoint mp = [NSEvent mouseLocation];
    mp = [self convertScreenToBase: mp];
    CVView *contentView = [self contentView];
    NSSize viewSize = contentView.frame.size;
    if (contentView.imageView) {
        viewSize = contentView.imageView.frame.size;
    }
    else {
        viewSize.height -= contentView.sliderHeight;
    }
    mp.y = viewSize.height - mp.y;

    NSSize imageSize = contentView.image.size;
    mp.y *= (imageSize.height / std::max(viewSize.height, 1.));
    mp.x *= (imageSize.width / std::max(viewSize.width, 1.));

    if( [event type] == NSEventTypeScrollWheel ) {
        if( event.hasPreciseScrollingDeltas ) {
            mp.x = int(event.scrollingDeltaX);
            mp.y = int(event.scrollingDeltaY);
        } else {
            mp.x = int(event.scrollingDeltaX / 0.100006);
            mp.y = int(event.scrollingDeltaY / 0.100006);
        }
        if( mp.x && !mp.y && cv::EVENT_MOUSEWHEEL == type ) {
            type = cv::EVENT_MOUSEHWHEEL;
        }
        mouseCallback(type, mp.x, mp.y, flags, mouseParam);
    } else if( mp.x >= 0 && mp.y >= 0 && mp.x < imageSize.width && mp.y < imageSize.height ) {
        mouseCallback(type, mp.x, mp.y, flags, mouseParam);
    }
}

- (void)cvMouseEvent:(NSEvent *)event {
    if(!mouseCallback)
        return;

    int flags = 0;
    if([event modifierFlags] & NSShiftKeyMask)		flags |= cv::EVENT_FLAG_SHIFTKEY;
    if([event modifierFlags] & NSControlKeyMask)	flags |= cv::EVENT_FLAG_CTRLKEY;
    if([event modifierFlags] & NSAlternateKeyMask)	flags |= cv::EVENT_FLAG_ALTKEY;

    //modified code using ternary operator:
    if ([event type] == NSLeftMouseDown) {
    [self cvSendMouseEvent:event
                      type:([event modifierFlags] & NSControlKeyMask) ? cv::EVENT_RBUTTONDOWN : cv::EVENT_LBUTTONDOWN
                     flags:flags | (([event modifierFlags] & NSControlKeyMask) ? cv::EVENT_FLAG_RBUTTON : cv::EVENT_FLAG_LBUTTON)];
    }

    if ([event type] == NSLeftMouseUp) {
        [self cvSendMouseEvent:event
                        type:([event modifierFlags] & NSControlKeyMask) ? cv::EVENT_RBUTTONUP : cv::EVENT_LBUTTONUP
                        flags:flags | (([event modifierFlags] & NSControlKeyMask) ? cv::EVENT_FLAG_RBUTTON : cv::EVENT_FLAG_LBUTTON)];
    }

    if([event type] == NSRightMouseDown){[self cvSendMouseEvent:event type:cv::EVENT_RBUTTONDOWN flags:flags | cv::EVENT_FLAG_RBUTTON];}
    if([event type] == NSRightMouseUp)	{[self cvSendMouseEvent:event type:cv::EVENT_RBUTTONUP   flags:flags | cv::EVENT_FLAG_RBUTTON];}
    if([event type] == NSOtherMouseDown){[self cvSendMouseEvent:event type:cv::EVENT_MBUTTONDOWN flags:flags];}
    if([event type] == NSOtherMouseUp)	{[self cvSendMouseEvent:event type:cv::EVENT_MBUTTONUP   flags:flags];}
    if([event type] == NSMouseMoved)	{[self cvSendMouseEvent:event type:cv::EVENT_MOUSEMOVE   flags:flags];}
    if([event type] == NSLeftMouseDragged) {[self cvSendMouseEvent:event type:cv::EVENT_MOUSEMOVE   flags:flags | cv::EVENT_FLAG_LBUTTON];}
    if([event type] == NSRightMouseDragged)	{[self cvSendMouseEvent:event type:cv::EVENT_MOUSEMOVE   flags:flags | cv::EVENT_FLAG_RBUTTON];}
    if([event type] == NSOtherMouseDragged)	{[self cvSendMouseEvent:event type:cv::EVENT_MOUSEMOVE   flags:flags | cv::EVENT_FLAG_MBUTTON];}
    if([event type] == NSEventTypeScrollWheel) {[self cvSendMouseEvent:event type:cv::EVENT_MOUSEWHEEL   flags:flags ];}
}

-(void)scrollWheel:(NSEvent *)theEvent {
    [self cvMouseEvent:theEvent];
}
- (void)keyDown:(NSEvent *)theEvent {
    [super keyDown:theEvent];
}
- (void)rightMouseDragged:(NSEvent *)theEvent {
    [self cvMouseEvent:theEvent];
}
- (void)rightMouseUp:(NSEvent *)theEvent {
    [self cvMouseEvent:theEvent];
}
- (void)rightMouseDown:(NSEvent *)theEvent {
    [self cvMouseEvent:theEvent];
}
- (void)mouseMoved:(NSEvent *)theEvent {
    [self cvMouseEvent:theEvent];
}
- (void)otherMouseDragged:(NSEvent *)theEvent {
    [self cvMouseEvent:theEvent];
}
- (void)otherMouseUp:(NSEvent *)theEvent {
    [self cvMouseEvent:theEvent];
}
- (void)otherMouseDown:(NSEvent *)theEvent {
    [self cvMouseEvent:theEvent];
}
- (void)mouseDragged:(NSEvent *)theEvent {
    [self cvMouseEvent:theEvent];
}
- (void)mouseUp:(NSEvent *)theEvent {
    [self cvMouseEvent:theEvent];
}
- (void)mouseDown:(NSEvent *)theEvent {
    [self cvMouseEvent:theEvent];
}

- (void)createSliderWithName:(const char *)name maxValue:(int)max value:(int *)value callback:(CvTrackbarCallback)callback {
    if(sliders == nil)
        sliders = [[NSMutableDictionary alloc] init];

    if(slidersKeys == nil)
        slidersKeys = [[NSMutableArray alloc] init];

    NSString *cvname = [NSString stringWithFormat:@"%s", name];

    // Avoid overwriting slider
    if([sliders valueForKey:cvname]!=nil)
        return;

    // Create slider
    CVSlider *slider = [[CVSlider alloc] init];
    [[slider name] setStringValue:cvname];
    slider.initialName = [NSString stringWithFormat:@"%s", name];
    [[slider slider] setMaxValue:max];
    [[slider slider] setMinValue:0];
    if(value)
    {
        [[slider slider] setIntValue:*value];
        [slider setValue:value];
        NSString *temp = [slider initialName];
        NSString *text = [NSString stringWithFormat:@"%@ %d", temp, *value];
        [[slider name] setStringValue: text];
    }
    if(callback)
        [slider setCallback:callback];

    // Save slider
    [sliders setValue:slider forKey:cvname];
    [slidersKeys addObject:cvname];
    [[self contentView] addSubview:slider];

    //update contentView size to contain sliders
    NSSize viewSize=[[self contentView] frame].size,
           sliderSize=[slider frame].size;
    viewSize.height += sliderSize.height;
    viewSize.width = std::max<int>(viewSize.width, MIN_SLIDER_WIDTH);

    // Update slider sizes
    [self contentView].sliderHeight += sliderSize.height;

    if ([[self contentView] image] && ![[self contentView] imageView]) {
        [[self contentView] setNeedsDisplay:YES];
    }

    //update window size to contain sliders
    [self setContentSize: viewSize];
}

- (CVView *)contentView {
    return (CVView*)[super contentView];
}

@end

@implementation CVView

@synthesize image;

- (id)init {
    [super init];
    return self;
}

- (void)setImageData:(CvArr *)arr {
    cv::Mat arrMat = cv::cvarrToMat(arr);
    @autoreleasepool{
        NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                    pixelsWide:arrMat.cols
                    pixelsHigh:arrMat.rows
                    bitsPerSample:8
                    samplesPerPixel:3
                    hasAlpha:NO
                    isPlanar:NO
                    colorSpaceName:NSDeviceRGBColorSpace
                    bitmapFormat: kCGImageAlphaNone
                    bytesPerRow:((arrMat.cols * 3 + 3) & -4)
                    bitsPerPixel:24];

        if (bitmap) {
            cv::Mat dst(arrMat.rows, arrMat.cols, CV_8UC3, [bitmap bitmapData], [bitmap bytesPerRow]);
            convertToShow(arrMat, dst);
        }
        else {
            // It's not guaranteed to like the bitsPerPixel:24, but this is a lot slower so we'd rather not do it
            bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                pixelsWide:arrMat.cols
                pixelsHigh:arrMat.rows
                bitsPerSample:8
                samplesPerPixel:3
                hasAlpha:NO
                isPlanar:NO
                colorSpaceName:NSDeviceRGBColorSpace
                bytesPerRow:(arrMat.cols * 4)
                bitsPerPixel:32];
            cv::Mat dst(arrMat.rows, arrMat.cols, CV_8UC4, [bitmap bitmapData], [bitmap bytesPerRow]);
            convertToShow(arrMat, dst);
        }

        if( image ) {
            [image release];
        }

        image = [[NSImage alloc] init];
        [image addRepresentation:bitmap];
        [bitmap release];

        // This isn't supported on older versions of macOS
        // The performance issues this solves are mainly on newer versions of macOS, so that's fine
        if( floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_5 ) {
            if (![self imageView]) {
                [self setImageView:[[NSView alloc] init]];
                [[self imageView] setWantsLayer:true];
                [self addSubview:[self imageView]];
            }

            [[[self imageView] layer] setContents:image];

            NSRect imageViewFrame = [self frame];
            imageViewFrame.size.height -= [self sliderHeight];
            NSRect constrainedFrame = { imageViewFrame.origin, constrainAspectRatio(imageViewFrame.size, [image size]) };
            [[self imageView] setFrame:constrainedFrame];
        }
        else {
            NSRect redisplayRect = [self frame];
            redisplayRect.size.height -= [self sliderHeight];
            [self setNeedsDisplayInRect:redisplayRect];
        }
    }
}

- (void)setFrameSize:(NSSize)size {
    [super setFrameSize:size];

    int height = size.height;

    @autoreleasepool{
        CVWindow *cvwindow = (CVWindow *)[self window];
        if ([cvwindow respondsToSelector:@selector(sliders)]) {
            for(NSString *key in [cvwindow slidersKeys]) {
                CVSlider *slider = [[cvwindow sliders] valueForKey:key];
                NSRect r = [slider frame];
                r.origin.y = height - r.size.height;
                r.size.width = [[cvwindow contentView] frame].size.width;

                CGRect sliderRect = slider.slider.frame;
                CGFloat targetWidth = r.size.width - (sliderRect.origin.x + 10);
                sliderRect.size.width = targetWidth < 0 ? 0 : targetWidth;
                slider.slider.frame = sliderRect;

                [slider setFrame:r];
                height -= r.size.height;
            }
        }
        NSRect frame = self.frame;
        if (frame.size.height < self.sliderHeight) {
            frame.size.height = self.sliderHeight;
            self.frame = frame;
        }
        if ([self imageView]) {
            NSRect imageViewFrame = frame;
            imageViewFrame.size.height -= [self sliderHeight];
            NSRect constrainedFrame = { imageViewFrame.origin, constrainAspectRatio(imageViewFrame.size, [image size]) };
            [[self imageView] setFrame:constrainedFrame];
        }
    }
}

- (void)drawRect:(NSRect)rect {
    [super drawRect:rect];
    // If imageView exists, all drawing will be done by it and nothing needs to happen here
    if ([self image] && ![self imageView]) {
        @autoreleasepool{
            if(image != nil) {
                [image drawInRect: [self frame]
                        fromRect: NSZeroRect
                        operation: NSCompositeSourceOver
                        fraction: 1.0];
            }
        }
    }
}

@end

@implementation CVSlider

@synthesize slider;
@synthesize name;
@synthesize initialName;
@synthesize value;
@synthesize userData;
@synthesize callback;
@synthesize callback2;

- (id)init {
    [super init];

    callback = NULL;
    value = NULL;
    userData = NULL;

    [self setFrame:NSMakeRect(0,0,200,30)];

    name = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 0,110, 25)];
    [name setEditable:NO];
    [name setSelectable:NO];
    [name setBezeled:NO];
    [name setBordered:NO];
    [name setDrawsBackground:NO];
    [[name cell] setLineBreakMode:NSLineBreakByTruncatingTail];
    [self addSubview:name];

    slider = [[NSSlider alloc] initWithFrame:NSMakeRect(120, 0, 70, 25)];
    [slider setAutoresizingMask:NSViewWidthSizable];
    [slider setMinValue:0];
    [slider setMaxValue:100];
    [slider setContinuous:YES];
    [slider setTarget:self];
    [slider setAction:@selector(handleSliderNotification:)];
    [self addSubview:slider];

    [self setAutoresizingMask:NSViewWidthSizable];

    return self;
}

- (void)handleSliderNotification:(NSNotification *)notification {
    (void)notification;
    [self handleSlider];
}

- (void)handleSlider {
    int pos = [slider intValue];
    NSString *temp = [self initialName];
    NSString *text = [NSString stringWithFormat:@"%@ %d", temp, pos];
    [name setStringValue: text];
    if(value)
        *value = pos;
    if(callback)
        callback(pos);
    if(callback2)
        callback2(pos, userData);
}

@end

#endif

/* End of file. */

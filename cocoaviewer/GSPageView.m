#import "GSPageView.h"

@implementation GSPageView

- (BOOL)isOpaque
{
    return YES;	// don't worry about transparency for now
}

- (void)drawRect: (NSRect)rect
{
    NSRect myBounds = [self bounds];
        
    // clear the background
    [[NSColor whiteColor] set];
    [NSBezierPath fillRect:myBounds];
    
    // border
    [[NSColor blackColor] set];
    [NSBezierPath strokeRect:myBounds];
    
    // crosshairs
    [[NSColor lightGrayColor] set];
    [NSBezierPath
        strokeLineFromPoint:NSMakePoint(0, (myBounds.size.height/2.0))
        toPoint:NSMakePoint(myBounds.size.width, (myBounds.size.height/2.0))];
    [NSBezierPath
        strokeLineFromPoint:NSMakePoint((myBounds.size.width/2.0), 0)
        toPoint:NSMakePoint((myBounds.size.width/2.0), myBounds.size.height)];
}

@end

#import <Cocoa/Cocoa.h>

@interface MyView : NSView
{
    NSString	*string;
    NSFont	*font;
    
    NSBezierPath *arrowpath;
    NSBezierPath *circlepath;
}

- (void) setstring: (NSString *)value;
- (void) setfont: (NSFont *)value;
- (BOOL) isFlipped;

- (void) awakeFromNib;
- (BOOL) isOpaque;

- (NSBezierPath *) arrowpath;
- (void) setarrowpath: (NSBezierPath *)newpath;
- (NSBezierPath *) circlepath;
- (void) setcirclepath: (NSBezierPath *)newpath;

- (NSBezierPath *) createarrowpath;
- (NSBezierPath *) createcirclepath;

- (void) drawarrow;
- (void) drawcircle;

@end

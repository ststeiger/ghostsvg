#import "MyView.h"

@implementation MyView

- (BOOL) isFlipped
{
    return YES;
}

- (id) initWithFrame: (NSRect)frame
{
    [super initWithFrame:frame];
    [self setstring: @"Hello, Sailor!"];
    [self setfont: [NSFont systemFontOfSize: 12]];

    return self;
}

- (void) setstring: (NSString *)value
{
    [string autorelease];
    string = [value copy];
    [self setNeedsDisplay: YES];
}

- (void) setfont: (NSFont *)value
{
    [font autorelease];
    font = [value retain];
    [self setNeedsDisplay: YES];
}

- (void) awakeFromNib
{
    [self setarrowpath: [self createarrowpath]];
    [self setcirclepath: [self createcirclepath]];
}

- (BOOL) isOpaque
{
    return YES;
}

- (void) drawRect: (NSRect)rect
{
    NSRect myBounds = [self bounds];
    NSMutableDictionary *attrs = [NSMutableDictionary dictionary];
    id tiger = [[NSImage alloc] initByReferencingFile: @"tiger.png"];
    
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
    
    [attrs setObject: font forKey: NSFontAttributeName];
    [string drawAtPoint:
        NSMakePoint((3.0*myBounds.size.width/4.0), 5) withAttributes: attrs];
    
    // image compositing
    [tiger compositeToPoint:
        NSMakePoint(myBounds.size.width/2.0,myBounds.size.height/2.0)
        operation: NSCompositeCopy];
        
    // stored path objects
    [self drawarrow];
    [self drawcircle];
}

- (NSBezierPath *) arrowpath
{
    return arrowpath;
}

- (void) setarrowpath: (NSBezierPath *)newpath
{
    [newpath retain];
    [arrowpath release];
    arrowpath = newpath;
}

- (NSBezierPath *) circlepath
{
    return circlepath;
}

- (void) setcirclepath: (NSBezierPath *)newpath
{
    [newpath retain];
    [circlepath release];
    circlepath = newpath;
}

- (NSBezierPath *) createarrowpath
{
    // point we'll reuse in creating the model
    NSPoint point;
    
    // ratios for arrow head size
    float headlengthratio = 0.3;
    float headbaseratio = 0.7;
    float arrowheadwidth = 0.3;
    
    // path used to hold the model
    NSBezierPath *model = [NSBezierPath bezierPath];
    
    // start at the origin
    point.x = point.y = 0;
    [model moveToPoint: point];
    
    // line to base of arrowhead
    point.x = 1 - (headlengthratio*headbaseratio);
    [model lineToPoint: point];
    
    // 'upper' corner
    point.x = 1 - headlengthratio;
    point.y = headlengthratio * tan(arrowheadwidth);
    [model lineToPoint: point];
    
    // tip of arrowhead
    point.x = 1;
    point.y = 0;
    [model lineToPoint: point];
    
    // 'lower' corner
    point.x = 1 - headlengthratio;
    point.y = - headlengthratio * tan(arrowheadwidth);
    [model lineToPoint: point];
    
    // back to base
    point.x = 1 - (headlengthratio*headbaseratio);
    point.y = 0;
    [model lineToPoint: point];
    
    // finish
    [model closePath];
    
    return model;
}

- (NSBezierPath *) createcirclepath
{
    NSPoint point;
    
    NSBezierPath *model = [NSBezierPath bezierPath];
    
    // circle around the origin
    point.x = point.y = 0;
    [model appendBezierPathWithArcWithCenter:point
        radius:1.0 startAngle:0 endAngle:360];
        
    // plus sign
    point.x = 0.5;
    point.y = 0;
    [model moveToPoint:point];
    point.x = -0.5;
    [model lineToPoint:point];
    point.x = 0;
    point.y = 0.5;
    [model moveToPoint:point];
    point.y = -0.5;
    [model lineToPoint:point];
    
    return model;
}

- (void) drawarrow
{
    NSBezierPath *arrow;
    
    NSAffineTransform *scale;
    NSAffineTransform *rotate;
    NSAffineTransform *translate;
    
    // combined transforms
    NSAffineTransform *arrowtransform;
    
    NSPoint screenlocation = NSMakePoint(100,100);
    NSColor *arrowstrokecolor = [NSColor blueColor];
    NSColor *arrowfillcolor = [NSColor greenColor];
    
    float scalefactor = 100.0;
    scale = [NSAffineTransform transform];
    [scale scaleBy: scalefactor];
    
    rotate = [NSAffineTransform transform];
    [rotate rotateByDegrees: 45];
    
    translate = [NSAffineTransform transform];
    [translate translateXBy: screenlocation.x yBy: screenlocation.y];
    
    arrowtransform = [NSAffineTransform transform];
    [arrowtransform appendTransform:scale];
    [arrowtransform appendTransform:rotate];
    [arrowtransform appendTransform:translate];
    
    // modify the model by the transform
    arrow = [arrowtransform transformBezierPath: [self arrowpath]];
    
    // draw
    [arrowstrokecolor set];
    [arrow setLineWidth:2.0];
    [arrow stroke];
    [arrowfillcolor set];
    [arrow fill];
}

- (void) drawcircle
{
}
@end

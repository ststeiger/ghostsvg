#include <Ghostscript/errors.h>
#include <Ghostscript/iapi.h>

#import "GSPageView.h"

@implementation GSPageView

- (gs_main_instance *)gs_instance
{
    return gs_instance;
}

- (NSString *)file
{
    return file;
}

- (void)setFile: (NSString *)newFile
{
    [file autorelease];
    file = [newFile copy];
}

- (NSImage *)image
{
    return image;
}

- (void)setImage: (NSImage *)newImage
{
    [image autorelease];
    image = [newImage retain];
}

- (void)awakeFromNib
{
    NSString *path = @"/Users/giles/projects/ghostscript/gs/examples/tiger.eps";
    const char start_string = "systemdict /start get exec\n";
    int error_code;
    
    // load a test image
    [self setImage: [[NSImage alloc]
        initWithContentsOfFile: [[NSBundle bundleForClass:[self class]] 			pathForResource:@"tiger" ofType:@"png"]]];
     
    // hardwire file to open for now
    [self setFile: path];
    
    // initialize the ghostscript engine
    {
        int error_code;
        const char *arg0 = "gscocoa";
        const char *arg1 = "-sDEVICE=display";
        const char *arg2 = "-dDisplayHandle=0";
        const char *arg3 = "-dDisplayFormat=0";
        const char *arg4 = [[self file] cString];
        char *argv[5] = {arg0, arg1, arg2, arg3, arg4};
        int argc = 5;
        
        error_code = gsapi_new_instance(&gs_instance, NULL);
        if (error_code < 0) {
            NSLog(@"unable to create ghostscript instance: error code %d", 			error_code);
            exit(1);
        }
        error_code = gsapi_init_with_args(gs_instance, argc, argv);
        gsapi_exit(gs_instance);
        
        gsapi_delete_instance(gs_instance);
        }
}

- (BOOL)isOpaque
{
    return YES;	// don't worry about transparency for now
}

- (void)drawRect: (NSRect)rect
{
    NSRect myBounds = [self bounds];
    NSRect imageRect = NSMakeRect(0,0,0,0);
        
    // clear the background
    [[NSColor whiteColor] set];
    [NSBezierPath fillRect:myBounds];
    
    // border
    [[NSColor blackColor] set];
    [NSBezierPath strokeRect:myBounds];
    
    // test image
    imageRect.size = [image size];
    [image drawAtPoint:NSMakePoint(0,0) fromRect:imageRect 	operation:NSCompositeCopy fraction:0.5];
    
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

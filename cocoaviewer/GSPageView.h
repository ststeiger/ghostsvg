/* GSPageView */

#import <Cocoa/Cocoa.h>

@interface GSPageView : NSView
{
    NSString *file;
    NSImage *image;
    
    gs_main_instance *gs_instance;
}
- (NSString *)file;
- (void)setFile: (NSString *)newFile;

- (NSImage *)image;
- (void)setImage: (NSImage *)newImage;

- (BOOL)isOpaque;
- (void)awakeFromNib;
- (void)drawRect: (NSRect)rect;
@end

/* 
   carbonviewer

   a simple viewer example for the Ghostscript shared library
   
   $Id$

*/

#include <Carbon.h>
#include <stdio.h>
#include <string.h>
#include <sioux.h>

#include "errors.h"
#include "iapi.h"
#include "gdevdsp.h"

#include "gs_shlib.h"
#include "document.h"

#define mbar_id 128



void init();
void makemenu();
WindowRef makewindow();

/* call whatever intialization we need */
static void init()
{
	//InitCursor();
	makemenu();
}

static void makemenu()
{
	MenuBarHandle mbar;
	
	mbar = GetNewMBar(mbar_id);
	SetMenuBar(mbar);
	DrawMenuBar();
	DisposeMenuBar(mbar);
}

static WindowRef makewindow()
{
	WindowRef window;
	WindowAttributes attrs;
	Rect r;
	CFStringRef title;
	OSStatus result;
	
	SetRect(&r, 50,50, 640, 400);
	attrs = kWindowStandardDocumentAttributes
		| kWindowStandardHandlerAttribute
		| kWindowInWindowMenuAttribute;
	result = CreateNewWindow(kDocumentWindowClass, attrs, &r, &window);
	
	title = CFSTR("Ghostscript (carbon)");
	result = SetWindowTitleWithCFString(window, title);
	CFRelease(title);
	
	RepositionWindow(window, NULL, kWindowCascadeOnMainScreen);
	ShowWindow(window);
	
	return window;
}

static void draw_hello()
{
	Rect frame;
	
	SetRect(&frame, 10, 50, 50, 100);
	FrameRect(&frame);
	
	MoveTo(10, 120);
	DrawString("\pHello, sailor.");
}

static void draw_image(WindowRef window)
{
	GWorldPtr	offworld;
	CGrafPtr	onport, saveport;
	GDHandle	savedevice;
	Rect		bounds;
	PixMapHandle	offpix, onpix;
	QDErr		err;
	int			i;
	
	GetGWorld(&saveport, &savedevice);
	onport = GetWindowPort(window);
	
	onpix = GetPortPixMap(onport);
	GetPixBounds(onpix, &bounds);
	err = NewGWorld(&offworld, 32, &bounds, NULL, NULL, 0);
	SetGWorld(offworld, NULL);
	
	offpix = GetGWorldPixMap(offworld);
	LockPixels(offpix);
	EraseRect(&bounds);
	draw_hello();
	UnlockPixels(offpix);
	
	SetGWorld(saveport, savedevice);
	SetPort(onport);
	LockPixels(offpix);
	CopyBits((BitMap*)*offpix, (BitMap*)*onpix,
			&bounds, &bounds, srcCopy, NULL);
	UnlockPixels(offpix);
	
	DisposeGWorld(offworld);
	SetGWorld(saveport, savedevice);
}

static int update_doc_window(cv_doc_t *doc)
{
	CGrafPtr	onport, saveport;
	GDHandle	savedevice;
	Rect		bounds, pagebounds;
	PixMapHandle	offpix, onpix;
	
	GetGWorld(&saveport, &savedevice);
	
	offpix = GetGWorldPixMap(doc->pages->image);
	SetRect(&pagebounds, 0, 0, doc->pages->width, doc->pages->height);
	
	BeginUpdate(doc->window);
	
	GetWindowBounds(doc->window, kWindowContentRgn, &bounds);
	onport = GetWindowPort(doc->window);	
	onpix = GetPortPixMap(onport);
	
	SetPort(onport);
	LockPixels(offpix);
	CopyBits((BitMap*)*offpix, (BitMap*)*onpix,
			&pagebounds, &bounds, srcCopy, NULL);
	UnlockPixels(offpix);

	EndUpdate(doc->window);
				
	SetGWorld(saveport, savedevice);

	return 0;
}

static int gscallback_stdin(void *instance, char *buf, int len)
{
	return 0;
}

static int gscallback_stdout(void *instance, const char *str, int len)
{
	return 0;
}

static int display_open(void *handle, void *device)
{
	fprintf(stderr, "display_open called (handle 0x%08x)\n", handle);
	return 0;
}

static int display_preclose(void *handle, void *device)
{
	fprintf(stderr, "display_preclose called (handle 0x%08x)\n", handle);
	return 0;
}

static int display_close(void *handle, void *device)
{

	fprintf(stderr, "display_close called (handle 0x%08x)\n", handle);
	return 0;
}

static int display_presize(void *handle, void *device,
	int width, int height, int raster, unsigned int format)
{
	fprintf(stderr, "display_presize called (handle 0x%08x)\n", handle);
	fprintf(stderr, "  width %d, height %d, format %d\n", width, height, format);
	return 0;
}

static int display_size(void *handle, void *device, int width, int height, 
	int raster, unsigned int format, unsigned char *pimage)
{
	cv_doc_t *doc = handle;
	
	fprintf(stderr, "display_presize called (handle 0x%08x)\n", handle);
	fprintf(stderr, "  width %d, height %d, format %d\n", width, height, format);
	
	doc->pages = add_page(doc->pages, width, height, format);
	doc->gs_stride = raster;
	doc->gs_image = pimage;
	
	return 0;
}

static int display_sync(void *handle, void *device)
{
	cv_doc_t *doc = handle;
	Rect bounds;
	int err = 0;
	
	fprintf(stderr, "display_sync called (handle 0x%08x)\n", handle);
#if 0	
	err = cv_copy_page(handle);
	
	GetWindowBounds(doc->window, kWindowContentRgn, &bounds);
	InvalWindowRect(doc->window, &bounds);
	
	update_doc_window(doc);
#endif
	return err;
}

static int display_page(void *handle, void *device, int copies, int flush)
{
	cv_doc_t *doc = handle;
	Rect bounds;
	int err;
	
	doc->num_pages++;
	doc->current_page++;
		
	fprintf(stderr, "display_page called (handle 0x%08x) page %d\n",
			handle, doc->current_page);
			
	//display_sync(handle, device);
	err = cv_copy_page(doc);
	
	update_doc_window(doc);
	
	return 0;
}


int main(int argc, char *argv[])
{
	WindowRef window;
	gsapi_t *gs;
	gs_main_instance *this_gs;
	int err;
	
	// initialize stdio window
	SIOUXSettings.standalone = false;
	SIOUXSettings.setupmenus = false;
	SIOUXSettings.initializeTB = false;
	SIOUXSettings.autocloseonquit = true;
	SIOUXSettings.asktosaveonclose = false;
	
	printf("hello, world!\n");

	init();
		
	window = makewindow();

	//draw_hello();
	draw_image(window);
		
	gs = cv_load_gs();
	if (gs == NULL) {
		return 1;
	}
	err = gs->new_instance(&this_gs, NULL);
	if (err < 0) return 1;
	//gs->set_stdio(this_gs, &gscallback_stdin, &gscallback_stdout, &gscallback_stdout);

#if 0	
	{
		int gs_argc = 10;
		char *gs_argv[] = {
			"carbonviewer",
			"-q",
			"-dSAFER",
			"-dBATCH",
			"-dNOPAUSE",
			"-sDEVICE=pnggray",
			"-sOutputFile=out.png",
			"-dGraphicsAlphaBits=4",
			"-dTextAlphaBits=4",
			"tiger.eps"
		};
		
		err = gs->init_with_args(this_gs, gs_argc, gs_argv);
		gs->exit(this_gs);
	}
#endif

    {
    	/* arguments to pass the the gs library */
    	int gs_argc = 11;
    	char *gs_argv[11];
    	char argformat[32];
    	char arghandle[32];
    	cv_doc_t *doc = new_document();
    	/* callback structure for "display" device */
		display_callback display = { 
		    sizeof(display_callback),
		    DISPLAY_VERSION_MAJOR,
		    DISPLAY_VERSION_MINOR,
		    display_open,
		    display_preclose,
		    display_close,
		    display_presize,
		    display_size,
		    display_sync,
		    display_page,
		    NULL,	/* update */
		    NULL,	/* memalloc */
		    NULL	/* memfree */
		};
		gs->set_display_callback(this_gs, &display);
		
		doc->window = window;		
		
    	snprintf(arghandle, 32, "-dDisplayHandle=%d", doc);
    	snprintf(argformat, 32, "-dDisplayFormat=%d", 
    			DISPLAY_COLORS_RGB | DISPLAY_UNUSED_FIRST | DISPLAY_DEPTH_8 |
    			DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST);
		gs_argv[0] = "carbonviewer";
		gs_argv[1] = "-q";
		gs_argv[2] = "-dSAFER";
		gs_argv[3] = "-dBATCH";
		gs_argv[4] = "-dNOPAUSE";
		gs_argv[5] = "-sDEVICE=display";
		gs_argv[6] = arghandle;
		gs_argv[7] = argformat;
		gs_argv[8] = "-dGraphicsAlphaBits=4";
		gs_argv[9] = "-dTextAlphaBits=4";
		gs_argv[10] = "tiger.eps";
    	err = gs->init_with_args(this_gs, gs_argc, gs_argv);
		gs->exit(this_gs);
    }
    
	RunApplicationEventLoop();

	gs->delete_instance(this_gs);
	cv_free_gs(gs);
	return 0;
}
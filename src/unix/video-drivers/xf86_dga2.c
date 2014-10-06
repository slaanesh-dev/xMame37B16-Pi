/*
 *	XFree86 VidMode and DGA support by Jens Vaasjo <jvaasjo@iname.com>
 *      Modified for DGA 2.0 native API support
 *                                      by Shyouzou Sugitani <shy@debian.or.jp>
 *                                         Stea Greene <stea@cs.binghamton.edu>
 */
#ifdef USE_DGA
#define __XF86_DGA_C

#include <sys/types.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/xf86dga.h>
#include <X11/extensions/xf86vmode.h>
#endif
#include "driver.h"
#include "xmame.h"
#include "x11.h"

#ifdef X_XDGASetMode

#ifdef USE_DGA

static void xf86_dga_update_display_8_to_8bpp(struct osd_bitmap *bitmap);
static void xf86_dga_update_display_8_to_16bpp(struct osd_bitmap *bitmap);
static void xf86_dga_update_display_8_to_24bpp(struct osd_bitmap *bitmap);
static void xf86_dga_update_display_8_to_32bpp(struct osd_bitmap *bitmap);
static void xf86_dga_update_display_16_to_16bpp(struct osd_bitmap *bitmap);
static void xf86_dga_update_display_16_to_24bpp(struct osd_bitmap *bitmap);
static void xf86_dga_update_display_16_to_32bpp(struct osd_bitmap *bitmap);
static void xf86_dga_update_display_32_to_32bpp_direct(struct osd_bitmap *bitmap);

static struct
{
	int screen;
	unsigned char *addr;
	int grabbed_keybd;
	int grabbed_mouse;
	int old_grab_mouse;
	char *base_addr;
	int width;
	Colormap cmap;
	void (*xf86_dga_update_display_func)(struct osd_bitmap *bitmap);
	XDGADevice *device;
	XDGAMode *modes;
	int vidmode_changed;
	int palette_dirty;
} xf86ctx = {-1,NULL,FALSE,FALSE,FALSE,NULL,-1,0,NULL,NULL,NULL,FALSE,FALSE};
	
static unsigned char *doublebuffer_buffer = NULL;

static Visual dga_xvisual;

int xf86_dga2_init(void)
{
	int i,j ;
	char *s;
	
	mode_available[X11_DGA] = FALSE;
	xf86ctx.screen          = DefaultScreen(display);
	
	
	if(geteuid())
		fprintf(stderr,"DGA requires root rights\n");
	else if (!(s = getenv("DISPLAY")) || (s[0] != ':'))
		fprintf(stderr,"DGA only works on a local display\n");
	else if(!XDGAQueryVersion(display, &i, &j))
		fprintf(stderr,"XDGAQueryVersion failed\n");
	else if (i < 2)
		fprintf(stderr,"This driver requires DGA 2.0 or newer\n");
	else if(!XDGAQueryExtension(display, &i, &j))
		fprintf(stderr,"XDGAQueryExtension failed\n");
	else if(!XDGAOpenFramebuffer(display,xf86ctx.screen))
		fprintf(stderr,"XDGAOpenFramebuffer failed\n");
	else
		mode_available[X11_DGA] = TRUE; 
		
	if (!mode_available[X11_DGA])
		fprintf(stderr,"Use of DGA-modes is disabled\n");

	return OSD_OK;
}

int xf86_dga2_16bpp_capable(void)
{
	int i, modecount = 0;
	int result = 0;
	XDGAMode *modes;

	modes = XDGAQueryModes(display, xf86ctx.screen, &modecount);
	for(i=0;i<modecount;i++)
        {
		if (modes[i].depth >= 16)
		{
			result = 1;
			break;
		}
	}
	XFree(modes);

	return result;
}

static int xf86_dga_vidmode_find_best_vidmode(int bitmap_depth)
{
	int bestmode = 0;
	int score, best_score = 0;
	int i,modecount = 0;

	xf86ctx.modes = XDGAQueryModes(display, xf86ctx.screen, &modecount);
	fprintf(stderr, "XDGA: info: found %d modes:\n", modecount);

	for(i=0;i<modecount;i++)
	{
		if (mode_disabled(xf86ctx.modes[i].viewportWidth, xf86ctx.modes[i].viewportHeight, bitmap_depth))
			continue;
		if ((Machine->drv->video_attributes & VIDEO_RGB_DIRECT)
			&& bitmap_depth == 32)
		{
			if (xf86ctx.modes[i].bitsPerPixel != 32)
				continue;
		}
		else if (xf86ctx.modes[i].depth < bitmap_depth)
			continue;
#if 0 /* DEBUG */
		fprintf(stderr, "XDGA: info: found mode: %dx%d\n",
		   xf86ctx.modes[i].viewportWidth, xf86ctx.modes[i].viewportHeight);
		fprintf(stderr, "          : image = %dx%d\n",
		   xf86ctx.modes[i].imageWidth, xf86ctx.modes[i].imageHeight);
		fprintf(stderr, "          : bytes/scanline = %d\n",
		   xf86ctx.modes[i].bytesPerScanline);
		fprintf(stderr, "          : bpp = %d, depth = %d\n",
			xf86ctx.modes[i].bitsPerPixel, xf86ctx.modes[i].depth);
		fprintf(stderr, "          : visual class = %s\n",
			xf86ctx.modes[i].visualClass == TrueColor ? "TrueColor":
			xf86ctx.modes[i].visualClass == DirectColor ? "DirectColor" :
			xf86ctx.modes[i].visualClass == PseudoColor ? "PseudoColor" : "Unknown");
#endif
		/* ignore modes with a width which is not 64 bit aligned */
		if(xf86ctx.modes[i].viewportWidth & 7) continue;
		
		score = mode_match(xf86ctx.modes[i].viewportWidth, xf86ctx.modes[i].viewportHeight);
		if (xf86ctx.modes[i].depth != bitmap_depth)
			score -= 10;
		if(score > best_score)
		{
			best_score = score;
			bestmode   = xf86ctx.modes[i].num;
		}
	}

	return bestmode;
}

static int xf86_dga_vidmode_setup_mode_restore(void)
{
	Display *disp;
	int status;
	pid_t pid;

	pid = fork();
	if(pid > 0)
	{
		waitpid(pid,&status,0);
		disp = XOpenDisplay(NULL);
		XDGACloseFramebuffer(disp, xf86ctx.screen);
		XDGASetMode(disp, xf86ctx.screen, 0);
		XCloseDisplay(disp);
		_exit(!WIFEXITED(status));
	}

	if (pid < 0)
	{
		perror("fork");
		return OSD_NOT_OK;
	}

	return OSD_OK;
}

int xf86_dga2_alloc_palette(int writable_colors)
{
	XColor color;
	int i;

	if (xf86ctx.device->mode.depth != 8) {
		xf86ctx.cmap = XDGACreateColormap(display, xf86ctx.screen,
						  xf86ctx.device, AllocNone);
	} else {
		xf86ctx.cmap = XDGACreateColormap(display, xf86ctx.screen,
						  xf86ctx.device, AllocAll);
		for(i=0;i<writable_colors;i++)
		{
			color.pixel = i;
			color.red   = 0;
			color.green = 0;
			color.blue  = 0;
			color.flags = DoRed | DoGreen | DoBlue;

			XStoreColor(display,xf86ctx.cmap,&color);
		}
	}
	XDGAInstallColormap(display,xf86ctx.screen,xf86ctx.cmap);
	return 0;
}

static int xf86_dga_setup_graphics(XDGAMode modeinfo, int bitmap_depth)
{
	int sizeof_pixel;

	if (bitmap_depth == 32)
	{
	    if (depth == 32 
		&& Machine->drv->video_attributes & VIDEO_RGB_DIRECT)
	    {
		xf86ctx.xf86_dga_update_display_func =
			xf86_dga_update_display_32_to_32bpp_direct;
	    }
	}
	else if (bitmap_depth == 16)
	{
	    switch(depth)
	    {
		case 16:
			xf86ctx.xf86_dga_update_display_func =
				xf86_dga_update_display_16_to_16bpp;
			break;
		case 24:
			xf86ctx.xf86_dga_update_display_func =
				xf86_dga_update_display_16_to_24bpp;
			break;
		case 32:
			xf86ctx.xf86_dga_update_display_func =
				xf86_dga_update_display_16_to_32bpp;
			break;
	    }
	}
	else
	{
	    switch(depth)
	    {
		case 8:
			xf86ctx.xf86_dga_update_display_func =
				xf86_dga_update_display_8_to_8bpp;
			break;
		case 16:
			xf86ctx.xf86_dga_update_display_func =
				xf86_dga_update_display_8_to_16bpp;
			break;
		case 24:
			xf86ctx.xf86_dga_update_display_func =
				xf86_dga_update_display_8_to_24bpp;
			break;
		case 32:
			xf86ctx.xf86_dga_update_display_func =
				xf86_dga_update_display_8_to_32bpp;
			break;
	    }
	}
	
	if (xf86ctx.xf86_dga_update_display_func == NULL)
	{
		fprintf(stderr_file, "Error: Unsupported bitmap depth = %dbpp, video depth = %dbpp\n", bitmap_depth, depth);
		return OSD_NOT_OK;
	}
	
	fprintf(stderr_file, "XF86-DGA2 running at: %dbpp\n", depth);
	
	sizeof_pixel  = depth / 8;

	xf86ctx.addr  = (unsigned char*)xf86ctx.base_addr;
	xf86ctx.addr += (((modeinfo.viewportWidth - visual_width*widthscale) / 2) & ~7)
						* sizeof_pixel;
	xf86ctx.addr += ((modeinfo.viewportHeight - visual_height*heightscale) / 2)
				* modeinfo.bytesPerScanline;

	return OSD_OK;
}

/* This name doesn't really cover this function, since it also sets up mouse
   and keyboard. This is done over here, since on most display targets the
   mouse and keyboard can't be setup before the display has. */
int xf86_dga2_create_display(int bitmap_depth)
{
	int bestmode;
	/* only have todo the fork's the first time we go DGA, otherwise people
	   who do a lott of dga <-> window switching will get a lott of
	   children */
	static int first_time  = 1;
	xf86_dga_fix_viewport  = 0;
	xf86_dga_first_click   = 1;
	xf86ctx.palette_dirty  = FALSE;
	xf86ctx.old_grab_mouse = x11_grab_mouse;
	x11_grab_mouse         = FALSE;
	
	window  = RootWindow(display,xf86ctx.screen);
        
	if (first_time)
	{
		if(xf86_dga_vidmode_setup_mode_restore())
			return OSD_NOT_OK;
		first_time = 0;
	}

	bestmode = xf86_dga_vidmode_find_best_vidmode(bitmap_depth);
	if (!bestmode)
	{
		fprintf(stderr_file,"no suitable mode found\n");
		return OSD_NOT_OK;
	}

	xf86ctx.device = XDGASetMode(display,xf86ctx.screen,bestmode);
	if (xf86ctx.device == NULL) {
		fprintf(stderr_file,"XDGASetMode failed\n");
		return OSD_NOT_OK;
	}
	xf86ctx.width = xf86ctx.device->mode.bytesPerScanline * 8
		/ xf86ctx.device->mode.bitsPerPixel;
	xf86ctx.base_addr = xf86ctx.device->data;
	xf86ctx.vidmode_changed = TRUE;

	if (Machine->drv->video_attributes & VIDEO_RGB_DIRECT
		&& bitmap_depth == 32)
		depth = xf86ctx.device->mode.bitsPerPixel;
	else
		depth = xf86ctx.device->mode.depth;

	fprintf(stderr_file,"VidMode Switching To Mode: %d x %d\n",
		xf86ctx.device->mode.viewportWidth,
		xf86ctx.device->mode.viewportHeight);

	xvisual = &dga_xvisual;
	dga_xvisual.class = xf86ctx.device->mode.visualClass;
	dga_xvisual.red_mask = xf86ctx.device->mode.redMask;
	dga_xvisual.green_mask = xf86ctx.device->mode.greenMask;
	dga_xvisual.blue_mask = xf86ctx.device->mode.blueMask;

	/* setup the palette_info struct now we have the depth */
	if (x11_init_palette_info() != OSD_OK)
	    return OSD_NOT_OK;
        
        if (widthscale != 1 || heightscale != 1)
        {
	   doublebuffer_buffer = malloc (visual_width * widthscale * depth / 8);
	   if (doublebuffer_buffer == NULL)
	   {
	      fprintf(stderr, "Error: Couldn't alloc enough memory\n");
	      return OSD_NOT_OK;
	   }
        }

	if(xf86_dga_setup_graphics(xf86ctx.device->mode, bitmap_depth))
		return OSD_NOT_OK;
	
	if(XGrabKeyboard(display,window,True,
		GrabModeAsync,GrabModeAsync,CurrentTime))
	{
		fprintf(stderr_file,"XGrabKeyboard failed\n");
		return OSD_NOT_OK;
	}
	xf86ctx.grabbed_keybd = 1;

	if(use_mouse)
	{
		if(XGrabPointer(display,window,True,
			PointerMotionMask|ButtonPressMask|ButtonReleaseMask,
			GrabModeAsync,GrabModeAsync,None,None,CurrentTime))
		{
			fprintf(stderr_file, "XGrabPointer failed, mouse disabled\n");
			use_mouse = 0;
		}
		else
			xf86ctx.grabbed_mouse = 1;
	}

	XDGASetViewport(display,xf86ctx.screen,0,0,XDGAFlipImmediate);

	memset(xf86ctx.base_addr, 0,
	       xf86ctx.device->mode.bytesPerScanline
	       * xf86ctx.device->mode.imageHeight);

	return OSD_OK;
}


int xf86_dga2_modify_pen(int pen,
	unsigned char red,unsigned char green,unsigned char blue)
{
	XColor color;
	color.pixel = pen;
	color.red   = red   << 8;
	color.green = green << 8;
	color.blue  = blue  << 8;
	color.flags = DoRed | DoGreen | DoBlue;

	XStoreColor(display,xf86ctx.cmap,&color);
	xf86ctx.palette_dirty = TRUE;
	return 0;
}

#define DEST xf86ctx.addr
#define DEST_WIDTH xf86ctx.width
#define SRC_PIXEL unsigned char
/* Use double buffering where it speeds things up */
#define DOUBLEBUFFER

static void xf86_dga_update_display_8_to_8bpp(struct osd_bitmap *bitmap)
{
   if (xf86ctx.palette_dirty)
   {
      XDGAInstallColormap(display,xf86ctx.screen,xf86ctx.cmap);
      xf86ctx.palette_dirty = FALSE;
   }
#define DEST_PIXEL unsigned char
#include "blit.h"
#undef DEST_PIXEL
}

#define INDIRECT current_palette->lookup

static void xf86_dga_update_display_8_to_16bpp(struct osd_bitmap *bitmap)
{
#define DEST_PIXEL unsigned short
#define BLIT_16BPP_HACK
#include "blit.h"
#undef DEST_PIXEL
#undef BLIT_16BPP_HACK
}

#define DEST_PIXEL unsigned int

static void xf86_dga_update_display_8_to_24bpp(struct osd_bitmap *bitmap)
{
#define PACK_BITS
#include "blit.h"
#undef PACK_BITS
}

static void xf86_dga_update_display_8_to_32bpp(struct osd_bitmap *bitmap)
{
#include "blit.h"
}

#undef  DEST_PIXEL

#undef  SRC_PIXEL
#define SRC_PIXEL unsigned short

static void xf86_dga_update_display_16_to_16bpp(struct osd_bitmap *bitmap)
{
#define DEST_PIXEL unsigned short
   if (current_palette->lookup)
   {
#include "blit.h"
   }
   else
   {
#undef  INDIRECT
#include "blit.h"
#define INDIRECT current_palette->lookup
   }
#undef DEST_PIXEL
}

#define DEST_PIXEL unsigned int

static void xf86_dga_update_display_16_to_24bpp(struct osd_bitmap *bitmap)
{
#define PACK_BITS
#include "blit.h"
#undef PACK_BITS
}

static void xf86_dga_update_display_16_to_32bpp(struct osd_bitmap *bitmap)
{
#include "blit.h"
}

#undef  INDIRECT
#undef  SRC_PIXEL
#define SRC_PIXEL unsigned int

static void xf86_dga_update_display_32_to_32bpp_direct(struct osd_bitmap *bitmap)
{
#include "blit.h"
}

#undef DEST_PIXEL

void xf86_dga2_update_display(struct osd_bitmap *bitmap)
{
	int old_use_dirty = use_dirty;
   
	if(xf86_dga_fix_viewport)
	{
		XF86DGASetViewPort(display,xf86ctx.screen,0,0);
		xf86_dga_fix_viewport = 0;
	}

	if (current_palette->lookup_dirty)
		use_dirty = 0;
	
	(*xf86ctx.xf86_dga_update_display_func)(bitmap);
	
	use_dirty = old_use_dirty;
	XDGASync(display,xf86ctx.screen);
}

void xf86_dga2_close_display(void)
{
	XDGASync(display,xf86ctx.screen);
	if(xf86ctx.device)
	{
		XFree(xf86ctx.device);
		xf86ctx.device = 0;
	}
	if(xf86ctx.modes)
	{
		XFree(xf86ctx.modes);
		xf86ctx.modes = 0;
	}
	if(doublebuffer_buffer)
	{
		free(doublebuffer_buffer);
		doublebuffer_buffer = NULL;
	}
	if(xf86ctx.cmap)
	{
		XFreeColormap(display,xf86ctx.cmap);
		xf86ctx.cmap = 0;
	}
	if(xf86ctx.grabbed_mouse)
	{
		XUngrabPointer(display,CurrentTime);
		xf86ctx.grabbed_mouse = FALSE;
	}
	if(xf86ctx.grabbed_keybd)
	{
		XUngrabKeyboard(display,CurrentTime);
		xf86ctx.grabbed_keybd = FALSE;
	}
	if(xf86ctx.vidmode_changed)
	{
		XDGASetMode(display, xf86ctx.screen, 0);
		xf86ctx.vidmode_changed = FALSE;
	}
	x11_grab_mouse = xf86ctx.old_grab_mouse;
}

#endif /*def X_XDGASetMode*/
#endif /*def USE_DGA*/

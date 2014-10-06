/*****************************************************************

  GLmame include file

  Copyright 1998 by Mike Oliphant - oliphant@ling.ed.ac.uk

    http://www.ling.ed.ac.uk/~oliphant/glmame

  Improved by Sven Goethel, http://www.jausoft.com, sgoethel@jausoft.com

  This code may be used and distributed under the terms of the
  Mame license

*****************************************************************/

#ifndef _GLMAME_H
#define _GLMAME_H

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <assert.h>
#include <math.h>
#include "MAME32.h"
#include "wgl_tool.h"
#include "wgl_GDIDisplay.h"
#else
#include <ctype.h>
#include <math.h>
#include <dlfcn.h>
#define CALLBACK
#endif

#include "gltool.h"
#include "gldirty.h"

/* Camera panning stuff */

typedef enum {pan_goto,pan_moveto,pan_repeat,pan_end,pan_nocab} PanType;

struct CameraPan {
  PanType type;      /* Type of pan */
  GLfloat lx,ly,lz;  /* Location of camera */
  GLfloat px,py,pz;  /* Vector to point camera along */
  GLfloat nx,ny,nz;  /* Normal to camera direction */
  int frames;        /* Number of frames for transition */
};

/* video.c */
extern float gamma_correction;

/* xgl.c */
extern char * libGLName;
extern char * libGLUName;

extern GLXContext glContext;
extern int antialias;
extern int fullscreen_width;
extern int fullscreen_height;
extern int winwidth;
extern int winheight;
extern int doublebuffer;
extern int bilinear;
extern int alphablending;
extern int fullscreen;

/* glvec.c */
extern float gl_beam;
extern float gl_translucency;

/* glgen.c */
extern int totalcolors;
extern int use_mod_ctable;
extern GLubyte *ctable;
extern GLushort *rcolmap, *gcolmap, *bcolmap, *acolmap;
extern int ctable_size; /* the true color table size */
extern GLint  gl_internal_format;
extern GLenum gl_bitmap_format;
extern GLenum gl_bitmap_type;
extern unsigned char gl_alpha_value; /* customize it :-) */
extern double scrnaspect, vscrnaspect;
extern GLsizei text_width;
extern GLsizei text_height;
extern int force_text_width_height;
extern int dodepth;
extern int cabview;
extern int cabload_err;
extern int drawbitmap;
extern int dopersist;
extern GLboolean useGLEXT78; /* paletted texture */
extern GLboolean useColorIndex; 
extern GLboolean isGL12;
extern int use_blitter;

extern char *cabname; /* 512 bytes reserved ... */
extern int cabspecified;
extern int gl_is_initialized;
extern GLuint cablist;
extern int gl_is_initialized;

/* xgl.c */
void toggleFullscreen();

/* glvec.c */
extern void set_gl_beam(float new_value);
extern float get_gl_beam();

/* glcab.c */
void InitCabGlobals();

/* glgen.c
 * 
 * the calling order is the listed order:
 * 
 * first the start sequence, then the quit sequence ..
 */

/* start sequence */
void gl_bootstrap_resources();
int sysdep_display_16bpp_capable (void);
void InitVScreen (int depth);
void gl_reset_resources();
int sysdep_display_alloc_palette (int writable_colors);
void InitTextures (void);

/* quit sequence */
void CloseVScreen (void);
void gl_reset_resources();

/* misc sequence */
void  gl_set_bilinear(int new_value);
void gl_init_cabview ();
void  gl_set_cabview(int new_value);
void  gl_set_antialias(int new_value);
void  gl_set_alphablending(int new_value);
void  xgl_fixaspectratio(int *w, int *h);
void xgl_resize(int w, int h, int now);
extern GLboolean glHasEXT78 (void);
extern void glSetUseEXT78 (GLboolean val);
extern GLboolean glGetUseEXT78 (void);

/* glexport */
void gl_save_screen_snapshot();
int gl_png_write_bitmap(void *fp);
void ppm_save_snapshot (void *fp);

#endif /* _GLMAME_H */

#ifndef __OSINLINE__
#define __OSINLINE__

/* for uclock() */
#include "sysdep/misc.h"

#define osd_cycles() uclock()

#if defined svgalib || defined x11 || defined ggi || defined openstep || defined SDL
extern unsigned char *dirty_lines;
extern unsigned char **dirty_blocks;

#define osd_mark_vector_dirty(x,y) \
{ \
   dirty_lines[(y)>>3] = 1; \
   dirty_blocks[(y)>>3][(x)>>3] = 1; \
}

#else
#define osd_mark_vector_dirty(x,y)
#endif

#define mix_sample _mix_sample
#define _mix_sample(dst,src) \
    __asm__ __volatile__ \
	( " mov %2, %2, asr #1 \n" \
	" add %0, %1, %2 , asr #1 \n" \
	: "=r" (dst) \
	: "r"  (src),"r" (dst) \
	)

#endif /* __OSINLINE__ */

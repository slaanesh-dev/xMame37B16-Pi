/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

#define BITMAP_WIDTH	256
static UINT8 *polybitmap1,*polybitmap2;
static UINT8 *polybitmap;

extern UINT8 irvg_clear;
extern UINT8 irobot_bufsel;
extern UINT8 irobot_alphamap;
extern UINT8 *irobot_combase;

static int ir_xmin, ir_ymin, ir_xmax, ir_ymax; /* clipping area */



/***************************************************************************

  Convert the color PROMs into a more useable format.

  5 bits from polygon ram address the palette ram

  Output of color RAM
  bit 8 -- inverter -- 1K ohm resistor  -- RED
  bit 7 -- inverter -- 2.2K ohm resistor  -- RED
        -- inverter -- 1K ohm resistor  -- GREEN
        -- inverter -- 2.2K ohm resistor  -- GREEN
        -- inverter -- 1K ohm resistor  -- BLUE
        -- inverter -- 2.2K ohm resistor  -- BLUE
        -- inverter -- 2.2K ohm resistor  -- INT
        -- inverter -- 4.7K ohm resistor  -- INT
  bit 0 -- inverter -- 9.1K ohm resistor  -- INT

  Alphanumeric colors are generated by ROM .125, it's outputs are connected
  to bits 1..8 as above. The inputs are:

  A0..1 - Character color
  A2    - Character image (1=pixel on/0=off)
  A3..4 - Alphamap 0,1 (appears that only Alphamap1 is used, it is set by
          the processor)

***************************************************************************/
void irobot_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])

	/* the palette will be initialized by the game. We just set it to some */
	/* pre-cooked values so the startup copyright notice can be displayed. */
	for (i = 0;i < 64;i++)
	{
		*(palette++) = ((i & 1) >> 0) * 0xff;
		*(palette++) = ((i & 2) >> 1) * 0xff;
		*(palette++) = ((i & 4) >> 2) * 0xff;
	}

	/* Convert the color prom for the text palette */
	for (i = 0;i < 32;i++)
	{
	    int r,g,b;
		int bits,intensity;
	    unsigned int color;

	    color = *color_prom;
	    intensity = color & 0x03;
	    bits = (color >> 6) & 0x03;
	    r = 28 * bits * intensity;
	    bits = (color >> 4) & 0x03;
	    g = 28 * bits * intensity;
	    bits = (color >> 2) & 0x03;
	    b = 28 * bits * intensity;
		*(palette++) = r;
		*(palette++) = g;
		*(palette++) = b;
		color_prom++;
	}

	/* polygons */
    for (i = 0;i < 64;i++)
         colortable[i] = i;

	/* text */
    for (i = 0;i < TOTAL_COLORS(0);i++)
	{
		COLOR(0,i) = ((i & 0x18) | ((i & 0x01) << 2) | ((i & 0x06) >> 1)) + 64;
	}
}


WRITE_HANDLER( irobot_paletteram_w )
{
    int r,g,b;
	int bits,intensity;
    unsigned int color;

    color = ((data << 1) | (offset & 0x01)) ^ 0x1ff;
    intensity = color & 0x07;
    bits = (color >> 3) & 0x03;
    b = 12 * bits * intensity;
    bits = (color >> 5) & 0x03;
    g = 12 * bits * intensity;
    bits = (color >> 7) & 0x03;
    r = 12 * bits * intensity;
    palette_change_color((offset >> 1) & 0x3F,r,g,b);
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int irobot_vh_start(void)
{
	/* Setup 2 bitmaps for the polygon generator */
	if ((polybitmap1 = malloc(BITMAP_WIDTH * Machine->drv->screen_height)) == 0)
		return 1;
	if ((polybitmap2 = malloc(BITMAP_WIDTH * Machine->drv->screen_height)) == 0)
		return 1;

	/* Set clipping */
	ir_xmin = ir_ymin = 0;
	ir_xmax = Machine->drv->screen_width;
	ir_ymax = Machine->drv->screen_height;

	return 0;
}

/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void irobot_vh_stop(void)
{
	free(polybitmap1);
	free(polybitmap2);
}

/***************************************************************************

	Polygon Generator  (Preliminary information)
	The polygon communication ram works as follows (each location is a 16-bit word):

	0000-xxxx: Object pointer table
		bits 00..10: Address of object data
		bits 12..15: Object type
			0x4 = Polygon
			0x8 = Point
			0xC = Vector
		(0xFFFF means end of table)

	Point Object:
		Word 0, bits 0..15: X Position  (0xFFFF = end of point objects)
		Word 1, bits 7..15: Y Position
		bits 0..5: Color

	Vector Object:
		Word 0, bits 7..15: Ending Y   (0xFFFF = end of line objects)
		Word 1, bits 7..15: Starting Y
				bits 0..5: Color
		Word 2: Slope
		Word 3, bits 0..15: Starting X

	Polygon Object:
		Word 0, bits 0..10: Pointer to second slope list
		Word 1, bits 0..15: Starting X first vector
		Word 2, bits 0..15: Starting X second vector
		Word 3, bits 0..5: Color
		bits 7..15: Initial Y value

	Slope Lists: (one starts at Word 4, other starts at pointer in Word 0)
		Word 0, Slope (0xFFFF = side done)
		Word 1, bits 7..15: Ending Y of vector

	Each side is a continous set of vectors. Both sides are drawn at
	the same time and the space between them is filled in.

***************************************************************************/

void irobot_poly_clear(void)
{
	UINT8 *bitmap_base = irobot_bufsel ? polybitmap2 : polybitmap1;
	memset(bitmap_base, 0, BITMAP_WIDTH * Machine->drv->screen_height);
}

#define draw_pixel(x,y,c)		polybitmap[(y) * BITMAP_WIDTH + (x)] = (c)
#define fill_hline(x1,x2,y,c)	memset(&polybitmap[(y) * BITMAP_WIDTH + (x1)], (c), (x2) - (x1) + 1)


/*
     Line draw routine
     modified from a routine written by Andrew Caldwell
 */

static void draw_line (int x1, int y1, int x2, int y2, int col)
{
    int dx,dy,sx,sy,cx,cy;

    dx = abs(x1-x2);
    dy = abs(y1-y2);
    sx = (x1 <= x2) ? 1: -1;
    sy = (y1 <= y2) ? 1: -1;
    cx = dx/2;
    cy = dy/2;

    if (dx>=dy)
    {
        for (;;)
        {
        	if (x1 >= ir_xmin && x1 < ir_xmax && y1 >= ir_ymin && y1 < ir_ymax)
	             draw_pixel (x1, y1, col);
             if (x1 == x2) break;
             x1 += sx;
             cx -= dy;
             if (cx < 0)
             {
                  y1 += sy;
                  cx += dx;
             }
        }
    }
    else
    {
        for (;;)
        {
        	if (x1 >= ir_xmin && x1 < ir_xmax && y1 >= ir_ymin && y1 < ir_ymax)
	            draw_pixel (x1, y1, col);
            if (y1 == y2) break;
            y1 += sy;
            cy -= dx;
            if (cy < 0)
            {
                 x1 += sx;
                 cy += dy;
             }
        }
    }
}


#define ROUND_TO_PIXEL(x)	((x >> 7) - 128)

void run_video(void)
{
	UINT16 *combase16 = (UINT16 *)irobot_combase;
	int sx,sy,ex,ey,sx2,ey2;
	int color;
	unsigned int d1;
	int lpnt,spnt,spnt2;
	int shp;
	INT32 word1,word2;

	logerror("Starting Polygon Generator, Clear=%d\n",irvg_clear);

	if (irobot_bufsel)
		polybitmap = polybitmap2;
	else
		polybitmap = polybitmap1;

	lpnt=0;
	while (lpnt < 0x7FF)
	{
		d1 = combase16[lpnt++];
		if (d1 == 0xFFFF) break;
		spnt = d1 & 0x07FF;
		shp = (d1 & 0xF000) >> 12;

		/* Pixel */
		if (shp == 0x8)
		{
			while (spnt < 0x7FF)
			{
				sx = combase16[spnt];
				if (sx == 0xFFFF) break;
				sy = combase16[spnt+1];
				color = sy & 0x3F;
				sx = ROUND_TO_PIXEL(sx);
				sy = ROUND_TO_PIXEL(sy);
	        	if (sx >= ir_xmin && sx < ir_xmax && sy >= ir_ymin && sy < ir_ymax)
					draw_pixel(sx,sy,color);
				spnt+=2;
			}/*while object */
		}/*if point */

		/* Line */
		if (shp == 0xC)
		{
			while (spnt < 0x7FF)
			{
				ey = combase16[spnt];
				if (ey == 0xFFFF) break;
				ey = ROUND_TO_PIXEL(ey);
				sy = combase16[spnt+1];
				color = sy & 0x3F;
				sy = ROUND_TO_PIXEL(sy);
				sx = combase16[spnt+3];
				word1 = (INT16)combase16[spnt+2];
				ex = sx + word1 * (ey - sy + 1);
				draw_line(ROUND_TO_PIXEL(sx),sy,ROUND_TO_PIXEL(ex),ey,color);
				spnt+=4;
			}/*while object */
		}/*if line */

		/* Polygon */
		if (shp == 0x4)
		{
			spnt2 = combase16[spnt] & 0x7FF;

			sx = combase16[spnt+1];
			sx2 = combase16[spnt+2];
			sy = combase16[spnt+3];
			color = sy & 0x3F;
			sy = ROUND_TO_PIXEL(sy);
			spnt+=4;

			word1 = (INT16)combase16[spnt];
			ey = combase16[spnt+1];
			if (word1 != -1 || ey != 0xFFFF)
			{
				ey = ROUND_TO_PIXEL(ey);
				spnt+=2;

			/*	sx += word1; */

				word2 = (INT16)combase16[spnt2];
				ey2 = ROUND_TO_PIXEL(combase16[spnt2+1]);
				spnt2+=2;

			/*	sx2 += word2; */

				while(1)
				{
					if (sy >= ir_ymin && sy < ir_ymax)
					{
						int x1 = ROUND_TO_PIXEL(sx);
						int x2 = ROUND_TO_PIXEL(sx2);
						int temp;

						if (x1 > x2) temp = x1, x1 = x2, x2 = temp;
						if (x1 < ir_xmin) x1 = ir_xmin;
						if (x2 >= ir_xmax) x2 = ir_xmax - 1;
						if (x1 < x2)
							fill_hline(x1 + 1, x2, sy, color);
					}
					sy++;

					if (sy > ey)
					{
						word1 = (INT16)combase16[spnt];
						ey = combase16[spnt+1];
						if (word1 == -1 && ey == 0xFFFF)
							break;
						ey = ROUND_TO_PIXEL(ey);
						spnt+=2;
					}
					else
						sx += word1;

					if (sy > ey2)
					{
						word2 = (INT16)combase16[spnt2];
						ey2 = ROUND_TO_PIXEL(combase16[spnt2+1]);
						spnt2+=2;
					}
					else
						sx2 += word2;

				} /*while polygon */
			}/*if at least 2 sides */
		} /*if polygon */
	} /*while object */
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void irobot_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	UINT8 *bitmap_base = irobot_bufsel ? polybitmap1 : polybitmap2;
	int x, y, offs;

	logerror("Screen Refresh\n");

	palette_recalc();

	/* copy the polygon bitmap */
	for (y = Machine->visible_area.min_y; y < Machine->visible_area.max_y; y++)
		draw_scanline8(bitmap, 0, y, BITMAP_WIDTH, &bitmap_base[y * BITMAP_WIDTH], Machine->pens, -1);

	/* redraw the non-zero characters in the alpha layer */
	for (y = offs = 0; y < 32; y++)
		for (x = 0; x < 32; x++, offs++)
			if (videoram[offs] != 0)
			{
				int code = videoram[offs] & 0x3f;
				int color = ((videoram[offs] & 0xC0) >> 6) | (irobot_alphamap >> 3);
				int transp=color + 64;

				drawgfx(bitmap,Machine->gfx[0],
						code, color,
						0,0,
						8*x,8*y,
						&Machine->visible_area,TRANSPARENCY_COLOR,transp);
			}
}

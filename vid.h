/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// vid.h -- video driver defs

#define VID_CBITS	6
#define VID_GRADES	(1 << VID_CBITS)

// a pixel can be one, two, or four bytes
using pixel_t = byte ;

struct vrect_t
{
	int				x,y,width,height;
	vrect_t	*pnext;
} ;

struct viddef_t
{
	pixel_t			*buffer;		// invisible buffer
	pixel_t			*colormap;		// 256 * VID_GRADES size
	unsigned short	*colormap16;	// 256 * VID_GRADES size
	int				fullbright;		// index of first fullbright color
	unsigned		rowbytes;	// may be > width if displayed in a window
	unsigned		width;
	unsigned		height;
	float			aspect;		// width / height -- < 0 is taller than wide
	int				numpages;
	int				recalc_refdef;	// if true, recalc vid-based stuff
	pixel_t			*conbuffer;
	int				conrowbytes;
	unsigned		conwidth;
	unsigned		conheight;
	int				maxwarpwidth;
	int				maxwarpheight;
	pixel_t			*direct;		// direct drawing to framebuffer, if not
									//  nullptr
};

extern	viddef_t	vid;				// global video state
//extern	unsigned short	d_8to16table[256]; //johnfitz -- never used
extern void (*vid_menudrawfn)(void);
extern void (*vid_menukeyfn)(int key);
extern void (*vid_menucmdfn)(void); //johnfitz

//johnfitz -- deleted VID_SetPalette and VID_ShiftPalette

void	VID_Init(); //johnfitz -- removed palette from argument list

void	VID_Shutdown();
// Called at shutdown

void	VID_Update (vrect_t *rects);
// flushes the given rectangles from the view buffer to the screen

int VID_SetMode (int modenum); //johnfitz -- removed palette from argument list
// sets the mode; only used by the Quake engine for resetting to mode 0 (the
// base mode) on memory allocation failures

void VID_HandlePause (bool pause);
// called only on Win32, when pause happens, so the mouse can be released

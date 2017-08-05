#include "quakedef.h"

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net
turn off messages option

the refresh is allways rendered, unless the console is full screen


console is:
	notify lines
	half
	full


*/


int glx, gly, glwidth, glheight;

float scr_con_current;
float scr_conlines; // lines of console to display

float oldscreensize, oldfov, oldsbarscale, oldsbaralpha; //johnfitz -- added oldsbarscale and oldsbaralpha

//johnfitz -- new cvars
cvar_t scr_menuscale = {"scr_menuscale", "1", true};
cvar_t scr_sbarscale = {"scr_sbarscale", "1", true};
cvar_t scr_sbaralpha = {"scr_sbaralpha", "1", true};
cvar_t scr_conwidth = {"scr_conwidth", "0", true};
cvar_t scr_conscale = {"scr_conscale", "1", true};
cvar_t scr_crosshaircale = {"scr_crosshaircale", "1", true};
cvar_t scr_showfps = {"scr_showfps", "0"};
cvar_t scr_clock = {"scr_clock", "0"};
//johnfitz

cvar_t scr_viewsize = {"viewsize","100", true};
cvar_t scr_fov = {"fov","90"}; // 10 - 170
cvar_t scr_conspeed = {"scr_conspeed","300"};
cvar_t scr_centertime = {"scr_centertime","2"};
cvar_t scr_showram = {"showram","1"};
cvar_t scr_showturtle = {"showturtle","0"};
cvar_t scr_showpause = {"showpause","1"};
cvar_t scr_printspeed = {"scr_printspeed","8"};
cvar_t gl_triplebuffer = {"gl_triplebuffer", "1", true};

extern cvar_t crosshair;

bool scr_initialized; // ready to draw

qpic_t* scr_ram;
qpic_t* scr_net;
qpic_t* scr_turtle;

int clearconsole;
int clearnotify;

int sb_lines;

viddef_t vid; // global video state

vrect_t scr_vrect;

bool scr_disabled_for_loading;
bool scr_drawloading;
float scr_disabled_time;

bool block_drawing;

int scr_tileclear_updates = 0; //johnfitz

void SCR_ScreenShot_f();

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char scr_centerstring[1024];
float scr_centertime_start; // for slow victory printing
float scr_centertime_off;
int scr_center_lines;
int scr_erase_lines;
int scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint(char* str) //update centerprint data
{
	strncpy(scr_centerstring, str, sizeof scr_centerstring - 1);
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void SCR_DrawCenterString() //actually do the drawing
{
	int l;
	int y;
	int remaining;

	GL_SetCanvas(canvastype::CANVAS_MENU); //johnfitz

	// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	auto start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = 200 * 0.35; //johnfitz -- 320x200 coordinate system
	else
		y = 48;

	do
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		auto x = (320 - l * 8) / 2; //johnfitz -- 320x200 coordinate system
		for (auto j = 0; j < l; j++ , x += 8)
		{
			Draw_Character(x, y, start[j]); //johnfitz -- stretch overlays
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++; // skip the \n
	}
	while (true);
}

void SCR_CheckDrawCenterString()
{
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != keydest_t::key_game)
		return;
	if (cl.paused) //johnfitz -- don't show centerprint during a pause
		return;

	SCR_DrawCenterString();
}

//=============================================================================

/*
====================
CalcFovy
====================
*/
float CalcFovy(float fov_x, float width, float height)
{
	if (fov_x < 1 || fov_x > 179)
		Sys_Error("Bad fov: %f", fov_x);

	float x = width / tan(fov_x / 360 * M_PI);
	auto a = atan(height / x);
	a = a * 360 / M_PI;
	return a;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef()
{
	float size; //johnfitz -- scale

	vid.recalc_refdef = 0;

	// force the status bar to redraw
	Sbar_Changed();

	scr_tileclear_updates = 0; //johnfitz

	// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set("viewsize", "30");
	if (scr_viewsize.value > 120)
		Cvar_Set("viewsize", "120");

	// bound fov
	if (scr_fov.value < 10)
		Cvar_Set("fov", "10");
	if (scr_fov.value > 170)
		Cvar_Set("fov", "170");

	//johnfitz -- rewrote this section
	size = scr_viewsize.value;
	float scale = CLAMP (1.0, scr_sbarscale.value, (float)glwidth / 320.0);

	if (size >= 120 || cl.intermission || scr_sbaralpha.value < 1) //johnfitz -- scr_sbaralpha.value
		sb_lines = 0;
	else if (size >= 110)
		sb_lines = 24 * scale;
	else
		sb_lines = 48 * scale;

	size = min(scr_viewsize.value, 100) / 100;
	//johnfitz

	//johnfitz -- rewrote this section
	r_refdef.vrect.width = max(glwidth * size, 96); //no smaller than 96, for icons
	r_refdef.vrect.height = min(glheight * size, glheight - sb_lines); //make room for sbar
	r_refdef.vrect.x = (glwidth - r_refdef.vrect.width) / 2;
	r_refdef.vrect.y = (glheight - sb_lines - r_refdef.vrect.height) / 2;
	//johnfitz

	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFovy(r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	scr_vrect = r_refdef.vrect;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f()
{
	Cvar_SetValue("viewsize", scr_viewsize.value + 10);
	vid.recalc_refdef = 1;
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f()
{
	Cvar_SetValue("viewsize", scr_viewsize.value - 10);
	vid.recalc_refdef = 1;
}

/*
==================
SCR_Conwidth_f -- johnfitz -- called when scr_conwidth or scr_conscale changes
==================
*/
void SCR_Conwidth_f()
{
	vid.conwidth = scr_conwidth.value > 0 ? static_cast<int>(scr_conwidth.value) : scr_conscale.value > 0 ? static_cast<int>(vid.width / scr_conscale.value) : vid.width;
	vid.conwidth = CLAMP (320, vid.conwidth, vid.width);
	vid.conwidth &= 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
}

//============================================================================

/*
==================
SCR_LoadPics -- johnfitz
==================
*/
void SCR_LoadPics()
{
	scr_ram = Draw_PicFromWad("ram");
	scr_net = Draw_PicFromWad("net");
	scr_turtle = Draw_PicFromWad("turtle");
}

/*
==================
SCR_Init
==================
*/
void SCR_Init()
{
	//johnfitz -- new cvars
	Cvar_RegisterVariable(&scr_menuscale, nullptr);
	Cvar_RegisterVariable(&scr_sbarscale, nullptr);
	Cvar_RegisterVariable(&scr_sbaralpha, nullptr);
	Cvar_RegisterVariable(&scr_conwidth, &SCR_Conwidth_f);
	Cvar_RegisterVariable(&scr_conscale, &SCR_Conwidth_f);
	Cvar_RegisterVariable(&scr_crosshaircale, nullptr);
	Cvar_RegisterVariable(&scr_showfps, nullptr);
	Cvar_RegisterVariable(&scr_clock, nullptr);
	//johnfitz

	Cvar_RegisterVariable(&scr_fov, nullptr);
	Cvar_RegisterVariable(&scr_viewsize, nullptr);
	Cvar_RegisterVariable(&scr_conspeed, nullptr);
	Cvar_RegisterVariable(&scr_showram, nullptr);
	Cvar_RegisterVariable(&scr_showturtle, nullptr);
	Cvar_RegisterVariable(&scr_showpause, nullptr);
	Cvar_RegisterVariable(&scr_centertime, nullptr);
	Cvar_RegisterVariable(&scr_printspeed, nullptr);
	Cvar_RegisterVariable(&gl_triplebuffer, nullptr);

	Cmd_AddCommand("screenshot", SCR_ScreenShot_f);
	Cmd_AddCommand("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand("sizedown", SCR_SizeDown_f);

	SCR_LoadPics(); //johnfitz

	scr_initialized = true;
}

//============================================================================

/*
==============
SCR_DrawFPS -- johnfitz
==============
*/
void SCR_DrawFPS()
{
	static double oldtime = 0, fps = 0;
	static auto oldframecount = 0;
	char str[12];

	auto time = realtime - oldtime;
	auto frames = r_framecount - oldframecount;

	if (time < 0 || frames < 0)
	{
		oldtime = realtime;
		oldframecount = r_framecount;
		return;
	}

	if (time > 0.75) //update value every 3/4 second
	{
		fps = frames / time;
		oldtime = realtime;
		oldframecount = r_framecount;
	}

	if (scr_showfps.value) //draw it
	{
		sprintf(str, "%4.0f fps", fps);
		int x = 320 - (strlen(str) << 3);
		auto y = 200 - 8;
		if (scr_clock.value) y -= 8; //make room for clock
		GL_SetCanvas(canvastype::CANVAS_BOTTOMRIGHT);
		Draw_String(x, y, str);
		scr_tileclear_updates = 0;
	}
}

/*
==============
SCR_DrawClock -- johnfitz
==============
*/
void SCR_DrawClock()
{
	char str[9];

	if (scr_clock.value == 1)
	{
		int minutes = cl.time / 60;
		auto seconds = static_cast<int>(cl.time) % 60;

		sprintf(str, "%i:%i%i", minutes, seconds / 10, seconds % 10);
	}
#ifdef _WIN32
	else if (scr_clock.value == 2)
	{
		SYSTEMTIME systime;
		char m[3] = "AM";

		GetLocalTime(&systime);
		int hours = systime.wHour;
		int minutes = systime.wMinute;
		int seconds = systime.wSecond;

		if (hours > 12)
			strcpy(m, "PM");
		hours = hours % 12;
		if (hours == 0)
			hours = 12;

		sprintf(str, "%i:%i%i:%i%i %s", hours, minutes / 10, minutes % 10, seconds / 10, seconds % 10, m);
	}
	else if (scr_clock.value == 3)
	{
		SYSTEMTIME systime;

		GetLocalTime(&systime);
		int hours = systime.wHour;
		int minutes = systime.wMinute;
		int seconds = systime.wSecond;

		sprintf(str, "%i:%i%i:%i%i", hours % 12, minutes / 10, minutes % 10, seconds / 10, seconds % 10);
	}
#endif
	else
		return;

	//draw it
	GL_SetCanvas(canvastype::CANVAS_BOTTOMRIGHT);
	Draw_String(320 - (strlen(str) << 3), 200 - 8, str);

	scr_tileclear_updates = 0;
}

/*
==============
SCR_DrawDevStats
==============
*/
void SCR_DrawDevStats()
{
	char str[40];
	auto y = 25 - 9; //9=number of lines to print
	auto x = 0; //margin

	if (!devstats.value)
		return;

	GL_SetCanvas(canvastype::CANVAS_BOTTOMLEFT);

	Draw_Fill(x, y * 8, 19 * 8, 9 * 8, 0, 0.5); //dark rectangle

	sprintf(str, "devstats |Curr Peak");
	Draw_String(x, y++ * 8 - x, str);

	sprintf(str, "---------+---------");
	Draw_String(x, y++ * 8 - x, str);

	sprintf(str, "Edicts   |%4i %4i", dev_stats.edicts, dev_peakstats.edicts);
	Draw_String(x, y++ * 8 - x, str);

	sprintf(str, "Packet   |%4i %4i", dev_stats.packetsize, dev_peakstats.packetsize);
	Draw_String(x, y++ * 8 - x, str);

	sprintf(str, "Visedicts|%4i %4i", dev_stats.visedicts, dev_peakstats.visedicts);
	Draw_String(x, y++ * 8 - x, str);

	sprintf(str, "Efrags   |%4i %4i", dev_stats.efrags, dev_peakstats.efrags);
	Draw_String(x, y++ * 8 - x, str);

	sprintf(str, "Dlights  |%4i %4i", dev_stats.dlights, dev_peakstats.dlights);
	Draw_String(x, y++ * 8 - x, str);

	sprintf(str, "Beams    |%4i %4i", dev_stats.beams, dev_peakstats.beams);
	Draw_String(x, y++ * 8 - x, str);

	sprintf(str, "Tempents |%4i %4i", dev_stats.tempents, dev_peakstats.tempents);
	Draw_String(x, y * 8 - x, str);
}

/*
==============
SCR_DrawRam
==============
*/
void SCR_DrawRam()
{
	if (!scr_showram.value)
		return;

	if (!r_cache_thrash)
		return;

	GL_SetCanvas(canvastype::CANVAS_DEFAULT); //johnfitz

	Draw_Pic(scr_vrect.x + 32, scr_vrect.y, scr_ram);
}

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle()
{
	static int count;

	if (!scr_showturtle.value)
		return;

	if (host_frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	GL_SetCanvas(canvastype::CANVAS_DEFAULT); //johnfitz

	Draw_Pic(scr_vrect.x, scr_vrect.y, scr_turtle);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet()
{
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	GL_SetCanvas(canvastype::CANVAS_DEFAULT); //johnfitz

	Draw_Pic(scr_vrect.x + 64, scr_vrect.y, scr_net);
}

/*
==============
DrawPause
==============
*/
void SCR_DrawPause()
{
	if (!cl.paused)
		return;

	if (!scr_showpause.value) // turn off for screenshots
		return;

	GL_SetCanvas(canvastype::CANVAS_MENU); //johnfitz

	auto pic = Draw_CachePic("gfx/pause.lmp");
	Draw_Pic((320 - pic->width) / 2, (240 - 48 - pic->height) / 2, pic); //johnfitz -- stretched menus

	scr_tileclear_updates = 0; //johnfitz
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading()
{
	if (!scr_drawloading)
		return;

	GL_SetCanvas(canvastype::CANVAS_MENU); //johnfitz

	auto pic = Draw_CachePic("gfx/loading.lmp");
	Draw_Pic((320 - pic->width) / 2, (240 - 48 - pic->height) / 2, pic); //johnfitz -- stretched menus

	scr_tileclear_updates = 0; //johnfitz
}

/*
==============
SCR_DrawCrosshair -- johnfitz
==============
*/
void SCR_DrawCrosshair()
{
	if (!crosshair.value)
		return;

	GL_SetCanvas(canvastype::CANVAS_CROSSHAIR);
	Draw_Character(-4, -4, '+'); //0,0 is center of viewport
}


//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole()
{
	//johnfitz -- let's hack away the problem of slow console when host_timescale is <0
	extern cvar_t host_timescale;
	//johnfitz

	Con_CheckResize();

	if (scr_drawloading)
		return; // never a console with loading plaque

	// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = glheight; //full screen //johnfitz -- glheight instead of vid.height
		scr_con_current = scr_conlines;
	}
	else if (key_dest == keydest_t::key_console)
		scr_conlines = glheight / 2; //half screen //johnfitz -- glheight instead of vid.height
	else
		scr_conlines = 0; //none visible

	auto timescale = host_timescale.value > 0 ? host_timescale.value : 1; //johnfitz -- timescale

	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.value * host_frametime / timescale; //johnfitz -- timescale
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;
	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.value * host_frametime / timescale; //johnfitz -- timescale
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages)
		Sbar_Changed();

	if (!con_forcedup && scr_con_current)
		scr_tileclear_updates = 0; //johnfitz
}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole()
{
	if (scr_con_current)
	{
		Con_DrawConsole(scr_con_current, true);
		clearconsole = 0;
	}
	else
	{
		if (key_dest == keydest_t::key_game || key_dest == keydest_t::key_message)
			Con_DrawNotify(); // only draw notify in game
	}
}


/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

/*
==================
SCR_ScreenShot_f -- johnfitz -- rewritten to use Image_WriteTGA
==================
*/
void SCR_ScreenShot_f()
{
	char tganame[16]; //johnfitz -- was [80]
	char checkname[MAX_OSPATH];
	int i;

	// find a file name to save it to
	for (i = 0; i < 10000; i++)
	{
		sprintf(tganame, "fitz%04i.tga", i);
		sprintf(checkname, "%s/%s", com_gamedir, tganame);
		if (Sys_FileTime(checkname) == -1)
			break; // file doesn't exist
	}
	if (i == 10000)
	{
		Con_Printf("SCR_ScreenShot_f: Couldn't find an unused filename\n");
		return;
	}

	//get data
	auto buffer = static_cast<byte*>(malloc(glwidth * glheight * 3));
	glReadPixels(glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer);

	// now write the file
	if (Image_WriteTGA(tganame, buffer, glwidth, glheight, 24, false))
		Con_Printf("Wrote %s\n", tganame);
	else
		Con_Printf("SCR_ScreenShot_f: Couldn't create a TGA file\n");

	free(buffer);
}


//=============================================================================


/*
===============
SCR_BeginLoadingPlaque

================
*/
void SCR_BeginLoadingPlaque()
{
	S_StopAllSounds(true);

	if (cls.state != cactive_t::ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;

	// redraw with no console and the loading plaque
	Con_ClearNotify();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	Sbar_Changed();
	SCR_UpdateScreen();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
}

/*
===============
SCR_EndLoadingPlaque

================
*/
void SCR_EndLoadingPlaque()
{
	scr_disabled_for_loading = false;
	Con_ClearNotify();
}

//=============================================================================

char* scr_notifystring;
bool scr_drawdialog;

void SCR_DrawNotifyString()
{
	int l;

	GL_SetCanvas(canvastype::CANVAS_MENU); //johnfitz

	auto start = scr_notifystring;

	int y = 200 * 0.35; //johnfitz -- stretched overlays

	do
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		auto x = (320 - l * 8) / 2; //johnfitz -- stretched overlays
		for (int j = 0; j < l; j++ , x += 8)
			Draw_Character(x, y, start[j]);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++; // skip the \n
	}
	while (true);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.
==================
*/
int SCR_ModalMessage(char* text, float timeout) //johnfitz -- timeout
{
	if (cls.state == cactive_t::ca_dedicated)
		return true;

	scr_notifystring = text;

	// draw a fresh screen
	scr_drawdialog = true;
	SCR_UpdateScreen();
	scr_drawdialog = false;

	S_ClearBuffer(); // so dma doesn't loop current sound

	auto time1 = Sys_FloatTime() + timeout; //johnfitz -- timeout
	double time2 = 0.0f; //johnfitz -- timeout

	do
	{
		key_count = -1; // wait for a key down and up
		Sys_SendKeyEvents();
		if (timeout) time2 = Sys_FloatTime(); //johnfitz -- zero timeout means wait forever.
	}
	while (key_lastpress != 'y' &&
		key_lastpress != 'n' &&
		key_lastpress != K_ESCAPE &&
		time2 <= time1);

	//	SCR_UpdateScreen (); //johnfitz -- commented out

	//johnfitz -- timeout
	if (time2 > time1)
		return false;
	//johnfitz

	return key_lastpress == 'y';
}


//=============================================================================

//johnfitz -- deleted SCR_BringDownConsole


/*
==================
SCR_TileClear -- johnfitz -- modified to use glwidth/glheight instead of vid.width/vid.height
                             also fixed the dimentions of right and top panels
							 also added scr_tileclear_updates
==================
*/
void SCR_TileClear()
{
	if (scr_tileclear_updates >= vid.numpages && !gl_clear.value && !isIntelVideo) //intel video workarounds from Baker
		return;
	scr_tileclear_updates++;

	if (r_refdef.vrect.x > 0)
	{
		// left
		Draw_TileClear(0,
		               0,
		               r_refdef.vrect.x,
		               glheight - sb_lines);
		// right
		Draw_TileClear(r_refdef.vrect.x + r_refdef.vrect.width,
		               0,
		               glwidth - r_refdef.vrect.x - r_refdef.vrect.width,
		               glheight - sb_lines);
	}

	if (r_refdef.vrect.y > 0)
	{
		// top
		Draw_TileClear(r_refdef.vrect.x,
		               0,
		               r_refdef.vrect.width,
		               r_refdef.vrect.y);
		// bottom
		Draw_TileClear(r_refdef.vrect.x,
		               r_refdef.vrect.y + r_refdef.vrect.height,
		               r_refdef.vrect.width,
		               glheight - r_refdef.vrect.y - r_refdef.vrect.height - sb_lines);
	}
}

void GL_Set2D();
void V_UpdateBlend();

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void SCR_UpdateScreen()
{
	if (block_drawing)
		return;

	vid.numpages = 2 + (gl_triplebuffer.value ? 1 : 0); //johnfitz -- in case gl_triplebuffer is not 0 or 1

	if (scr_disabled_for_loading)
	{
		if (realtime - scr_disabled_time > 60)
		{
			scr_disabled_for_loading = false;
			Con_Printf("load failed.\n");
		}
		else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return; // not initialized yet


	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);

	//
	// determine size of refresh window
	//

	if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

	if (oldscreensize != scr_viewsize.value)
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}

	//johnfitz -- added oldsbarscale and oldsbaralpha
	if (oldsbarscale != scr_sbarscale.value)
	{
		oldsbarscale = scr_sbarscale.value;
		vid.recalc_refdef = true;
	}

	if (oldsbaralpha != scr_sbaralpha.value)
	{
		oldsbaralpha = scr_sbaralpha.value;
		vid.recalc_refdef = true;
	}
	//johnfitz

	if (vid.recalc_refdef)
		SCR_CalcRefdef();

	//
	// do 3D refresh drawing, and then update the screen
	//
	SCR_SetUpToDrawConsole();

	V_RenderView();

	GL_Set2D();

	//FIXME: only call this when needed
	SCR_TileClear();

	if (scr_drawdialog) //new game confirm
	{
		Sbar_Draw();
		Draw_FadeScreen();
		SCR_DrawNotifyString();
	}
	else if (scr_drawloading) //loading
	{
		SCR_DrawLoading();
		Sbar_Draw();
	}
	else if (cl.intermission == 1 && key_dest == keydest_t::key_game) //end of level
	{
		Sbar_IntermissionOverlay();
	}
	else if (cl.intermission == 2 && key_dest == keydest_t::key_game) //end of episode
	{
		Sbar_FinaleOverlay();
		SCR_CheckDrawCenterString();
	}
	else
	{
		SCR_DrawCrosshair(); //johnfitz
		SCR_DrawRam();
		SCR_DrawNet();
		SCR_DrawTurtle();
		SCR_DrawPause();
		SCR_CheckDrawCenterString();
		Sbar_Draw();
		SCR_DrawDevStats(); //johnfitz
		SCR_DrawFPS(); //johnfitz
		SCR_DrawClock(); //johnfitz
		SCR_DrawConsole();
		M_Draw();
	}

	V_UpdateBlend(); //johnfitz -- V_UpdatePalette cleaned up and renamed

	GL_EndRendering();
}

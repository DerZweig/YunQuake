#pragma once
void SCR_Init();

void SCR_UpdateScreen();


void SCR_SizeUp();
void SCR_SizeDown();
void SCR_BringDownConsole();
void SCR_CenterPrint(char* str);

void SCR_BeginLoadingPlaque();
void SCR_EndLoadingPlaque();

int SCR_ModalMessage(char* text, float timeout); //johnfitz -- added timeout

extern float scr_con_current;
extern float scr_conlines; // lines of console to display

extern int sb_lines;

extern int clearnotify; // set to 0 whenever notify text is drawn
extern bool scr_disabled_for_loading;
extern bool scr_skipupdate;

extern cvar_t scr_viewsize;

extern cvar_t scr_sbaralpha; //johnfitz

extern bool block_drawing;

void SCR_UpdateWholeScreen();

//johnfitz -- stuff for 2d drawing control
enum class canvastype
{
	CANVAS_NONE,
	CANVAS_DEFAULT,
	CANVAS_CONSOLE,
	CANVAS_MENU,
	CANVAS_SBAR,
	CANVAS_WARPIMAGE,
	CANVAS_CROSSHAIR,
	CANVAS_BOTTOMLEFT,
	CANVAS_BOTTOMRIGHT,
	CANVAS_TOPRIGHT,
};

extern cvar_t scr_menuscale;
extern cvar_t scr_sbarscale;
extern cvar_t scr_conwidth;
extern cvar_t scr_conscale;
extern cvar_t scr_crosshaircale;
//johnfitz

extern int scr_tileclear_updates; //johnfitz

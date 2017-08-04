#pragma once
extern int con_totallines;
extern int con_backscroll;
extern bool con_forcedup; // because no entities to refresh
extern bool con_initialized;
extern byte* con_chars;

extern char con_lastcenterstring[]; //johnfitz

void Con_DrawCharacter(int cx, int line, int num) = delete;

void Con_CheckResize();
void Con_Init();
void Con_DrawConsole(int lines, bool drawinput);
void Con_Print(char* txt);
void Con_Printf(char* fmt, ...);
void Con_Warning(char* fmt, ...); //johnfitz
void Con_DPrintf(char* fmt, ...);
void Con_DPrintf2(char* fmt, ...); //johnfitz
void Con_SafePrintf(char* fmt, ...);
void Con_Clear_f();
void Con_DrawNotify();
void Con_ClearNotify();
void Con_ToggleConsole_f();

void Con_NotifyBox(char* text); // during startup for sound / cd warnings

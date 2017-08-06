#pragma once
struct cvar_t
{
	char* name;
	char* string;
	qboolean archive; // set to qtrue to cause it to be saved to vars.rc
	qboolean server; // notifies players when changed
	float value;
	cvar_t* next;
};

void Cvar_RegisterVariable(cvar_t* variable);
// registers a cvar that allready has the name, string, and optionally the
// archive elements set.

void Cvar_Set(char* var_name, char* value);
// equivelant to "<name> <variable>" typed at the console

void Cvar_SetValue(char* var_name, float value);
// expands value to a string and calls Cvar_Set

float Cvar_VariableValue(char* var_name);
// returns 0 if not defined or non numeric

char* Cvar_VariableString(char* var_name);
// returns an empty string if not defined

char* Cvar_CompleteVariable(char* partial);
// attempts to match a partial variable name for command line completion
// returns nullptr if nothing fits

qboolean Cvar_Command(void);
// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns qtrue if the command was a variable reference that
// was handled. (print or change)

void Cvar_WriteVariables(FILE* f);
// Writes lines containing "set variable value" for all variables
// with the archive flag set to qtrue.

cvar_t* Cvar_FindVar(char* var_name);

//
// cvars
//
extern cvar_t cl_name;
extern cvar_t cl_color;
extern cvar_t cl_upspeed;
extern cvar_t cl_forwardspeed;
extern cvar_t cl_backspeed;
extern cvar_t cl_sidespeed;
extern cvar_t cl_movespeedkey;
extern cvar_t cl_yawspeed;
extern cvar_t cl_pitchspeed;
extern cvar_t cl_anglespeedkey;
extern cvar_t cl_autofire;
extern cvar_t cl_shownet;
extern cvar_t cl_nolerp;
extern cvar_t cl_pitchdriftspeed;
extern cvar_t lookspring;
extern cvar_t lookstrafe;
extern cvar_t sensitivity;

extern cvar_t m_pitch;
extern cvar_t m_yaw;
extern cvar_t m_forward;
extern cvar_t m_side;
extern cvar_t r_norefresh;
extern cvar_t r_drawentities;
extern cvar_t r_drawworld;
extern cvar_t r_drawviewmodel;
extern cvar_t r_speeds;
extern cvar_t r_waterwarp;
extern cvar_t r_fullbright;
extern cvar_t r_lightmap;
extern cvar_t r_shadows;
extern cvar_t r_mirroralpha;
extern cvar_t r_wateralpha;
extern cvar_t r_dynamic;
extern cvar_t r_novis;

extern cvar_t gl_clear;
extern cvar_t gl_cull;
extern cvar_t gl_poly;
extern cvar_t gl_texsort;
extern cvar_t gl_smoothmodels;
extern cvar_t gl_affinemodels;
extern cvar_t gl_polyblend;
extern cvar_t gl_keeptjunctions;
extern cvar_t gl_reporttjunctions;
extern cvar_t gl_flashblend;
extern cvar_t gl_nocolors;
extern cvar_t gl_doubleeyes;
extern cvar_t gl_ztrick;
extern cvar_t gl_finish;
extern cvar_t gl_subdivide_size;

extern cvar_t scr_viewsize;

extern cvar_t sv_maxvelocity;
extern cvar_t sv_gravity;
extern cvar_t sv_nostep;
extern cvar_t sv_friction;
extern cvar_t sv_edgefriction;
extern cvar_t sv_stopspeed;
extern cvar_t sv_maxspeed;
extern cvar_t sv_accelerate;
extern cvar_t sv_idealpitchscale;
extern cvar_t sv_aim;
extern cvar_t sv_stopspeed;

extern cvar_t hostname;
extern cvar_t sys_ticrate;
extern cvar_t sys_nostdout;
extern cvar_t developer;
extern cvar_t registered;
extern cvar_t teamplay;
extern cvar_t skill;
extern cvar_t deathmatch;
extern cvar_t coop;
extern cvar_t fraglimit;
extern cvar_t timelimit;
extern cvar_t loadas8bit;
extern cvar_t bgmvolume;
extern cvar_t volume;
extern cvar_t v_gamma;
extern cvar_t lcd_x;
extern cvar_t _windowed_mouse;
extern cvar_t crosshair;
extern cvar_t pausable;

extern cvar_t* cvar_vars;

#pragma once

#define	VERSION				1.09
#define	GLQUAKE_VERSION		1.00
#define	D3DQUAKE_VERSION	0.01
#define	WINQUAKE_VERSION	0.996
#define	LINUX_VERSION		1.30
#define	X11_VERSION			1.10

//define	PARANOID			// speed sapping error checking

#define	GAMENAME	"id1"

#include <cmath>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <csetjmp>
#include <mutex>
#include <memory>
#include <limits>
#include <chrono>
#include <cinttypes>
#include <algorithm>
#include <type_traits>
#include <functional>
#include <vector>
#include <list>
#include <queue>
#include <map>
#include <sstream>
#include <fstream>
#include <string>
#include <iostream>
#include <utility>
#include <future>
#include <locale>

#if defined(_WIN32) && !defined(WINDED)

#if defined(_M_IX86)
#define __i386__	1
#endif

void VID_LockBuffer();
void VID_UnlockBuffer();

#else

#define	VID_LockBuffer()
#define	VID_UnlockBuffer()

#endif

#define id386	0

#define UNALIGNED_OK	0

#define CACHE_SIZE	32		// used to align key data structures

#define UNUSED(x)	(x = x)	// for pesky compiler / lint warnings

#define	MINIMUM_MEMORY			0x550000
#define	MINIMUM_MEMORY_LEVELPAK	(MINIMUM_MEMORY + 0x100000)

#define MAX_NUM_ARGVS	50

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2


#define	MAX_QPATH		64			// max length of a quake game pathname
#define	MAX_OSPATH		128			// max length of a filesystem pathname

#define	ON_EPSILON		0.1			// point on plane side epsilon

#define	MAX_MSGLEN		8000		// max length of a reliable message
#define	MAX_DATAGRAM	1024		// max length of unreliable message

//
// per-level limits
//
#define	MAX_EDICTS		600			// FIXME: ouch! ouch! ouch!
#define	MAX_LIGHTSTYLES	64
#define	MAX_MODELS		256			// these are sent over the net as bytes
#define	MAX_SOUNDS		256			// so they cannot be blindly increased

#define	SAVEGAME_COMMENT_LENGTH	39

#define	MAX_STYLESTRING	64

//
// stats are integers communicated to the client by the server
//
#define	MAX_CL_STATS		32
#define	STAT_HEALTH			0
#define	STAT_FRAGS			1
#define	STAT_WEAPON			2
#define	STAT_AMMO			3
#define	STAT_ARMOR			4
#define	STAT_WEAPONFRAME	5
#define	STAT_SHELLS			6
#define	STAT_NAILS			7
#define	STAT_ROCKETS		8
#define	STAT_CELLS			9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_TOTALSECRETS	11
#define	STAT_TOTALMONSTERS	12
#define	STAT_SECRETS		13		// bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14		// bumped by svc_killedmonster

// stock defines

#define	IT_SHOTGUN				1
#define	IT_SUPER_SHOTGUN		2
#define	IT_NAILGUN				4
#define	IT_SUPER_NAILGUN		8
#define	IT_GRENADE_LAUNCHER		16
#define	IT_ROCKET_LAUNCHER		32
#define	IT_LIGHTNING			64
#define IT_SUPER_LIGHTNING      128
#define IT_SHELLS               256
#define IT_NAILS                512
#define IT_ROCKETS              1024
#define IT_CELLS                2048
#define IT_AXE                  4096
#define IT_ARMOR1               8192
#define IT_ARMOR2               16384
#define IT_ARMOR3               32768
#define IT_SUPERHEALTH          65536
#define IT_KEY1                 131072
#define IT_KEY2                 262144
#define	IT_INVISIBILITY			524288
#define	IT_INVULNERABILITY		1048576
#define	IT_SUIT					2097152
#define	IT_QUAD					4194304
#define IT_SIGIL1               (1<<28)
#define IT_SIGIL2               (1<<29)
#define IT_SIGIL3               (1<<30)
#define IT_SIGIL4               (1<<31)

//===========================================
//rogue changed and added defines

#define RIT_SHELLS              128
#define RIT_NAILS               256
#define RIT_ROCKETS             512
#define RIT_CELLS               1024
#define RIT_AXE                 2048
#define RIT_LAVA_NAILGUN        4096
#define RIT_LAVA_SUPER_NAILGUN  8192
#define RIT_MULTI_GRENADE       16384
#define RIT_MULTI_ROCKET        32768
#define RIT_PLASMA_GUN          65536
#define RIT_ARMOR1              8388608
#define RIT_ARMOR2              16777216
#define RIT_ARMOR3              33554432
#define RIT_LAVA_NAILS          67108864
#define RIT_PLASMA_AMMO         134217728
#define RIT_MULTI_ROCKETS       268435456
#define RIT_SHIELD              536870912
#define RIT_ANTIGRAV            1073741824
#define RIT_SUPERHEALTH         2147483648

//MED 01/04/97 added hipnotic defines
//===========================================
//hipnotic added defines
#define HIT_PROXIMITY_GUN_BIT 16
#define HIT_MJOLNIR_BIT       7
#define HIT_LASER_CANNON_BIT  23
#define HIT_PROXIMITY_GUN   (1<<HIT_PROXIMITY_GUN_BIT)
#define HIT_MJOLNIR         (1<<HIT_MJOLNIR_BIT)
#define HIT_LASER_CANNON    (1<<HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT         (1<<(23+2))
#define HIT_EMPATHY_SHIELDS (1<<(23+3))

//===========================================

#define	MAX_SCOREBOARD		16
#define	MAX_SCOREBOARDNAME	32

#define	SOUND_CHANNELS		8

// This makes anyone on id's net privileged
// Use for multiplayer testing only - VERY dangerous!!!
// #define IDGODS

#include "common.h"
#include "bspfile.h"
#include "mathlib.h"

struct entity_state_t
{
	vec3_t origin;
	vec3_t angles;
	int    modelindex;
	int    frame;
	int    colormap;
	int    skin;
	int    effects;
};

#define VID_CBITS	6
#define VID_GRADES	(1 << VID_CBITS)

// a pixel can be one, two, or four bytes
using pixel_t = byte;

struct vrect_t
{
	int      x, y, width, height;
	vrect_t* pnext;
};

struct viddef_t
{
	pixel_t*        buffer; // invisible buffer
	pixel_t*        colormap; // 256 * VID_GRADES size
	unsigned short* colormap16; // 256 * VID_GRADES size
	int             fullbright; // index of first fullbright color
	unsigned        rowbytes; // may be > width if displayed in a window
	unsigned        width;
	unsigned        height;
	float           aspect; // width / height -- < 0 is taller than wide
	int             numpages;
	int             recalc_refdef; // if qtrue, recalc vid-based stuff
	pixel_t*        conbuffer;
	int             conrowbytes;
	unsigned        conwidth;
	unsigned        conheight;
	int             maxwarpwidth;
	int             maxwarpheight;
	pixel_t*        direct;
};

#include "wad.h"
#include "draw.h"
#include "cvar.h"
#include "screen.h"
#include "net.h"
#include "protocol.h"
#include "command.h"
#include "sbar.h"
#include "sound.h"
#include "render.h"
#include "cl_shared.h"
#include "progs.h"
#include "server.h"

#include "gl_model.h"
#include "sv_world.h"
#include "keys.h"
#include "console.h"
#include "view.h"
#include "menu.h"
#include "glquake.h"

//=============================================================================

// the host system specifies the base of the directory tree, the
// command line parms passed to the program, and the amount of memory
// available for the program to use

struct quakeparms_t
{
	char*  basedir;
	char*  cachedir; // for development over ISDN lines
	int    argc;
	char** argv;
	void*  membase;
	int    memsize;
};


//=============================================================================


extern qboolean noclip_anglehack;


//
// host
//
extern quakeparms_t host_parms;


extern qboolean host_initialized; // qtrue if into command execution
extern double   host_frametime;
extern byte*    host_basepal;
extern byte*    host_colormap;
extern int      host_framecount; // incremented every frame, never reset
extern double   realtime; // not bounded in any way, changed at
// start of every frame, never reset

void Host_ClearMemory();
void Host_ServerFrame();
void Host_InitCommands();
void Host_Init(quakeparms_t* parms);
void Host_Shutdown();
void Host_Error(char*   error, ...);
void Host_EndGame(char* message, ...);
void Host_Frame(float   time);
void Host_Quit_f();
void Host_ClientCommands(char*    fmt, ...);
void Host_ShutdownServer(qboolean crash);

void IN_Init();
void IN_Shutdown();
void IN_Commands();
void IN_Move(usercmd_t* cmd);
void IN_ClearStates();
void IN_Accumulate();

int    Sys_FileOpenRead(char*   path, int* hndl);
int    Sys_FileOpenAppend(char* path);
int    Sys_FileOpenWrite(char*  path);
void   Sys_FileClose(int        handle);
void   Sys_FileSeek(int         handle, int   position);
int    Sys_FileRead(int         handle, void* dest, int count);
int    Sys_FileWrite(int        handle, void* data, int count);
int    Sys_FileTime(char*       path);
void   Sys_mkdir(char*          path);
void   Sys_Error(char*          error, ...);
void   Sys_Printf(char*         fmt, ...);
void   Sys_Quit();
double Sys_FloatTime();
char*  Sys_ConsoleInput();
void   Sys_Sleep();
void   Sys_SendKeyEvents();


extern viddef_t       vid; // global video state
extern unsigned short d_8to16table[256];
extern unsigned       d_8to24table[256];
extern void (*        vid_menudrawfn)();
extern void (*        vid_menukeyfn)(int key);

void VID_SetPalette(unsigned char*   palette);
void VID_ShiftPalette(unsigned char* palette);
void VID_Init(unsigned char*         palette);
void VID_Shutdown();
int  VID_SetMode(int modenum, unsigned char* palette);

extern qboolean msg_suppress_1;
extern int      current_skill;
extern qboolean isDedicated;
extern int      minimum_memory;

//
// chase
//
extern cvar_t chase_active;

void Chase_Init();
void Chase_Reset();
void Chase_Update();

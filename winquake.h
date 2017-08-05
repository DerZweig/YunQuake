#pragma once
#pragma warning( disable : 4229 )  // mgraph gets this

#include <windows.h>
#define WM_MOUSEWHEEL                   0x020A

#ifndef SERVERONLY
#include <ddraw.h>
#include <dsound.h>
#endif

extern HINSTANCE global_hInstance;
extern int global_nCmdShow;

#ifndef SERVERONLY

extern LPDIRECTDRAW lpDD;
extern bool DDActive;
extern LPDIRECTDRAWSURFACE lpPrimary;
extern LPDIRECTDRAWSURFACE lpFrontBuffer;
extern LPDIRECTDRAWSURFACE lpBackBuffer;
extern LPDIRECTDRAWPALETTE lpDDPal;
extern LPDIRECTSOUND pDS;
extern LPDIRECTSOUNDBUFFER pDSBuf;

extern DWORD gSndBufSize;
//#define SNDBUFSIZE 65536

void VID_LockBuffer();
void VID_UnlockBuffer();

#endif

enum class modestate_t
{
	MS_WINDOWED,
	MS_FULLSCREEN,
	MS_FULLDIB,
	MS_UNINIT
};

extern modestate_t modestate;

extern HWND mainwindow;
extern bool ActiveApp, Minimized;

extern bool WinNT;

int VID_ForceUnlockedAndReturnState();
void VID_ForceLockState(int lk);

void IN_ShowMouse();
void IN_DeactivateMouse();
void IN_HideMouse();
void IN_ActivateMouse();
void IN_RestoreOriginalMouseState();
void IN_SetQuakeMouseState();
void IN_MouseEvent(int mstate);

extern bool winsock_lib_initialized;

extern cvar_t _windowed_mouse;

extern int window_center_x, window_center_y;
extern RECT window_rect;

extern bool mouseinitialized;
extern HWND hwnd_dialog;

extern HANDLE hinput, houtput;

void IN_UpdateClipCursor();
void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify);

void S_BlockSound();
void S_UnblockSound();

void VID_SetDefaultMode();

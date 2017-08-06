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
extern qboolean DDActive;
extern LPDIRECTDRAWSURFACE lpPrimary;
extern LPDIRECTDRAWSURFACE lpFrontBuffer;
extern LPDIRECTDRAWSURFACE lpBackBuffer;
extern LPDIRECTDRAWPALETTE lpDDPal;
extern LPDIRECTSOUND pDS;
extern LPDIRECTSOUNDBUFFER pDSBuf;

extern DWORD gSndBufSize;
//#define SNDBUFSIZE 65536

void VID_LockBuffer(void);
void VID_UnlockBuffer(void);

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
extern qboolean ActiveApp, Minimized;

int VID_ForceUnlockedAndReturnState(void);
void VID_ForceLockState(int lk);

void IN_ShowMouse(void);
void IN_DeactivateMouse(void);
void IN_HideMouse(void);
void IN_ActivateMouse(void);
void IN_RestoreOriginalMouseState(void);
void IN_SetQuakeMouseState(void);
void IN_MouseEvent(int mstate);

extern qboolean winsock_lib_initialized;

extern cvar_t _windowed_mouse;

extern int window_center_x, window_center_y;
extern RECT window_rect;

extern qboolean mouseinitialized;

extern HANDLE hinput, houtput;

void IN_UpdateClipCursor(void);
void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify);

void S_BlockSound(void);
void S_UnblockSound(void);

void VID_SetDefaultMode(void);

extern int (PASCAL FAR *pWSAStartup)(WORD wVersionRequired, LPWSADATA lpWSAData);
extern int (PASCAL FAR *pWSACleanup)(void);
extern int (PASCAL FAR *pWSAGetLastError)(void);
extern SOCKET (PASCAL FAR *psocket)(int af, int type, int protocol);
extern int (PASCAL FAR *pioctlsocket)(SOCKET s, long cmd, u_long FAR * argp);
extern int (PASCAL FAR *psetsockopt)(SOCKET s, int level, int optname, const char FAR * optval, int optlen);
extern int (PASCAL FAR *precvfrom)(SOCKET s, char FAR * buf, int len, int flags, sockaddr FAR * from, int FAR * fromlen);
extern int (PASCAL FAR *psendto)(SOCKET s, const char FAR * buf, int len, int flags, const sockaddr FAR * to, int tolen);
extern int (PASCAL FAR *pclosesocket)(SOCKET s);
extern int (PASCAL FAR *pgethostname)(char FAR * name, int namelen);
extern hostent FAR * (PASCAL FAR *pgethostbyname)(const char FAR * name);
extern hostent FAR * (PASCAL FAR *pgethostbyaddr)(const char FAR * addr, int len, int type);
extern int (PASCAL FAR *pgetsockname)(SOCKET s, sockaddr FAR * name, int FAR * namelen);

#pragma once
#if defined(_WIN32) && !defined(WINDED)

#if defined(_M_IX86)
#define __i386__	1
#endif

#endif


// !!! must be kept the same as in d_iface.h !!!
#define TRANSPARENT_COLOR	255

#ifndef NeXT


.
extern C (snd_scaletable)
.
extern C (paintbuffer)
.
extern C (snd_linear_count)
.
extern C (snd_p)
.
extern C (snd_vol)
.
extern C (snd_out)
.
extern C (vright)
.
extern C (vup)
.
extern C (vpn)
.
extern C (BOPS_Error)

#endif

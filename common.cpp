#include "quakedef.h"

#define NUM_SAFE_ARGVS  7
#define	DYNAMIC_SIZE	0xc000

#define	ZONEID	0x1d4a11
#define MINFRAGMENT	64

struct memblock_t
{
	int         size; // including the header and possibly tiny fragments
	int         tag; // a tag of 0 is a free block
	int         id; // should be ZONEID
	memblock_t* next;
	memblock_t* prev;
	int         pad; // pad to 64 bit boundary
};

struct memzone_t
{
	int         size; // total bytes malloced, including header
	memblock_t  blocklist; // start / end cap for linked list
	memblock_t* rover;
};

static char* largv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];
static char* argvdummy = " ";

static char* safeargvs[NUM_SAFE_ARGVS] =
	{"-stdvid", "-nolan", "-nosound", "-nocdaudio", "-nojoy", "-nomouse", "-dibonly"};

cvar_t registered = {"registered", "0"};
cvar_t cmdline    = {"cmdline", "0", qfalse, qtrue};

qboolean com_modified; // set qtrue if using non-id files

qboolean proghack;

int static_registered = 1; // only for startup check, then set

qboolean msg_suppress_1 = 0;

void COM_InitFilesystem();
void Cache_FreeLow(int  new_low_hunk);
void Cache_FreeHigh(int new_high_hunk);

// if a packfile directory differs from this, it is assumed to be hacked
#define PAK0_COUNT              339
#define PAK0_CRC                32981

char   com_token[1024];
int    com_argc;
char** com_argv;

#define CMDLINE_LENGTH	256
char com_cmdline[CMDLINE_LENGTH];

qboolean standard_quake = qtrue, rogue, hipnotic;

// this graphic needs to be in the pak file to use registered features
unsigned short pop[] =
{
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x6600, 0x0000, 0x0000, 0x0000, 0x6600, 0x0000, 0x0000, 0x0066, 0x0000, 0x0000, 0x0000, 0x0000, 0x0067, 0x0000, 0x0000, 0x6665, 0x0000, 0x0000, 0x0000, 0x0000, 0x0065, 0x6600, 0x0063, 0x6561, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0061, 0x6563, 0x0064, 0x6561, 0x0000, 0x0000, 0x0000, 0x0000, 0x0061, 0x6564, 0x0064, 0x6564, 0x0000, 0x6469, 0x6969, 0x6400, 0x0064, 0x6564, 0x0063, 0x6568, 0x6200, 0x0064, 0x6864, 0x0000, 0x6268, 0x6563, 0x0000, 0x6567, 0x6963, 0x0064, 0x6764, 0x0063, 0x6967, 0x6500, 0x0000, 0x6266,
	0x6769, 0x6a68, 0x6768, 0x6a69, 0x6766, 0x6200, 0x0000, 0x0062, 0x6566, 0x6666, 0x6666, 0x6666, 0x6562, 0x0000, 0x0000, 0x0000, 0x0062, 0x6364, 0x6664, 0x6362, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0062, 0x6662, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0061, 0x6661, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x6500, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x6400, 0x0000, 0x0000, 0x0000
};

/*


All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

The "cache directory" is only used during development to save network bandwidth, especially over ISDN / T1 lines.  If there is a cache directory
specified, when a file is found by the normal search path, it will be mirrored
into the cache directory, then opened there.



FIXME:
The file "parms.txt" will be read out of the game directory and appended to the current command line arguments to allow different games to initialize startup parms differently.  This could be used to add a "-sspeed 22050" for the high quality sound edition.  Because they are added at the end, they will not override an explicit setting on the original command line.
	
*/

//============================================================================


// ClearLink is used for new headnodes
void ClearLink(link_t* l)
{
	l->prev = l->next = l;
}

void RemoveLink(link_t* l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore(link_t* l, link_t* before)
{
	l->next       = before;
	l->prev       = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void InsertLinkAfter(link_t* l, link_t* after)
{
	l->next       = after->next;
	l->prev       = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

void Q_memset(void* dest, int fill, int count)
{
	int i;

	if (((reinterpret_cast<long>(dest) | count) & 3) == 0)
	{
		count >>= 2;
		fill                            = fill | fill << 8 | fill << 16 | fill << 24;
		for (i                          = 0; i < count; i++)
			static_cast<int *>(dest)[i] = fill;
	}
	else
		for (i                           = 0; i < count; i++)
			static_cast<byte *>(dest)[i] = fill;
}

void Q_memcpy(void* dest, void* src, int count)
{
	int i;

	if (((reinterpret_cast<long>(dest) | reinterpret_cast<long>(src) | count) & 3) == 0)
	{
		count >>= 2;
		for (i                          = 0; i < count; i++)
			static_cast<int *>(dest)[i] = static_cast<int *>(src)[i];
	}
	else
		for (i                           = 0; i < count; i++)
			static_cast<byte *>(dest)[i] = static_cast<byte *>(src)[i];
}

int Q_memcmp(void* m1, void* m2, int count)
{
	while (count)
	{
		count--;
		if (static_cast<byte *>(m1)[count] != static_cast<byte *>(m2)[count])
			return -1;
	}
	return 0;
}

void Q_strcpy(char* dest, char* src)
{
	while (*src)
	{
		*dest++ = *src++;
	}
	*dest = 0;
}

void Q_strncpy(char* dest, char* src, int count)
{
	while (*src && count--)
	{
		*dest++ = *src++;
	}
	if (count)
		*dest = 0;
}

int Q_strlen(char* str)
{
	auto count = 0;
	while (str[count])
		count++;

	return count;
}

char* Q_strrchr(char* s, char c)
{
	auto len = Q_strlen(s);
	s += len;
	while (len--)
		if (*--s == c) return s;
	return nullptr;
}

void Q_strcat(char* dest, char* src)
{
	dest += Q_strlen(dest);
	Q_strcpy(dest, src);
}

int Q_strcmp(char* s1, char* s2)
{
	while (true)
	{
		if (*s1 != *s2)
			return -1; // strings not equal    
		if (!*s1)
			return 0; // strings are equal
		s1++;
		s2++;
	}
}

int Q_strncmp(char* s1, char* s2, int count)
{
	while (true)
	{
		if (!count--)
			return 0;
		if (*s1 != *s2)
			return -1; // strings not equal    
		if (!*s1)
			return 0; // strings are equal
		s1++;
		s2++;
	}
}

int Q_strncasecmp(const char* s1, char* s2, int n)
{
	while (true)
	{
		int c1 = *s1++;
		int c2 = *s2++;

		if (!n--)
			return 0; // strings are equal until end point

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= 'a' - 'A';
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= 'a' - 'A';
			if (c1 != c2)
				return -1; // strings not equal
		}
		if (!c1)
			return 0;
	}
}

int Q_strcasecmp(char* s1, char* s2)
{
	return Q_strncasecmp(s1, s2, 99999);
}

int Q_atoi(char* str)
{
	int sign;
	int c;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;

	auto val = 0;

	//
	// check for hex
	//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
		while (true)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val << 4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val << 4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val << 4) + c - 'A' + 10;
			else
				return val * sign;
		}
	}

	//
	// check for character
	//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}

	//
	// assume decimal
	//
	while (true)
	{
		c = *str++;
		if (c < '0' || c > '9')
			return val * sign;
		val = val * 10 + c - '0';
	}
}


float Q_atof(char* str)
{
	int sign;
	int c;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;

	double val = 0;

	//
	// check for hex
	//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
		while (true)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = val * 16 + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = val * 16 + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = val * 16 + c - 'A' + 10;
			else
				return val * sign;
		}
	}

	//
	// check for character
	//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}

	//
	// assume decimal
	//
	auto decimal = -1;
	auto total   = 0;
	while (true)
	{
		c = *str++;
		if (c == '.')
		{
			decimal = total;
			continue;
		}
		if (c < '0' || c > '9')
			break;
		val = val * 10 + c - '0';
		total++;
	}

	if (decimal == -1)
		return val * sign;
	while (total > decimal)
	{
		val /= 10;
		total--;
	}

	return val * sign;
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

qboolean bigendien;

short (*BigShort)(short    l);
short (*LittleShort)(short l);
int (*  BigLong)(int       l);
int (*  LittleLong)(int    l);
float (*BigFloat)(float    l);
float (*LittleFloat)(float l);

short ShortSwap(const short l)
{
	const byte b1 = l & 255;
	const byte b2 = l >> 8 & 255;

	return (b1 << 8) + b2;
}

short ShortNoSwap(const short l)
{
	return l;
}

int LongSwap(const int l)
{
	const byte b1 = l & 255;
	const byte b2 = l >> 8 & 255;
	const byte b3 = l >> 16 & 255;
	const byte b4 = l >> 24 & 255;

	return (static_cast<int>(b1) << 24) + (static_cast<int>(b2) << 16) + (static_cast<int>(b3) << 8) + b4;
}

int LongNoSwap(const int l)
{
	return l;
}

float FloatSwap(const float f)
{
	union
	{
		float f;
		byte  b[4];
	}         dat1, dat2;


	dat1.f    = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap(const float f)
{
	return f;
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

//
// writing functions
//

void MSG_WriteChar(sizebuf_t* sb, const int c)
{
#ifdef PARANOID
	if (c < -128 || c > 127)
		Sys_Error ("MSG_WriteChar: range error");
#endif

	const auto buf = static_cast<byte *>(SZ_GetSpace(sb, 1));
	buf[0]         = c;
}

void MSG_WriteByte(sizebuf_t* sb, const int c)
{
#ifdef PARANOID
	if (c < 0 || c > 255)
		Sys_Error ("MSG_WriteByte: range error");
#endif

	const auto buf = static_cast<byte *>(SZ_GetSpace(sb, 1));
	buf[0]         = c;
}

void MSG_WriteShort(sizebuf_t* sb, const int c)
{
#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
		Sys_Error ("MSG_WriteShort: range error");
#endif

	const auto buf = static_cast<byte *>(SZ_GetSpace(sb, 2));
	buf[0]         = c & 0xff;
	buf[1]         = c >> 8;
}

auto MSG_WriteLong(sizebuf_t* sb, const int c) -> void
{
	const auto buf = static_cast<byte *>(SZ_GetSpace(sb, 4));
	buf[0]         = c & 0xff;
	buf[1]         = c >> 8 & 0xff;
	buf[2]         = c >> 16 & 0xff;
	buf[3]         = c >> 24;
}

void MSG_WriteFloat(sizebuf_t* sb, const float f)
{
	union
	{
		float f;
		int   l;
	}         dat;


	dat.f = f;
	dat.l = LittleLong(dat.l);

	SZ_Write(sb, &dat.l, 4);
}

void MSG_WriteString(sizebuf_t* sb, char* s)
{
	if (!s)
		SZ_Write(sb, "", 1);
	else
		SZ_Write(sb, s, Q_strlen(s) + 1);
}

void MSG_WriteCoord(sizebuf_t* sb, const float f)
{
	MSG_WriteShort(sb, static_cast<int>(f * 8));
}

void MSG_WriteAngle(sizebuf_t* sb, const float f)
{
	MSG_WriteByte(sb, static_cast<int>(f) * 256 / 360 & 255);
}

//
// reading functions
//
int      msg_readcount;
qboolean msg_badread;

void MSG_BeginReading()
{
	msg_readcount = 0;
	msg_badread   = qfalse;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar()
{
	if (msg_readcount + 1 > net_message.cursize)
	{
		msg_badread = qtrue;
		return -1;
	}

	const int c = static_cast<signed char>(net_message.data[msg_readcount]);
	msg_readcount++;

	return c;
}

int MSG_ReadByte()
{
	if (msg_readcount + 1 > net_message.cursize)
	{
		msg_badread = qtrue;
		return -1;
	}

	const int c = static_cast<unsigned char>(net_message.data[msg_readcount]);
	msg_readcount++;

	return c;
}

int MSG_ReadShort()
{
	if (msg_readcount + 2 > net_message.cursize)
	{
		msg_badread = qtrue;
		return -1;
	}

	const int c = static_cast<short>(net_message.data[msg_readcount] + (net_message.data[msg_readcount + 1] << 8));

	msg_readcount += 2;

	return c;
}

int MSG_ReadLong()
{
	if (msg_readcount + 4 > net_message.cursize)
	{
		msg_badread = qtrue;
		return -1;
	}

	const auto c = net_message.data[msg_readcount]
		+ (net_message.data[msg_readcount + 1] << 8)
		+ (net_message.data[msg_readcount + 2] << 16)
		+ (net_message.data[msg_readcount + 3] << 24);

	msg_readcount += 4;

	return c;
}

float MSG_ReadFloat()
{
	union
	{
		byte  b[4];
		float f;
		int   l;
	}         dat;

	dat.b[0] = net_message.data[msg_readcount];
	dat.b[1] = net_message.data[msg_readcount + 1];
	dat.b[2] = net_message.data[msg_readcount + 2];
	dat.b[3] = net_message.data[msg_readcount + 3];
	msg_readcount += 4;

	dat.l = LittleLong(dat.l);

	return dat.f;
}

char* MSG_ReadString()
{
	static char string[2048];

	auto l = 0;
	do
	{
		const auto c = MSG_ReadChar();
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	}
	while (l < sizeof string - 1);

	string[l] = 0;

	return string;
}

float MSG_ReadCoord()
{
	return MSG_ReadShort() * (1.0 / 8);
}

float MSG_ReadAngle()
{
	return MSG_ReadChar() * (360.0 / 256);
}


//===========================================================================

void SZ_Alloc(sizebuf_t* buf, int startsize)
{
	if (startsize < 256)
		startsize = 256;
	buf->data     = static_cast<byte*>(Hunk_AllocName(startsize, "sizebuf"));
	buf->maxsize  = startsize;
	buf->cursize  = 0;
}


void SZ_Free(sizebuf_t* buf)
{
	//      Z_Free (buf->data);
	//      buf->data = nullptr;
	//      buf->maxsize = 0;
	buf->cursize = 0;
}

void SZ_Clear(sizebuf_t* buf)
{
	buf->cursize = 0;
}

void* SZ_GetSpace(sizebuf_t* buf, const int length)
{
	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Sys_Error("SZ_GetSpace: overflow without allowoverflow set");

		if (length > buf->maxsize)
			Sys_Error("SZ_GetSpace: %i is > full buffer size", length);

		buf->overflowed = qtrue;
		Con_Printf("SZ_GetSpace: overflow");
		SZ_Clear(buf);
	}

	void* data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write(sizebuf_t* buf, void* data, int length)
{
	Q_memcpy(SZ_GetSpace(buf, length), data, length);
}

void SZ_Print(sizebuf_t* buf, char* data)
{
	const auto len = Q_strlen(data) + 1;

	// byte * cast to keep VC++ happy
	if (buf->data[buf->cursize - 1])
		Q_memcpy(static_cast<byte *>(SZ_GetSpace(buf, len)), data, len); // no trailing 0
	else
		Q_memcpy(static_cast<byte *>(SZ_GetSpace(buf, len - 1)) - 1, data, len); // write over trailing 0
}


//============================================================================


/*
============
COM_SkipPath
============
*/
char* COM_SkipPath(char* pathname)
{
	auto last = pathname;
	while (*pathname)
	{
		if (*pathname == '/')
			last = pathname + 1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension(char* in, char* out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out       = 0;
}

/*
============
COM_FileExtension
============
*/
char* COM_FileExtension(char* in)
{
	static char exten[8];
	int         i;

	while (*in && *in != '.')
		in++;
	if (!*in)
		return "";
	in++;
	for (i       = 0; i < 7 && *in; i++, in++)
		exten[i] = *in;
	exten[i]     = 0;
	return exten;
}

/*
============
COM_FileBase
============
*/
void COM_FileBase(char* in, char* out)
{
	auto s = in + strlen(in) - 1;

	while (s != in && *s != '.')
		s--;

	auto s2 = s;

	while (*s2 && *s2 != '/')
	{
		--s2;
	}

	if (s - s2 < 2)
		strcpy(out, "?model?");
	else
	{
		s--;
		strncpy(out, s2 + 1, s - s2);
		out[s - s2] = 0;
	}
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension(char* path, char* extension)
{
	//
	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	//
	auto src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return; // it has an extension
		src--;
	}

	strcat(path, extension);
}


/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char* COM_Parse(char* data)
{
	int c;

	auto len     = 0;
	com_token[0] = 0;

	if (!data)
		return nullptr;

	// skip whitespace
skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
			return nullptr; // end of file;
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}


	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (true)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse single characters
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
			break;
	}
	while (c > 32);

	com_token[len] = 0;
	return data;
}


/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParm(char* parm)
{
	for (auto i = 1; i < com_argc; i++)
	{
		if (!com_argv[i])
			continue; // NEXTSTEP sometimes clears appkit vars.
		if (!Q_strcmp(parm, com_argv[i]))
			return i;
	}

	return 0;
}

/*
================
COM_CheckRegistered

Looks for the pop.txt file and verifies it.
Sets the "registered" cvar.
Immediately exits out if an alternate game was attempted to be started without
being registered.
================
*/
void COM_CheckRegistered()
{
	int            h;
	unsigned short check[128];

	COM_OpenFile("gfx/pop.lmp", &h);
	static_registered = 0;

	if (h == -1)
	{
#if WINDED
	Sys_Error ("This dedicated server requires a full registered copy of Quake");
#endif
		Con_Printf("Playing shareware version.\n");
		if (com_modified)
			Sys_Error("You must have the registered version to use modified games");
		return;
	}

	Sys_FileRead(h, check, sizeof check);
	COM_CloseFile(h);

	for (auto i = 0; i < 128; i++)
		if (pop[i] != static_cast<unsigned short>(BigShort(check[i])))
			Sys_Error("Corrupted data file.");

	Cvar_Set("cmdline", com_cmdline);
	Cvar_Set("registered", "1");
	static_registered = 1;
	Con_Printf("Playing registered version.\n");
}


void COM_Path_f();


/*
================
COM_InitArgv
================
*/
void COM_InitArgv(const int argc, char** argv)
{
	int i;

	// reconstitute the command line for the cmdline externally visible cvar
	auto n = 0;

	for (auto j = 0; j < MAX_NUM_ARGVS && j < argc; j++)
	{
		i = 0;

		while (n < CMDLINE_LENGTH - 1 && argv[j][i])
		{
			com_cmdline[n++] = argv[j][i++];
		}

		if (n < CMDLINE_LENGTH - 1)
			com_cmdline[n++] = ' ';
		else
			break;
	}

	com_cmdline[n] = 0;

	auto safe = qfalse;

	for (com_argc = 0; com_argc < MAX_NUM_ARGVS && com_argc < argc;
	     com_argc++)
	{
		largv[com_argc] = argv[com_argc];
		if (!Q_strcmp("-safe", argv[com_argc]))
			safe = qtrue;
	}

	if (safe)
	{
		// force all the safe-mode switches. Note that we reserved extra space in
		// case we need to add these, so we don't need an overflow check
		for (i = 0; i < NUM_SAFE_ARGVS; i++)
		{
			largv[com_argc] = safeargvs[i];
			com_argc++;
		}
	}

	largv[com_argc] = argvdummy;
	com_argv        = largv;

	if (COM_CheckParm("-rogue"))
	{
		rogue          = qtrue;
		standard_quake = qfalse;
	}

	if (COM_CheckParm("-hipnotic"))
	{
		hipnotic       = qtrue;
		standard_quake = qfalse;
	}
}


/*
================
COM_Init
================
*/
void COM_Init(char* basedir)
{
	byte swaptest[2] = {1, 0};

	// set the byte swapping variables in a portable manner 
	if (*reinterpret_cast<short *>(swaptest) == 1)
	{
		bigendien   = qfalse;
		BigShort    = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong     = LongSwap;
		LittleLong  = LongNoSwap;
		BigFloat    = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien   = qtrue;
		BigShort    = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong     = LongNoSwap;
		LittleLong  = LongSwap;
		BigFloat    = FloatNoSwap;
		LittleFloat = FloatSwap;
	}

	Cvar_RegisterVariable(&registered);
	Cvar_RegisterVariable(&cmdline);
	Cmd_AddCommand("path", COM_Path_f);

	COM_InitFilesystem();
	COM_CheckRegistered();
}


/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char* va(char* format, ...)
{
	va_list     argptr;
	static char string[1024];

	va_start (argptr, format);
	vsprintf(string, format, argptr);
	va_end (argptr);

	return string;
}


/// just for debugging
int memsearch(const byte* start, const int count, const int search)
{
	for (auto i = 0; i < count; i++)
		if (start[i] == search)
			return i;
	return -1;
}

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

int com_filesize;


//
// in memory
//

struct packfile_t
{
	char name[MAX_QPATH];
	int  filepos, filelen;
};

struct pack_t
{
	char        filename[MAX_OSPATH];
	int         handle;
	int         numfiles;
	packfile_t* files;
};

//
// on disk
//
struct dpackfile_t
{
	char name[56];
	int  filepos, filelen;
};

struct dpackheader_t
{
	char id[4];
	int  dirofs;
	int  dirlen;
};

#define MAX_FILES_IN_PACK       2048

char com_cachedir[MAX_OSPATH];
char com_gamedir[MAX_OSPATH];

struct searchpath_t
{
	char          filename[MAX_OSPATH];
	pack_t*       pack; // only one of filename / pack will be used
	searchpath_t* next;
};

searchpath_t* com_searchpaths;

/*
============
COM_Path_f

============
*/
void COM_Path_f()
{
	Con_Printf("Current search path:\n");
	for (auto s = com_searchpaths; s; s = s->next)
	{
		if (s->pack)
		{
			Con_Printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		}
		else
			Con_Printf("%s\n", s->filename);
	}
}

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void COM_WriteFile(char* filename, void* data, int len)
{
	char name[MAX_OSPATH];

	sprintf(name, "%s/%s", com_gamedir, filename);

	const auto handle = Sys_FileOpenWrite(name);
	if (handle == -1)
	{
		Sys_Printf("COM_WriteFile: failed on %s\n", name);
		return;
	}

	Sys_Printf("COM_WriteFile: %s\n", name);
	Sys_FileWrite(handle, data, len);
	Sys_FileClose(handle);
}


/*
============
COM_CreatePath

Only used for CopyFile
============
*/
void COM_CreatePath(char* path)
{
	for (auto ofs = path + 1; *ofs; ofs++)
	{
		if (*ofs == '/')
		{
			// create the directory
			*ofs = 0;
			Sys_mkdir(path);
			*ofs = '/';
		}
	}
}


/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed.  This is for the convenience of developers using ISDN from home.
===========
*/
void COM_CopyFile(char* netpath, char* cachepath)
{
	int  in;
	int  count;
	char buf[4096];

	auto remaining = Sys_FileOpenRead(netpath, &in);
	COM_CreatePath(cachepath); // create directories up to the cache file
	const auto out = Sys_FileOpenWrite(cachepath);

	while (remaining)
	{
		if (remaining < sizeof buf)
			count = remaining;
		else
			count = sizeof buf;
		Sys_FileRead(in, buf, count);
		Sys_FileWrite(out, buf, count);
		remaining -= count;
	}

	Sys_FileClose(in);
	Sys_FileClose(out);
}

/*
===========
COM_FindFile

Finds the file in the search path.
Sets com_filesize and one of handle or file
===========
*/
int COM_FindFile(char* filename, int* handle, FILE** file)
{
	char netpath[MAX_OSPATH];
	char cachepath[MAX_OSPATH];
	int  i;

	if (file && handle)
		Sys_Error("COM_FindFile: both handle and file set");
	if (!file && !handle)
		Sys_Error("COM_FindFile: neither handle or file set");

	//
	// search through the path, one element at a time
	//
	auto search = com_searchpaths;
	if (proghack)
	{
		// gross hack to use quake 1 progs with quake 2 maps
		if (!strcmp(filename, "progs.dat"))
			search = search->next;
	}

	for (; search; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			// look through all the pak file elements
			auto pak = search->pack;
			for (i   = 0; i < pak->numfiles; i++)
				if (!strcmp(pak->files[i].name, filename))
				{
					// found it!
					Sys_Printf("PackFile: %s : %s\n", pak->filename, filename);
					if (handle)
					{
						*handle = pak->handle;
						Sys_FileSeek(pak->handle, pak->files[i].filepos);
					}
					else
					{
						// open a new file on the pakfile
						*file = fopen(pak->filename, "rb");
						if (*file)
							fseek(*file, pak->files[i].filepos, SEEK_SET);
					}
					com_filesize = pak->files[i].filelen;
					return com_filesize;
				}
		}
		else
		{
			// check a file in the directory tree
			if (!static_registered)
			{
				// if not a registered version, don't ever go beyond base
				if (strchr(filename, '/') || strchr(filename, '\\'))
					continue;
			}

			sprintf(netpath, "%s/%s", search->filename, filename);

			const auto findtime = Sys_FileTime(netpath);
			if (findtime == -1)
				continue;

			// see if the file needs to be updated in the cache
			if (!com_cachedir[0])
				strcpy(cachepath, netpath);
			else
			{
#if defined(_WIN32)
				if (strlen(netpath) < 2 || netpath[1] != ':')
					sprintf(cachepath, "%s%s", com_cachedir, netpath);
				else
					sprintf(cachepath, "%s%s", com_cachedir, netpath + 2);
#else
				sprintf (cachepath,"%s%s", com_cachedir, netpath);
#endif

				const auto cachetime = Sys_FileTime(cachepath);

				if (cachetime < findtime)
					COM_CopyFile(netpath, cachepath);
				strcpy(netpath, cachepath);
			}

			Sys_Printf("FindFile: %s\n", netpath);
			com_filesize = Sys_FileOpenRead(netpath, &i);
			if (handle)
				*handle = i;
			else
			{
				Sys_FileClose(i);
				*file = fopen(netpath, "rb");
			}
			return com_filesize;
		}
	}

	Sys_Printf("FindFile: can't find %s\n", filename);

	if (handle)
		*handle = -1;
	else
		*file    = nullptr;
	com_filesize = -1;
	return -1;
}


/*
===========
COM_OpenFile

filename never has a leading slash, but may contain directory walks
returns a handle and a length
it may actually be inside a pak file
===========
*/
int COM_OpenFile(char* filename, int* hndl)
{
	return COM_FindFile(filename, hndl, nullptr);
}

/*
===========
COM_FOpenFile

If the requested file is inside a packfile, a new FILE * will be opened
into the file.
===========
*/
int COM_FOpenFile(char* filename, FILE** file)
{
	return COM_FindFile(filename, nullptr, file);
}

/*
============
COM_CloseFile

If it is a pak file handle, don't really close it
============
*/
void COM_CloseFile(const int h)
{
	for (auto s = com_searchpaths; s; s = s->next)
		if (s->pack && s->pack->handle == h)
			return;

	Sys_FileClose(h);
}


/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Allways appends a 0 byte.
============
*/
cache_user_t* loadcache;
byte*         loadbuf;
int           loadsize;

byte* COM_LoadFile(char* path, int usehunk)
{
	int  h;
	char base[32];

	byte* buf = nullptr; // quiet compiler warning

	// look for it in the filesystem or pack files
	const auto len = COM_OpenFile(path, &h);
	if (h == -1)
		return nullptr;

	// extract the filename base name for hunk tag
	COM_FileBase(path, base);

	if (usehunk == 1)
		buf = static_cast<byte*>(Hunk_AllocName(len + 1, base));
	else if (usehunk == 2)
		buf = static_cast<byte*>(Hunk_TempAlloc(len + 1));
	else if (usehunk == 0)
		buf = static_cast<byte*>(Z_Malloc(len + 1));
	else if (usehunk == 3)
		buf = static_cast<byte*>(Cache_Alloc(loadcache, len + 1, base));
	else if (usehunk == 4)
	{
		if (len + 1 > loadsize)
			buf = static_cast<byte*>(Hunk_TempAlloc(len + 1));
		else
			buf = loadbuf;
	}
	else
		Sys_Error("COM_LoadFile: bad usehunk");

	if (!buf)
		Sys_Error("COM_LoadFile: not enough space for %s", path);

	static_cast<byte *>(buf)[len] = 0;

	Draw_BeginDisc();
	Sys_FileRead(h, buf, len);
	COM_CloseFile(h);
	Draw_EndDisc();

	return buf;
}

byte* COM_LoadHunkFile(char* path)
{
	return COM_LoadFile(path, 1);
}

byte* COM_LoadTempFile(char* path)
{
	return COM_LoadFile(path, 2);
}

void COM_LoadCacheFile(char* path, cache_user_t* cu)
{
	loadcache = cu;
	COM_LoadFile(path, 3);
}

// uses temp hunk if larger than bufsize
byte* COM_LoadStackFile(char* path, void* buffer, int bufsize)
{
	loadbuf        = static_cast<byte *>(buffer);
	loadsize       = bufsize;
	const auto buf = COM_LoadFile(path, 4);

	return buf;
}

/*
=================
COM_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t* COM_LoadPackFile(char* packfile)
{
	dpackheader_t  header;
	int            i;
	int            packhandle;
	dpackfile_t    info[MAX_FILES_IN_PACK];
	unsigned short crc;

	if (Sys_FileOpenRead(packfile, &packhandle) == -1)
	{
		//              Con_Printf ("Couldn't open %s\n", packfile);
		return nullptr;
	}
	Sys_FileRead(packhandle, static_cast<void *>(&header), sizeof header);
	if (header.id[0] != 'P' || header.id[1] != 'A'
		|| header.id[2] != 'C' || header.id[3] != 'K')
		Sys_Error("%s is not a packfile", packfile);
	header.dirofs = LittleLong(header.dirofs);
	header.dirlen = LittleLong(header.dirlen);

	const int numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error("%s has %i files", packfile, numpackfiles);

	if (numpackfiles != PAK0_COUNT)
		com_modified = qtrue; // not the original file

	const auto newfiles = static_cast<packfile_t *>(Hunk_AllocName(numpackfiles * sizeof(packfile_t), "packfile"));

	Sys_FileSeek(packhandle, header.dirofs);
	Sys_FileRead(packhandle, static_cast<void *>(info), header.dirlen);

	// crc the directory to check for modifications
	CRC_Init(&crc);
	for (i = 0; i < header.dirlen; i++)
		CRC_ProcessByte(&crc, reinterpret_cast<byte *>(info)[i]);
	if (crc != PAK0_CRC)
		com_modified = qtrue;

	// parse the directory
	for (i = 0; i < numpackfiles; i++)
	{
		strcpy(newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);
	}

	auto pack = static_cast<pack_t *>(Hunk_Alloc(sizeof(pack_t)));
	strcpy(pack->filename, packfile);
	pack->handle   = packhandle;
	pack->numfiles = numpackfiles;
	pack->files    = newfiles;

	Con_Printf("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}


/*
================
COM_AddGameDirectory

Sets com_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ... 
================
*/
void COM_AddGameDirectory(char* dir)
{
	char pakfile[MAX_OSPATH];

	strcpy(com_gamedir, dir);

	//
	// add the directory to the search path
	//
	auto search = static_cast<searchpath_t*>(Hunk_Alloc(sizeof(searchpath_t)));
	strcpy(search->filename, dir);
	search->next    = com_searchpaths;
	com_searchpaths = search;

	//
	// add any pak files in the format pak0.pak pak1.pak, ...
	//
	for (auto i = 0; ; i++)
	{
		sprintf(pakfile, "%s/pak%i.pak", dir, i);
		const auto pak = COM_LoadPackFile(pakfile);
		if (!pak)
			break;
		search          = static_cast<searchpath_t*>(Hunk_Alloc(sizeof(searchpath_t)));
		search->pack    = pak;
		search->next    = com_searchpaths;
		com_searchpaths = search;
	}

	//
	// add the contents of the parms.txt file to the end of the command line
	//
}

/*
================
COM_InitFilesystem
================
*/
void COM_InitFilesystem()
{
	char basedir[MAX_OSPATH];

	//
	// -basedir <path>
	// Overrides the system supplied base directory (under GAMENAME)
	//
	auto i = COM_CheckParm("-basedir");
	if (i && i < com_argc - 1)
		strcpy(basedir, com_argv[i + 1]);
	else
		strcpy(basedir, host_parms.basedir);

	const int j = strlen(basedir);

	if (j > 0)
	{
		if (basedir[j - 1] == '\\' || basedir[j - 1] == '/')
			basedir[j - 1] = 0;
	}

	//
	// -cachedir <path>
	// Overrides the system supplied cache directory (nullptr or /qcache)
	// -cachedir - will disable caching.
	//
	i = COM_CheckParm("-cachedir");
	if (i && i < com_argc - 1)
	{
		if (com_argv[i + 1][0] == '-')
			com_cachedir[0] = 0;
		else
			strcpy(com_cachedir, com_argv[i + 1]);
	}
	else if (host_parms.cachedir)
		strcpy(com_cachedir, host_parms.cachedir);
	else
		com_cachedir[0] = 0;

	//
	// start up with GAMENAME by default (id1)
	//
	COM_AddGameDirectory(va("%s/" GAMENAME, basedir));

	if (COM_CheckParm("-rogue"))
		COM_AddGameDirectory(va("%s/rogue", basedir));
	if (COM_CheckParm("-hipnotic"))
		COM_AddGameDirectory(va("%s/hipnotic", basedir));

	//
	// -game <gamedir>
	// Adds basedir/gamedir as an override game
	//
	i = COM_CheckParm("-game");
	if (i && i < com_argc - 1)
	{
		com_modified = qtrue;
		COM_AddGameDirectory(va("%s/%s", basedir, com_argv[i + 1]));
	}

	//
	// -path <dir or packfile> [<dir or packfile>] ...
	// Fully specifies the exact serach path, overriding the generated one
	//
	i = COM_CheckParm("-path");
	if (i)
	{
		com_modified    = qtrue;
		com_searchpaths = nullptr;
		while (++i < com_argc)
		{
			if (!com_argv[i] || com_argv[i][0] == '+' || com_argv[i][0] == '-')
				break;

			auto search = static_cast<searchpath_t *>(Hunk_Alloc(sizeof(searchpath_t)));
			if (!strcmp(COM_FileExtension(com_argv[i]), "pak"))
			{
				search->pack = COM_LoadPackFile(com_argv[i]);
				if (!search->pack)
					Sys_Error("Couldn't load packfile: %s", com_argv[i]);
			}
			else
				strcpy(search->filename, com_argv[i]);
			search->next    = com_searchpaths;
			com_searchpaths = search;
		}
	}

	if (COM_CheckParm("-proghack"))
		proghack = qtrue;
}


/*
==============================================================================

ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

The zone calls are pretty much only used for small strings and structures,
all big things are allocated on the hunk.
==============================================================================
*/

memzone_t* mainzone;

void Z_ClearZone(memzone_t* zone, int size);


/*
========================
Z_ClearZone
========================
*/
void Z_ClearZone(memzone_t* zone, int size)
{
	// set the entire zone to one free block

	zone->blocklist.next = reinterpret_cast<memblock_t *>(reinterpret_cast<byte *>(zone) + sizeof(memzone_t));
	zone->blocklist.prev = zone->blocklist.next;
	const auto block     = zone->blocklist.next;

	zone->blocklist.tag  = 1; // in use block
	zone->blocklist.id   = 0;
	zone->blocklist.size = 0;
	zone->rover          = block;

	block->prev = block->next = &zone->blocklist;
	block->tag  = 0; // free block
	block->id   = ZONEID;
	block->size = size - sizeof(memzone_t);
}


/*
========================
Z_Free
========================
*/
void Z_Free(void* ptr)
{
	if (!ptr)
		Sys_Error("Z_Free: nullptr pointer");

	auto block = reinterpret_cast<memblock_t *>(static_cast<byte *>(ptr) - sizeof(memblock_t));
	if (block->id != ZONEID)
		Sys_Error("Z_Free: freed a pointer without ZONEID");
	if (block->tag == 0)
		Sys_Error("Z_Free: freed a freed pointer");

	block->tag = 0; // mark as free

	auto other = block->prev;
	if (!other->tag)
	{
		// merge with previous free block
		other->size += block->size;
		other->next       = block->next;
		other->next->prev = other;
		if (block == mainzone->rover)
			mainzone->rover = other;
		block               = other;
	}

	other = block->next;
	if (!other->tag)
	{
		// merge the next free block onto the end
		block->size += other->size;
		block->next       = other->next;
		block->next->prev = block;
		if (other == mainzone->rover)
			mainzone->rover = block;
	}
}


/*
========================
Z_Malloc
========================
*/
void* Z_Malloc(int size)
{
	Z_CheckHeap(); // DEBUG
	const auto buf = Z_TagMalloc(size, 1);
	if (!buf)
		Sys_Error("Z_Malloc: failed on allocation of %i bytes", size);
	Q_memset(buf, 0, size);

	return buf;
}

void* Z_TagMalloc(int size, int tag)
{
	memblock_t* rover;

	if (!tag)
		Sys_Error("Z_TagMalloc: tried to use a 0 tag");

	//
	// scan through the block list looking for the first free block
	// of sufficient size
	//
	size += sizeof(memblock_t); // account for size of block header
	size += 4; // space for memory trash tester
	size = size + 7 & ~7; // align to 8-byte boundary

	auto       base  = rover = mainzone->rover;
	const auto start = base->prev;

	do
	{
		if (rover == start) // scaned all the way around the list
			return nullptr;
		if (rover->tag)
			base = rover = rover->next;
		else
			rover = rover->next;
	}
	while (base->tag || base->size < size);

	//
	// found a block big enough
	//
	const auto extra = base->size - size;
	if (extra > MINFRAGMENT)
	{
		// there will be a free fragment after the allocated block
		const auto res  = reinterpret_cast<memblock_t *>(reinterpret_cast<byte *>(base) + size);
		res->size       = extra;
		res->tag        = 0; // free block
		res->prev       = base;
		res->id         = ZONEID;
		res->next       = base->next;
		res->next->prev = res;
		base->next      = res;
		base->size      = size;
	}

	base->tag = tag; // no longer a free block

	mainzone->rover = base->next; // next allocation will start looking here

	base->id = ZONEID;

	// marker for memory trash testing
	*reinterpret_cast<int *>(reinterpret_cast<byte *>(base) + base->size - 4) = ZONEID;

	return static_cast<void *>(reinterpret_cast<byte *>(base) + sizeof(memblock_t));
}


/*
========================
Z_Print
========================
*/
void Z_Print(memzone_t* zone)
{
	Con_Printf("zone size: %i  location: %p\n", mainzone->size, mainzone);

	for (auto block = zone->blocklist.next; ; block = block->next)
	{
		Con_Printf("block:%p    size:%7i    tag:%3i\n",
		           block, block->size, block->tag);

		if (block->next == &zone->blocklist)
			break; // all blocks have been hit	
		if (reinterpret_cast<byte *>(block) + block->size != reinterpret_cast<byte *>(block->next))
			Con_Printf("ERROR: block size does not touch the next block\n");
		if (block->next->prev != block)
			Con_Printf("ERROR: next block doesn't have proper back link\n");
		if (!block->tag && !block->next->tag)
			Con_Printf("ERROR: two consecutive free blocks\n");
	}
}


/*
========================
Z_CheckHeap
========================
*/
void Z_CheckHeap()
{
	for (auto block = mainzone->blocklist.next; ; block = block->next)
	{
		if (block->next == &mainzone->blocklist)
			break; // all blocks have been hit	
		if (reinterpret_cast<byte *>(block) + block->size != reinterpret_cast<byte *>(block->next))
			Sys_Error("Z_CheckHeap: block size does not touch the next block\n");
		if (block->next->prev != block)
			Sys_Error("Z_CheckHeap: next block doesn't have proper back link\n");
		if (!block->tag && !block->next->tag)
			Sys_Error("Z_CheckHeap: two consecutive free blocks\n");
	}
}

//============================================================================

#define	HUNK_SENTINAL	0x1df001ed

struct hunk_t
{
	int  sentinal;
	int  size; // including sizeof(hunk_t), -1 = not allocated
	char name[8];
};

byte* hunk_base;
int   hunk_size;

int hunk_low_used;
int hunk_high_used;

qboolean hunk_tempactive;
int      hunk_tempmark;


/*
==============
Hunk_Check

Run consistancy and sentinal trahing checks
==============
*/
void Hunk_Check()
{
	for (auto h = reinterpret_cast<hunk_t *>(hunk_base); reinterpret_cast<byte *>(h) != hunk_base + hunk_low_used;)
	{
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error("Hunk_Check: trahsed sentinal");
		if (h->size < 16 || h->size + reinterpret_cast<byte *>(h) - hunk_base > hunk_size)
			Sys_Error("Hunk_Check: bad size");
		h = reinterpret_cast<hunk_t *>(reinterpret_cast<byte *>(h) + h->size);
	}
}

/*
==============
Hunk_Print

If "all" is specified, every single allocation is printed.
Otherwise, allocations with the same name will be totaled up before printing.
==============
*/
void Hunk_Print(qboolean all)
{
	char name[9];

	name[8]          = 0;
	auto count       = 0;
	auto sum         = 0;
	auto totalblocks = 0;

	auto       h         = reinterpret_cast<hunk_t *>(hunk_base);
	const auto endlow    = reinterpret_cast<hunk_t *>(hunk_base + hunk_low_used);
	const auto starthigh = reinterpret_cast<hunk_t *>(hunk_base + hunk_size - hunk_high_used);
	const auto endhigh   = reinterpret_cast<hunk_t *>(hunk_base + hunk_size);

	Con_Printf("          :%8i total hunk size\n", hunk_size);
	Con_Printf("-------------------------\n");

	while (true)
	{
		//
		// skip to the high hunk if done with low hunk
		//
		if (h == endlow)
		{
			Con_Printf("-------------------------\n");
			Con_Printf("          :%8i REMAINING\n", hunk_size - hunk_low_used - hunk_high_used);
			Con_Printf("-------------------------\n");
			h = starthigh;
		}

		//
		// if totally done, break
		//
		if (h == endhigh)
			break;

		//
		// run consistancy checks
		//
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error("Hunk_Check: trahsed sentinal");
		if (h->size < 16 || h->size + reinterpret_cast<byte *>(h) - hunk_base > hunk_size)
			Sys_Error("Hunk_Check: bad size");

		auto next = reinterpret_cast<hunk_t *>(reinterpret_cast<byte *>(h) + h->size);
		count++;
		totalblocks++;
		sum += h->size;

		//
		// print the single block
		//
		memcpy(name, h->name, 8);
		if (all)
			Con_Printf("%8p :%8i %8s\n", h, h->size, name);

		//
		// print the total
		//
		if (next == endlow || next == endhigh ||
			strncmp(h->name, next->name, 8))
		{
			if (!all)
				Con_Printf("          :%8i %8s (TOTAL)\n", sum, name);
			count = 0;
			sum   = 0;
		}

		h = next;
	}

	Con_Printf("-------------------------\n");
	Con_Printf("%8i total blocks\n", totalblocks);
}

/*
===================
Hunk_AllocName
===================
*/
void* Hunk_AllocName(int size, char* name)
{
#ifdef PARANOID
	Hunk_Check();
#endif

	if (size < 0)
		Sys_Error("Hunk_Alloc: bad size: %i", size);

	size = sizeof(hunk_t) + (size + 15 & ~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size)
		Sys_Error("Hunk_Alloc: failed on %i bytes", size);

	auto h = reinterpret_cast<hunk_t *>(hunk_base + hunk_low_used);
	hunk_low_used += size;

	Cache_FreeLow(hunk_low_used);

	memset(h, 0, size);

	h->size     = size;
	h->sentinal = HUNK_SENTINAL;
	Q_strncpy(h->name, name, 8);

	return static_cast<void *>(h + 1);
}

/*
===================
Hunk_Alloc
===================
*/
void* Hunk_Alloc(int size)
{
	return Hunk_AllocName(size, "unknown");
}

int Hunk_LowMark()
{
	return hunk_low_used;
}

void Hunk_FreeToLowMark(int mark)
{
	if (mark < 0 || mark > hunk_low_used)
		Sys_Error("Hunk_FreeToLowMark: bad mark %i", mark);
	memset(hunk_base + mark, 0, hunk_low_used - mark);
	hunk_low_used = mark;
}

int Hunk_HighMark()
{
	if (hunk_tempactive)
	{
		hunk_tempactive = qfalse;
		Hunk_FreeToHighMark(hunk_tempmark);
	}

	return hunk_high_used;
}

void Hunk_FreeToHighMark(int mark)
{
	if (hunk_tempactive)
	{
		hunk_tempactive = qfalse;
		Hunk_FreeToHighMark(hunk_tempmark);
	}
	if (mark < 0 || mark > hunk_high_used)
		Sys_Error("Hunk_FreeToHighMark: bad mark %i", mark);
	memset(hunk_base + hunk_size - hunk_high_used, 0, hunk_high_used - mark);
	hunk_high_used = mark;
}


/*
===================
Hunk_HighAllocName
===================
*/
void* Hunk_HighAllocName(int size, char* name)
{
	if (size < 0)
		Sys_Error("Hunk_HighAllocName: bad size: %i", size);

	if (hunk_tempactive)
	{
		Hunk_FreeToHighMark(hunk_tempmark);
		hunk_tempactive = qfalse;
	}

#ifdef PARANOID
	Hunk_Check();
#endif

	size = sizeof(hunk_t) + (size + 15 & ~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size)
	{
		Con_Printf("Hunk_HighAlloc: failed on %i bytes\n", size);
		return nullptr;
	}

	hunk_high_used += size;
	Cache_FreeHigh(hunk_high_used);

	auto h = reinterpret_cast<hunk_t *>(hunk_base + hunk_size - hunk_high_used);

	memset(h, 0, size);
	h->size     = size;
	h->sentinal = HUNK_SENTINAL;
	Q_strncpy(h->name, name, 8);

	return static_cast<void *>(h + 1);
}


/*
=================
Hunk_TempAlloc

Return space from the top of the hunk
=================
*/
void* Hunk_TempAlloc(int size)
{
	size = size + 15 & ~15;

	if (hunk_tempactive)
	{
		Hunk_FreeToHighMark(hunk_tempmark);
		hunk_tempactive = qfalse;
	}

	hunk_tempmark = Hunk_HighMark();

	const auto buf = Hunk_HighAllocName(size, "temp");

	hunk_tempactive = qtrue;

	return buf;
}

/*
===============================================================================

CACHE MEMORY

===============================================================================
*/

struct cache_system_t
{
	int             size; // including this header
	cache_user_t*   user;
	char            name[16];
	cache_system_t *prev, *    next;
	cache_system_t *lru_prev, *lru_next; // for LRU flushing	
};

cache_system_t* Cache_TryAlloc(int size, qboolean nobottom);

cache_system_t cache_head;

/*
===========
Cache_Move
===========
*/
void Cache_Move(cache_system_t* c)
{
	// we are clearing up space at the bottom, so only allocate it late
	auto res = Cache_TryAlloc(c->size, qtrue);
	if (res)
	{
		//		Con_Printf ("cache_move ok\n");

		Q_memcpy(res + 1, c + 1, c->size - sizeof(cache_system_t));
		res->user = c->user;
		Q_memcpy(res->name, c->name, sizeof res->name);
		Cache_Free(c->user);
		res->user->data = static_cast<void *>(res + 1);
	}
	else
	{
		//		Con_Printf ("cache_move failed\n");

		Cache_Free(c->user); // tough luck...
	}
}

/*
============
Cache_FreeLow

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeLow(int new_low_hunk)
{
	while (true)
	{
		const auto c = cache_head.next;
		if (c == &cache_head)
			return; // nothing in cache at all
		if (reinterpret_cast<byte *>(c) >= hunk_base + new_low_hunk)
			return; // there is space to grow the hunk
		Cache_Move(c); // reclaim the space
	}
}

/*
============
Cache_FreeHigh

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeHigh(int new_high_hunk)
{
	cache_system_t* prev = nullptr;
	while (true)
	{
		const auto c = cache_head.prev;
		if (c == &cache_head)
			return; // nothing in cache at all
		if (reinterpret_cast<byte *>(c) + c->size <= hunk_base + hunk_size - new_high_hunk)
			return; // there is space to grow the hunk
		if (c == prev)
			Cache_Free(c->user); // didn't move out of the way
		else
		{
			Cache_Move(c); // try to move it
			prev = c;
		}
	}
}

void Cache_UnlinkLRU(cache_system_t* cs)
{
	if (!cs->lru_next || !cs->lru_prev)
		Sys_Error("Cache_UnlinkLRU: nullptr link");

	cs->lru_next->lru_prev = cs->lru_prev;
	cs->lru_prev->lru_next = cs->lru_next;

	cs->lru_prev = cs->lru_next = nullptr;
}

void Cache_MakeLRU(cache_system_t* cs)
{
	if (cs->lru_next || cs->lru_prev)
		Sys_Error("Cache_MakeLRU: active link");

	cache_head.lru_next->lru_prev = cs;
	cs->lru_next                  = cache_head.lru_next;
	cs->lru_prev                  = &cache_head;
	cache_head.lru_next           = cs;
}

/*
============
Cache_TryAlloc

Looks for a free block of memory between the high and low hunk marks
Size should already include the header and padding
============
*/
cache_system_t* Cache_TryAlloc(int size, qboolean nobottom)
{
	cache_system_t* res;

	// is the cache completely empty?

	if (!nobottom && cache_head.prev == &cache_head)
	{
		if (hunk_size - hunk_high_used - hunk_low_used < size)
			Sys_Error("Cache_TryAlloc: %i is greater then free hunk", size);

		res = reinterpret_cast<cache_system_t *>(hunk_base + hunk_low_used);
		memset(res, 0, sizeof*res);
		res->size = size;

		cache_head.prev = cache_head.next = res;
		res->prev       = res->next       = &cache_head;

		Cache_MakeLRU(res);
		return res;
	}

	// search from the bottom up for space

	res     = reinterpret_cast<cache_system_t *>(hunk_base + hunk_low_used);
	auto cs = cache_head.next;

	do
	{
		if (!nobottom || cs != cache_head.next)
		{
			if (reinterpret_cast<byte *>(cs) - reinterpret_cast<byte *>(res) >= size)
			{
				// found space
				memset(res, 0, sizeof*res);
				res->size = size;

				res->next      = cs;
				res->prev      = cs->prev;
				cs->prev->next = res;
				cs->prev       = res;

				Cache_MakeLRU(res);

				return res;
			}
		}

		// continue looking		
		res = reinterpret_cast<cache_system_t *>(reinterpret_cast<byte *>(cs) + cs->size);
		cs  = cs->next;
	}
	while (cs != &cache_head);

	// try to allocate one at the very end
	if (hunk_base + hunk_size - hunk_high_used - reinterpret_cast<byte *>(res) >= size)
	{
		memset(res, 0, sizeof*res);
		res->size = size;

		res->next             = &cache_head;
		res->prev             = cache_head.prev;
		cache_head.prev->next = res;
		cache_head.prev       = res;

		Cache_MakeLRU(res);

		return res;
	}

	return nullptr; // couldn't allocate
}

/*
============
Cache_Flush

Throw everything out, so new data will be demand cached
============
*/
void Cache_Flush()
{
	while (cache_head.next != &cache_head)
		Cache_Free(cache_head.next->user); // reclaim the space
}


/*
============
Cache_Print

============
*/
void Cache_Print()
{
	for (auto cd = cache_head.next; cd != &cache_head; cd = cd->next)
	{
		Con_Printf("%8i : %s\n", cd->size, cd->name);
	}
}

/*
============
Cache_Report

============
*/
void Cache_Report()
{
	Con_DPrintf("%4.1f megabyte data cache\n", (hunk_size - hunk_high_used - hunk_low_used) / static_cast<float>(1024 * 1024));
}

/*
============
Cache_Compact

============
*/
void Cache_Compact()
{
}

/*
============
Cache_Init

============
*/
void Cache_Init()
{
	cache_head.next     = cache_head.prev     = &cache_head;
	cache_head.lru_next = cache_head.lru_prev = &cache_head;

	Cmd_AddCommand("flush", Cache_Flush);
}

/*
==============
Cache_Free

Frees the memory and removes it from the LRU list
==============
*/
void Cache_Free(cache_user_t* c)
{
	if (!c->data)
		Sys_Error("Cache_Free: not allocated");

	const auto cs = static_cast<cache_system_t *>(c->data) - 1;

	cs->prev->next = cs->next;
	cs->next->prev = cs->prev;
	cs->next       = cs->prev = nullptr;

	c->data = nullptr;

	Cache_UnlinkLRU(cs);
}


/*
==============
Cache_Check
==============
*/
void* Cache_Check(cache_user_t* c)
{
	if (!c->data)
		return nullptr;

	const auto cs = static_cast<cache_system_t *>(c->data) - 1;

	// move to head of LRU
	Cache_UnlinkLRU(cs);
	Cache_MakeLRU(cs);

	return c->data;
}


/*
==============
Cache_Alloc
==============
*/
void* Cache_Alloc(cache_user_t* c, int size, char* name)
{
	if (c->data)
		Sys_Error("Cache_Alloc: allready allocated");

	if (size <= 0)
		Sys_Error("Cache_Alloc: size %i", size);

	size = size + sizeof(cache_system_t) + 15 & ~15;

	// find memory for it	
	while (true)
	{
		auto cs = Cache_TryAlloc(size, qfalse);
		if (cs)
		{
			strncpy(cs->name, name, sizeof cs->name - 1);
			c->data  = static_cast<void *>(cs + 1);
			cs->user = c;
			break;
		}

		// free the least recently used cahedat
		if (cache_head.lru_prev == &cache_head)
			Sys_Error("Cache_Alloc: out of memory");
		// not enough memory at all
		Cache_Free(cache_head.lru_prev->user);
	}

	return Cache_Check(c);
}

//============================================================================


/*
========================
Memory_Init
========================
*/
void Memory_Init(void* buf, int size)
{
	auto zonesize = DYNAMIC_SIZE;

	hunk_base      = static_cast<byte*>(buf);
	hunk_size      = size;
	hunk_low_used  = 0;
	hunk_high_used = 0;

	Cache_Init();
	const auto p = COM_CheckParm("-zone");
	if (p)
	{
		if (p < com_argc - 1)
			zonesize = Q_atoi(com_argv[p + 1]) * 1024;
		else
			Sys_Error("Memory_Init: you must specify a size in KB after -zone");
	}
	mainzone = static_cast<memzone_t*>(Hunk_AllocName(zonesize, "zone"));
	Z_ClearZone(mainzone, zonesize);
}


// this is a 16 bit, non-reflected CRC using the polynomial 0x1021
// and the initial and final xor values shown below...  in other words, the
// CCITT standard CRC used by XMODEM

#define CRC_INIT_VALUE	0xffff
#define CRC_XOR_VALUE	0x0000

static unsigned short crctable[256] =
{
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

void CRC_Init(unsigned short* crcvalue)
{
	*crcvalue = CRC_INIT_VALUE;
}

void CRC_ProcessByte(unsigned short* crcvalue, byte data)
{
	*crcvalue = *crcvalue << 8 ^ crctable[*crcvalue >> 8 ^ data];
}

unsigned short CRC_Value(unsigned short crcvalue)
{
	return crcvalue ^ CRC_XOR_VALUE;
}

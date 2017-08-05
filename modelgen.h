#pragma once
#define ALIAS_VERSION	6

#define ALIAS_ONSEAM				0x0020

// must match definition in spritegn.h
#ifndef SYNCTYPE_T
#define SYNCTYPE_T

enum class synctype_t
{
	ST_SYNC=0,
	ST_RAND
};
#endif

enum class aliasframetype_t
{
	ALIAS_SINGLE=0,
	ALIAS_GROUP
};

enum class aliasskintype_t
{
	ALIAS_SKIN_SINGLE=0,
	ALIAS_SKIN_GROUP
};

struct mdl_t
{
	int ident;
	int version;
	vec3_t scale;
	vec3_t scale_origin;
	float boundingradius;
	vec3_t eyeposition;
	int numskins;
	int skinwidth;
	int skinheight;
	int numverts;
	int numtris;
	int numframes;
	synctype_t synctype;
	int flags;
	float size;
};

// TODO: could be shorts

struct stvert_t
{
	int onseam;
	int s;
	int t;
};

struct dtriangle_t
{
	int facesfront;
	int vertindex[3];
};

#define DT_FACES_FRONT				0x0010

// This mirrors trivert_t in trilib.h, is present so Quake knows how to
// load this data

struct trivertx_t
{
	byte v[3];
	byte lightnormalindex;
};

struct daliasframe_t
{
	trivertx_t bboxmin; // lightnormal isn't used
	trivertx_t bboxmax; // lightnormal isn't used
	char name[16]; // frame name from grabbing
};

struct daliasgroup_t
{
	int numframes;
	trivertx_t bboxmin; // lightnormal isn't used
	trivertx_t bboxmax; // lightnormal isn't used
};

struct daliasskingroup_t
{
	int numskins;
};

struct daliasinterval_t
{
	float interval;
};

struct daliasskininterval_t
{
	float interval;
};

struct daliasframetype_t
{
	aliasframetype_t type;
};

struct daliasskintype_t
{
	aliasskintype_t type;
};

#define IDPOLYHEADER	(('O'<<24)+('P'<<16)+('D'<<8)+'I')
// little-endian "IDPO"

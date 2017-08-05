#pragma once
#define SPRITE_VERSION	1

// must match definition in modelgen.h
#ifndef SYNCTYPE_T
#define SYNCTYPE_T
enum class synctype_t
{ST_SYNC=0, ST_RAND } ;
#endif

// TODO: shorten these?
struct dsprite_t
{
	int ident;
	int version;
	int type;
	float boundingradius;
	int width;
	int height;
	int numframes;
	float beamlength;
	synctype_t synctype;
};

#define SPR_VP_PARALLEL_UPRIGHT		0
#define SPR_FACING_UPRIGHT			1
#define SPR_VP_PARALLEL				2
#define SPR_ORIENTED				3
#define SPR_VP_PARALLEL_ORIENTED	4

struct dspriteframe_t
{
	int origin[2];
	int width;
	int height;
};

struct dspritegroup_t
{
	int numframes;
};

struct dspriteinterval_t
{
	float interval;
};

enum class spriteframetype_t
{
	SPR_SINGLE=0,
	SPR_GROUP
};

struct dspriteframetype_t
{
	spriteframetype_t type;
};

#define IDSPRITEHEADER	(('P'<<24)+('S'<<16)+('D'<<8)+'I')
// little-endian "IDSP"

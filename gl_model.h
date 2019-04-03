#ifndef __MODEL__
#define __MODEL__

#include "modelgen.h"
#include "spritegn.h"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

// entity effects

#define	EF_BRIGHTFIELD			1
#define	EF_MUZZLEFLASH 			2
#define	EF_BRIGHTLIGHT 			4
#define	EF_DIMLIGHT 			8


/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//
// !!! if this is changed, it must be changed in asm_draw.h too !!!
struct mvertex_t
{
	vec3_t position;
};

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2


// plane_t structure
struct mplane_t
{
	vec3_t normal;
	float dist;
	byte type; // for texture axis selection and fast side tests
	byte signbits; // signx + signy<<1 + signz<<1
	byte pad[2];
};

struct msurface_t;

struct texture_t
{
	char name[16];
	unsigned width, height;
	int gl_texturenum;
	msurface_t* texturechain; // for gl_texsort drawing
	int anim_total; // total tenths in sequence ( 0 = no)
	int anim_min, anim_max; // time for this frame min <=time< max
	texture_t* anim_next; // in the animation sequence
	texture_t* alternate_anims; // bmodels in frmae 1 use these
	unsigned offsets[MIPLEVELS]; // four mip maps stored
};


#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_DRAWTILED		0x20
#define SURF_DRAWBACKGROUND	0x40
#define SURF_UNDERWATER		0x80

// !!! if this is changed, it must be changed in asm_draw.h too !!!
struct medge_t
{
	unsigned short v[2];
	unsigned int cachededgeoffset;
};

struct mtexinfo_t
{
	float vecs[2][4];
	float mipadjust;
	texture_t* texture;
	int flags;
};

#define	VERTEXSIZE	7

struct glpoly_t
{
	glpoly_t* next;
	glpoly_t* chain;
	int numverts;
	int flags; // for SURF_UNDERWATER
	float verts[4][VERTEXSIZE]; // variable sized (xyz s1t1 s2t2)
};

struct msurface_t
{
	int visframe; // should be drawn when node is crossed

	mplane_t* plane;
	int flags;

	int firstedge; // look up in model->surfedges[], negative numbers
	int numedges; // are backwards edges

	short texturemins[2];
	short extents[2];

	int light_s, light_t; // gl lightmap coordinates

	glpoly_t* polys; // multiple if warped
	msurface_t* texturechain;

	mtexinfo_t* texinfo;

	// lighting info
	int dlightframe;
	int dlightbits;

	int lightmaptexturenum;
	byte styles[MAXLIGHTMAPS];
	int cached_light[MAXLIGHTMAPS]; // values currently used in lightmap
	qboolean cached_dlight; // qtrue if dynamic light in cache
	byte* samples; // [numstyles*surfsize]
};

struct mnode_t
{
	// common with leaf
	int contents; // 0, to differentiate from leafs
	int visframe; // node needs to be traversed if current

	float minmaxs[6]; // for bounding box culling

	mnode_t* parent;

	// node specific
	mplane_t* plane;
	mnode_t* children[2];

	unsigned short firstsurface;
	unsigned short numsurfaces;
};


struct mleaf_t
{
	// common with node
	int contents; // wil be a negative contents number
	int visframe; // node needs to be traversed if current

	float minmaxs[6]; // for bounding box culling

	mnode_t* parent;

	// leaf specific
	byte* compressed_vis;
	efrag_t* efrags;

	msurface_t** firstmarksurface;
	int nummarksurfaces;
	int key; // BSP sequence number for leaf's contents
	byte ambient_sound_level[NUM_AMBIENTS];
};

struct hull_t
{
	dclipnode_t* clipnodes;
	mplane_t* planes;
	int firstclipnode;
	int lastclipnode;
	vec3_t clip_mins;
	vec3_t clip_maxs;
};

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/


// FIXME: shorten these?
struct mspriteframe_t
{
	int width;
	int height;
	float up, down, left, right;
	int gl_texturenum;
};

struct mspritegroup_t
{
	int numframes;
	float* intervals;
	mspriteframe_t* frames[1];
};

struct mspriteframedesc_t
{
	spriteframetype_t type;
	mspriteframe_t* frameptr;
};

struct msprite_t
{
	int type;
	int maxwidth;
	int maxheight;
	int numframes;
	float beamlength; // remove?
	void* cachespot; // remove?
	mspriteframedesc_t frames[1];
};


/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

struct maliasframedesc_t
{
	int firstpose;
	int numposes;
	float interval;
	trivertx_t bboxmin;
	trivertx_t bboxmax;
	int frame;
	char name[16];
};

struct maliasgroupframedesc_t
{
	trivertx_t bboxmin;
	trivertx_t bboxmax;
	int frame;
};

struct maliasgroup_t
{
	int numframes;
	int intervals;
	maliasgroupframedesc_t frames[1];
};

// !!! if this is changed, it must be changed in asm_draw.h too !!!
struct mtriangle_t
{
	int facesfront;
	int vertindex[3];
};


#define	MAX_SKINS	32

struct aliashdr_t
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

	int numposes;
	int poseverts;
	int posedata; // numposes*poseverts trivert_t
	int commands; // gl command list with embedded s/t
	int gl_texturenum[MAX_SKINS][4];
	int texels[MAX_SKINS]; // only for player skins
	maliasframedesc_t frames[1]; // variable sized
};

#define	MAXALIASVERTS	1024
#define	MAXALIASFRAMES	256
#define	MAXALIASTRIS	2048
extern aliashdr_t* pheader;
extern stvert_t stverts[MAXALIASVERTS];
extern mtriangle_t triangles[MAXALIASTRIS];
extern trivertx_t* poseverts[MAXALIASFRAMES];

//===================================================================

//
// Whole model
//

enum class modtype_t
{
	mod_brush,
	mod_sprite,
	mod_alias
};

#define	EF_ROCKET	1			// leave a trail
#define	EF_GRENADE	2			// leave a trail
#define	EF_GIB		4			// leave a trail
#define	EF_ROTATE	8			// rotate (bonus items)
#define	EF_TRACER	16			// green split trail
#define	EF_ZOMGIB	32			// small blood trail
#define	EF_TRACER2	64			// orange split trail + rotate
#define	EF_TRACER3	128			// purple trail

struct model_t
{
	char name[MAX_QPATH];
	qboolean needload; // bmodels and sprites don't cache normally

	modtype_t type;
	int numframes;
	synctype_t synctype;

	int flags;

	//
	// volume occupied by the model graphics
	//		
	vec3_t mins, maxs;
	float radius;

	//
	// solid volume for clipping 
	//
	qboolean clipbox;
	vec3_t clipmins, clipmaxs;

	//
	// brush model
	//
	int firstmodelsurface, nummodelsurfaces;

	int numsubmodels;
	dmodel_t* submodels;

	int numplanes;
	mplane_t* planes;

	int numleafs; // number of visible leafs, not counting 0
	mleaf_t* leafs;

	int numvertexes;
	mvertex_t* vertexes;

	int numedges;
	medge_t* edges;

	int numnodes;
	mnode_t* nodes;

	int numtexinfo;
	mtexinfo_t* texinfo;

	int numsurfaces;
	msurface_t* surfaces;

	int numsurfedges;
	int* surfedges;

	int numclipnodes;
	dclipnode_t* clipnodes;

	int nummarksurfaces;
	msurface_t** marksurfaces;

	hull_t hulls[MAX_MAP_HULLS];

	int numtextures;
	texture_t** textures;

	byte* visdata;
	byte* lightdata;
	char* entities;

	//
	// additional model data
	//
	cache_user_t cache; // only access through Mod_Extradata
};

//============================================================================

void Mod_Init();
void Mod_ClearAll();
model_t* Mod_ForName(char* name, qboolean crash);
void* Mod_Extradata(model_t* mod); // handles caching
void Mod_TouchModel(char* name);
byte* Mod_LeafPVS(mleaf_t* leaf, model_t* model);
mleaf_t* Mod_PointInLeaf(vec3_t p, model_t* model);

#endif	// __MODEL__

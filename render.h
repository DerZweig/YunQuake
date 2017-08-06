#pragma once
#define	MAXCLIPPLANES	11

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

struct dlight_t;
//=============================================================================
struct mleaf_t;
struct entity_t;
struct model_t;
struct mnode_t;

struct efrag_t
{
	mleaf_t* leaf;
	efrag_t* leafnext;
	entity_t* entity;
	efrag_t* entnext;
};


struct entity_t
{
	qboolean forcelink; // model changed

	int update_type;

	entity_state_t baseline; // to fill in defaults in updates

	double msgtime; // time of last update
	vec3_t msg_origins[2]; // last two updates (0 is newest)	
	vec3_t origin;
	vec3_t msg_angles[2]; // last two updates (0 is newest)
	vec3_t angles;
	model_t* model; // nullptr = no model
	efrag_t* efrag; // linked list of efrags
	int frame;
	float syncbase; // for client-side animations
	byte* colormap;
	int effects; // light, particals, etc
	int skinnum; // for Alias models
	int visframe; // last frame this entity was
	//  found in an active leaf

	int dlightframe; // dynamic lighting
	int dlightbits;

	// FIXME: could turn these into a union
	int trivial_accept;
	mnode_t* topnode; // for bmodels, first world node
	//  that splits bmodel, or nullptr if
	//  not split
};

// !!! if this is changed, it must be changed in asm_draw.h too !!!
struct refdef_t
{
	vrect_t vrect; // subwindow in video for refresh
	// FIXME: not need vrect next field here?
	vrect_t aliasvrect; // scaled Alias version
	int vrectright, vrectbottom; // right & bottom screen coords
	int aliasvrectright, aliasvrectbottom; // scaled Alias versions
	float vrectrightedge; // rightmost right edge we care about,
	//  for use in edge list
	float fvrectx, fvrecty; // for floating-point compares
	float fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	int vrect_x_adj_shift20; // (vrect.x + 0.5 - epsilon) << 20
	int vrectright_adj_shift20; // (vrectright + 0.5 - epsilon) << 20
	float fvrectright_adj, fvrectbottom_adj;
	// right and bottom edges, for clamping
	float fvrectright; // rightmost edge, for Alias clamping
	float fvrectbottom; // bottommost edge, for Alias clamping
	float horizontalFieldOfView; // at Z = 1.0, this many X is visible 
	// 2.0 = 90 degrees
	float xOrigin; // should probably allways be 0.5
	float yOrigin; // between be around 0.3 to 0.5

	vec3_t vieworg;
	vec3_t viewangles;

	float fov_x, fov_y;

	int ambientlight;
};


//
// refresh
//
extern int reinit_surfcache;
struct texture_t;

extern refdef_t r_refdef;
extern vec3_t r_origin, vpn, vright, vup;

extern texture_t* r_notexture_mip;

struct msurface_t;

void R_Init(void);
void R_InitTextures(void);
void R_InitEfrags(void);
void R_RenderView(void); // must set r_refdef first
void R_ViewChanged(vrect_t* pvrect, int lineadj, float aspect);
// called whenever r_refdef or vid change
void R_InitSky(texture_t * mt); // called at level load

void R_AddEfrags(entity_t* ent);
void R_RemoveEfrags(entity_t* ent);

void R_NewMap(void);


void R_ParseParticleEffect(void);
void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count);
void R_RocketTrail(vec3_t start, vec3_t end, int type);

void R_EntityParticles(entity_t* ent);
void R_BlobExplosion(vec3_t org);
void R_ParticleExplosion(vec3_t org);
void R_ParticleExplosion2(vec3_t org, int colorStart, int colorLength);
void R_LavaSplash(vec3_t org);
void R_TeleportSplash(vec3_t org);

void R_PushDlights(void);
void R_DrawSkyChain(msurface_t *s);

void R_InitParticles(void);
void R_DrawParticles(void);
void R_ClearParticles(void);

void R_RenderDlights(void);
int R_LightPoint(vec3_t p);
void R_MarkLights(dlight_t* light, int bit, mnode_t* node);
void R_AnimateLight(void);

void R_DrawWorld(void);
void R_MarkLeaves(void);
void R_DrawWaterSurfaces(void);
void R_DrawBrushModel(entity_t *e);
void R_RenderBrushPoly(msurface_t *fa);
qboolean R_CullBox(vec3_t mins, vec3_t maxs);
void R_RotateForEntity(entity_t* e);
void R_StoreEfrags(efrag_t** ppefrag);

void R_FreeTextures(void);

//
// surface cache related
//
extern int reinit_surfcache; // if 1, surface cache is currently empty and
extern qboolean r_cache_thrash; // set if thrashing the surface cache

int D_SurfaceCacheForRes(int width, int height);
void D_DeleteSurfaceCache(void);
void D_InitCaches(void* buffer, int size);
void R_SetVrect(vrect_t* pvrect, vrect_t* pvrectin, int lineadj);

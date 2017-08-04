#pragma once
#define	MAXCLIPPLANES	11

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

struct mleaf_t;
struct entity_t;
struct mnode_t;
struct model_t;

struct efrag_t
{
	mleaf_t* leaf;
	efrag_t* leafnext;
	entity_t* entity;
	efrag_t* entnext;
};

//johnfitz -- for lerping
#define LERP_MOVESTEP	(1<<0) //this is a MOVETYPE_STEP entity, enable movement lerp
#define LERP_RESETANIM	(1<<1) //disable anim lerping until next anim frame
#define LERP_RESETANIM2	(1<<2) //set this and previous flag to disable anim lerping for two anim frames
#define LERP_RESETMOVE	(1<<3) //disable movement lerping until next origin/angles change
#define LERP_FINISH		(1<<4) //use lerpfinish time from server update instead of assuming interval of 0.1

//johnfitz

struct entity_t
{
	bool forcelink; // model changed

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
	int effects; // light, particles, etc
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

	byte alpha; //johnfitz -- alpha
	byte lerpflags; //johnfitz -- lerping
	float lerpstart; //johnfitz -- animation lerping
	float lerptime; //johnfitz -- animation lerping
	float lerpfinish; //johnfitz -- lerping -- server sent us a more accurate interval, use it instead of 0.1
	short previouspose; //johnfitz -- animation lerping
	short currentpose; //johnfitz -- animation lerping
	//	short					futurepose;		//johnfitz -- animation lerping
	float movelerpstart; //johnfitz -- transform lerping
	vec3_t previousorigin; //johnfitz -- transform lerping
	vec3_t currentorigin; //johnfitz -- transform lerping
	vec3_t previousangles; //johnfitz -- transform lerping
	vec3_t currentangles; //johnfitz -- transform lerping
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


extern refdef_t r_refdef;
extern vec3_t r_origin, vpn, vright, vup;


void R_Init();
void R_InitTextures();
void R_InitEfrags();
void R_RenderView(); // must set r_refdef first
void R_ViewChanged(vrect_t* pvrect, int lineadj, float aspect);
// called whenever r_refdef or vid change
struct texture_t;
void R_InitSky(texture_t* mt); // called at level load

void R_CheckEfrags(); //johnfitz
void R_AddEfrags(entity_t* ent);
void R_RemoveEfrags(entity_t* ent);

void R_NewMap();


void R_ParseParticleEffect();
void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count);
void R_RocketTrail(vec3_t start, vec3_t end, int type);
void R_EntityParticles(entity_t* ent);
void R_BlobExplosion(vec3_t org);
void R_ParticleExplosion(vec3_t org);
void R_ParticleExplosion2(vec3_t org, int colorStart, int colorLength);
void R_LavaSplash(vec3_t org);
void R_TeleportSplash(vec3_t org);

void R_PushDlights();


//
// surface cache related
//
extern int reinit_surfcache; // if 1, surface cache is currently empty and
extern bool r_cache_thrash; // set if thrashing the surface cache

int D_SurfaceCacheForRes(int width, int height);
void D_FlushCaches();
void D_DeleteSurfaceCache();
void D_InitCaches(void* buffer, int size);
void R_SetVrect(vrect_t* pvrect, vrect_t* pvrectin, int lineadj);

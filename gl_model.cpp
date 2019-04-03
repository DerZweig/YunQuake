#include "quakedef.h"

model_t* loadmodel;
char     loadname[32]; // for hunk tags

void     Mod_LoadSpriteModel(model_t* mod, void*    buffer);
void     Mod_LoadBrushModel(model_t*  mod, void*    buffer);
void     Mod_LoadAliasModel(model_t*  mod, void*    buffer);
model_t* Mod_LoadModel(model_t*       mod, qboolean crash);

byte mod_novis[MAX_MAP_LEAFS / 8];

#define	MAX_MOD_KNOWN	512
model_t mod_known[MAX_MOD_KNOWN];
int     mod_numknown;

cvar_t gl_subdivide_size = {"gl_subdivide_size", "128", qtrue};

/*
===============
Mod_Init
===============
*/
void Mod_Init()
{
	Cvar_RegisterVariable(&gl_subdivide_size);
	memset(mod_novis, 0xff, sizeof mod_novis);
}

/*
===============
Mod_Init

Caches the data if needed
===============
*/
void* Mod_Extradata(model_t* mod)
{
	const auto r = Cache_Check(&mod->cache);
	if (r)
		return r;

	Mod_LoadModel(mod, qtrue);

	if (!mod->cache.data)
		Sys_Error("Mod_Extradata: caching failed");
	return mod->cache.data;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t* Mod_PointInLeaf(vec3_t p, model_t* model)
{
	if (!model || !model->nodes)
		Sys_Error("Mod_PointInLeaf: bad model");

	auto node = model->nodes;
	while (true)
	{
		if (node->contents < 0)
			return reinterpret_cast<mleaf_t *>(node);
		auto       plane = node->plane;
		const auto d     = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
}


/*
===================
Mod_DecompressVis
===================
*/
byte* Mod_DecompressVis(byte* in, model_t* model)
{
	static byte decompressed[MAX_MAP_LEAFS / 8];

	auto row = model->numleafs + 7 >> 3;
	auto out = decompressed;

	if (!in)
	{
		// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		int c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	}
	while (out - decompressed < row);

	return decompressed;
}

byte* Mod_LeafPVS(mleaf_t* leaf, model_t* model)
{
	if (leaf == model->leafs)
		return mod_novis;
	return Mod_DecompressVis(leaf->compressed_vis, model);
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll()
{
	int      i;
	model_t* mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (mod->type != modtype_t::mod_alias)
			mod->needload = qtrue;
}

/*
==================
Mod_FindName

==================
*/
model_t* Mod_FindName(char* name)
{
	int      i;
	model_t* mod;

	if (!name[0])
		Sys_Error("Mod_ForName: nullptr name");

	//
	// search the currently loaded models
	//
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (!strcmp(mod->name, name))
			break;

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error("mod_numknown == MAX_MOD_KNOWN");
		strcpy(mod->name, name);
		mod->needload = qtrue;
		mod_numknown++;
	}

	return mod;
}

/*
==================
Mod_TouchModel

==================
*/
void Mod_TouchModel(char* name)
{
	auto mod = Mod_FindName(name);

	if (!mod->needload)
	{
		if (mod->type == modtype_t::mod_alias)
			Cache_Check(&mod->cache);
	}
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t* Mod_LoadModel(model_t* mod, qboolean crash)
{
	byte stackbuf[1024]; // avoid dirtying the cache heap

	if (!mod->needload)
	{
		if (mod->type == modtype_t::mod_alias)
		{
			const auto d = Cache_Check(&mod->cache);
			if (d)
				return mod;
		}
		else
			return mod; // not cached at all
	}

	//
	// because the world is so huge, load it one piece at a time
	//
	if (!crash)
	{
	}

	//
	// load the file
	//
	const auto buf = reinterpret_cast<unsigned *>(COM_LoadStackFile(mod->name, stackbuf, sizeof stackbuf));
	if (!buf)
	{
		if (crash)
			Sys_Error("Mod_NumForName: %s not found", mod->name);
		return nullptr;
	}

	//
	// allocate a new model
	//
	COM_FileBase(mod->name, loadname);

	loadmodel = mod;

	//
	// fill it in
	//

	// call the apropriate loader
	mod->needload = qfalse;

	switch (LittleLong(*static_cast<unsigned *>(buf)))
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel(mod, buf);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel(mod, buf);
		break;

	default:
		Mod_LoadBrushModel(mod, buf);
		break;
	}

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t* Mod_ForName(char* name, qboolean crash)
{
	const auto mod = Mod_FindName(name);
	return Mod_LoadModel(mod, crash);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

byte* mod_base;


/*
=================
Mod_LoadTextures
=================
*/
void Mod_LoadTextures(lump_t* l)
{
	int        i;
	int        j;
	texture_t* tx;
	texture_t* tx2;
	texture_t* anims[10];
	texture_t* altanims[10];

	if (!l->filelen)
	{
		loadmodel->textures = nullptr;
		return;
	}
	auto m = reinterpret_cast<dmiptexlump_t *>(mod_base + l->fileofs);

	m->nummiptex = LittleLong(m->nummiptex);

	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures    = static_cast<texture_t**>(Hunk_AllocName(m->nummiptex * sizeof(texture_t*), loadname));

	for (i = 0; i < m->nummiptex; i++)
	{
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;
		auto mt            = reinterpret_cast<miptex_t *>(reinterpret_cast<byte *>(m) + m->dataofs[i]);
		mt->width          = LittleLong(mt->width);
		mt->height         = LittleLong(mt->height);
		for (j             = 0; j < MIPLEVELS; j++)
			mt->offsets[j] = LittleLong(mt->offsets[j]);

		if (mt->width & 15 || mt->height & 15)
			Sys_Error("Texture %s is not 16 aligned", mt->name);
		const int pixels       = mt->width * mt->height / 64 * 85;
		tx                     = static_cast<texture_t*>(Hunk_AllocName(sizeof(texture_t) + pixels, loadname));
		loadmodel->textures[i] = tx;

		memcpy(tx->name, mt->name, sizeof tx->name);
		tx->width          = mt->width;
		tx->height         = mt->height;
		for (j             = 0; j < MIPLEVELS; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
		// the pixels immediately follow the structures
		memcpy(tx + 1, mt + 1, pixels);


		if (!Q_strncmp(mt->name, "sky", 3))
			R_InitSky(tx);
		else
		{
			texture_mode      = GL_LINEAR_MIPMAP_NEAREST; //_LINEAR;
			tx->gl_texturenum = GL_LoadTexture(mt->name, tx->width, tx->height, reinterpret_cast<byte *>(tx + 1), qtrue, qfalse);
			texture_mode      = GL_LINEAR;
		}
	}

	//
	// sequence the animations
	//
	for (i = 0; i < m->nummiptex; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue; // allready sequenced

		// find the number of frames in the animation
		memset(anims, 0, sizeof anims);
		memset(altanims, 0, sizeof altanims);

		int  max    = tx->name[1];
		auto altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax     = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax           = max - 'A';
			max              = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
			Sys_Error("Bad animating texture %s", tx->name);

		for (j = i + 1; j < m->nummiptex; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp(tx2->name + 2, tx->name + 2))
				continue;

			int num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num + 1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num           = num - 'A';
				altanims[num] = tx2;
				if (num + 1 > altmax)
					altmax = num + 1;
			}
			else
				Sys_Error("Bad animating texture %s", tx->name);
		}

#define	ANIM_CYCLE	2
		// link them all together
		for (j = 0; j < max; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Sys_Error("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min   = j * ANIM_CYCLE;
			tx2->anim_max   = (j + 1) * ANIM_CYCLE;
			tx2->anim_next  = reinterpret_cast<texture_t*>(anims[(j + 1) % max]);
			if (altmax)
				tx2->alternate_anims = reinterpret_cast<texture_t*>(altanims[0]);
		}
		for (j = 0; j < altmax; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min   = j * ANIM_CYCLE;
			tx2->anim_max   = (j + 1) * ANIM_CYCLE;
			tx2->anim_next  = reinterpret_cast<texture_t*>(altanims[(j + 1) % altmax]);
			if (max)
				tx2->alternate_anims = reinterpret_cast<texture_t*>(anims[0]);
		}
	}
}

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting(lump_t* l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = nullptr;
		return;
	}
	loadmodel->lightdata = static_cast<byte*>(Hunk_AllocName(l->filelen, loadname));
	memcpy(loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility(lump_t* l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = nullptr;
		return;
	}
	loadmodel->visdata = static_cast<byte*>(Hunk_AllocName(l->filelen, loadname));
	memcpy(loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities(lump_t* l)
{
	if (!l->filelen)
	{
		loadmodel->entities = nullptr;
		return;
	}
	loadmodel->entities = static_cast<char*>(Hunk_AllocName(l->filelen, loadname));
	memcpy(loadmodel->entities, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes(lump_t* l)
{
	auto in = reinterpret_cast<dvertex_t *>(mod_base + l->fileofs);
	if (l->filelen % sizeof(dvertex_t))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	const int count = l->filelen / sizeof(dvertex_t);
	auto      out   = static_cast<mvertex_t *>(Hunk_AllocName(count * sizeof(mvertex_t), loadname));

	loadmodel->vertexes    = out;
	loadmodel->numvertexes = count;

	for (auto i = 0; i < count; i++, in++, out++)
	{
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels(lump_t* l)
{
	int j;

	auto in = reinterpret_cast<dmodel_t *>(mod_base + l->fileofs);
	if (l->filelen % sizeof(dmodel_t))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	const int count = l->filelen / sizeof(dmodel_t);
	auto      out   = static_cast<dmodel_t*>(Hunk_AllocName(count * sizeof(dmodel_t), loadname));

	loadmodel->submodels    = out;
	loadmodel->numsubmodels = count;

	for (auto i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			// spread the mins / maxs by a pixel
			out->mins[j]   = LittleFloat(in->mins[j]) - 1;
			out->maxs[j]   = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}
		for (j               = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = LittleLong(in->headnode[j]);
		out->visleafs        = LittleLong(in->visleafs);
		out->firstface       = LittleLong(in->firstface);
		out->numfaces        = LittleLong(in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges(lump_t* l)
{
	auto in = reinterpret_cast<dedge_t *>(mod_base + l->fileofs);
	if (l->filelen % sizeof(dedge_t))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	const int count = l->filelen / sizeof(dedge_t);
	auto      out   = static_cast<medge_t*>(Hunk_AllocName((count + 1) * sizeof(medge_t), loadname));

	loadmodel->edges    = out;
	loadmodel->numedges = count;

	for (auto i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = static_cast<unsigned short>(LittleShort(in->v[0]));
		out->v[1] = static_cast<unsigned short>(LittleShort(in->v[1]));
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo(lump_t* l)
{
	auto in = reinterpret_cast<texinfo_t *>(mod_base + l->fileofs);
	if (l->filelen % sizeof(texinfo_t))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	const int count = l->filelen / sizeof(texinfo_t);
	auto      out   = static_cast<mtexinfo_t*>(Hunk_AllocName(count * sizeof(mtexinfo_t), loadname));

	loadmodel->texinfo    = out;
	loadmodel->numtexinfo = count;

	for (auto i = 0; i < count; i++, in++, out++)
	{
		for (auto j         = 0; j < 8; j++)
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);
		auto       len1     = Length(out->vecs[0]);
		const auto len2     = Length(out->vecs[1]);
		len1                = (len1 + len2) / 2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;

		const auto miptex = LittleLong(in->miptex);
		out->flags        = LittleLong(in->flags);

		if (!loadmodel->textures)
		{
			out->texture = r_notexture_mip; // checkerboard texture
			out->flags   = 0;
		}
		else
		{
			if (miptex >= loadmodel->numtextures)
				Sys_Error("miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip; // texture not found
				out->flags   = 0;
			}
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents(msurface_t* s)
{
	float      mins[2], maxs[2];
	int        i;
	mvertex_t* v;
	int        bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	auto tex = s->texinfo;

	for (i = 0; i < s->numedges; i++)
	{
		const auto e = loadmodel->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (auto j = 0; j < 2; j++)
		{
			const auto val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++)
	{
		bmins[i] = floor(mins[i] / 16);
		bmaxs[i] = ceil(maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i]     = (bmaxs[i] - bmins[i]) * 16;
		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 /* 256 */)
			Sys_Error("Bad surface extents");
	}
}


/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces(lump_t* l)
{
	int i;

	auto in = reinterpret_cast<dface_t *>(mod_base + l->fileofs);
	if (l->filelen % sizeof(dface_t))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	const int count = l->filelen / sizeof(dface_t);
	auto      out   = static_cast<msurface_t*>(Hunk_AllocName(count * sizeof(msurface_t), loadname));

	loadmodel->surfaces    = out;
	loadmodel->numsurfaces = count;

	for (auto surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges  = LittleShort(in->numedges);
		out->flags     = 0;

		const int planenum = LittleShort(in->planenum);
		const int side     = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + LittleShort(in->texinfo);

		CalcSurfaceExtents(out);

		// lighting info

		for (i             = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i                  = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = nullptr;
		else
			out->samples = loadmodel->lightdata + i;

		// set the drawing flags flag

		if (!Q_strncmp(out->texinfo->texture->name, "sky", 3)) // sky
		{
			out->flags |= SURF_DRAWSKY | SURF_DRAWTILED;
			continue;
		}

		if (!Q_strncmp(out->texinfo->texture->name, "*", 1)) // turbulent
		{
			out->flags |= SURF_DRAWTURB | SURF_DRAWTILED;
			for (i = 0; i < 2; i++)
			{
				out->extents[i]     = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface(out); // cut up polygon for warps
		}
	}
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent(mnode_t* node, mnode_t* parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent(node->children[0], node);
	Mod_SetParent(node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes(lump_t* l)
{
	int j;

	auto in = reinterpret_cast<dnode_t *>(mod_base + l->fileofs);
	if (l->filelen % sizeof(dnode_t))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	const int count = l->filelen / sizeof(dnode_t);
	auto      out   = static_cast<mnode_t*>(Hunk_AllocName(count * sizeof(mnode_t), loadname));

	loadmodel->nodes    = out;
	loadmodel->numnodes = count;

	for (auto i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j]     = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		auto p     = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces  = LittleShort(in->numfaces);

		for (j = 0; j < 2; j++)
		{
			p = LittleShort(in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = reinterpret_cast<mnode_t *>(loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent(loadmodel->nodes, nullptr); // sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs(lump_t* l)
{
	int j;

	auto in = reinterpret_cast<dleaf_t *>(mod_base + l->fileofs);
	if (l->filelen % sizeof(dleaf_t))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	const int count = l->filelen / sizeof*in;
	auto      out   = static_cast<mleaf_t*>(Hunk_AllocName(count * sizeof(mleaf_t), loadname));

	loadmodel->leafs    = out;
	loadmodel->numleafs = count;

	for (auto i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j]     = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		auto p        = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = nullptr;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags             = nullptr;

		for (j                          = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp
		if (out->contents != CONTENTS_EMPTY)
		{
			for (j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}
}

/*
=================
Mod_LoadClipnodes
=================
*/
void Mod_LoadClipnodes(lump_t* l)
{
	auto in = reinterpret_cast<dclipnode_t *>(mod_base + l->fileofs);
	if (l->filelen % sizeof(dclipnode_t))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	const int count = l->filelen / sizeof*in;
	auto      out   = static_cast<dclipnode_t*>(Hunk_AllocName(count * sizeof(dclipnode_t), loadname));

	loadmodel->clipnodes    = out;
	loadmodel->numclipnodes = count;

	auto hull           = &loadmodel->hulls[1];
	hull->clipnodes     = out;
	hull->firstclipnode = 0;
	hull->lastclipnode  = count - 1;
	hull->planes        = loadmodel->planes;
	hull->clip_mins[0]  = -16;
	hull->clip_mins[1]  = -16;
	hull->clip_mins[2]  = -24;
	hull->clip_maxs[0]  = 16;
	hull->clip_maxs[1]  = 16;
	hull->clip_maxs[2]  = 32;

	hull                = &loadmodel->hulls[2];
	hull->clipnodes     = out;
	hull->firstclipnode = 0;
	hull->lastclipnode  = count - 1;
	hull->planes        = loadmodel->planes;
	hull->clip_mins[0]  = -32;
	hull->clip_mins[1]  = -32;
	hull->clip_mins[2]  = -24;
	hull->clip_maxs[0]  = 32;
	hull->clip_maxs[1]  = 32;
	hull->clip_maxs[2]  = 64;

	for (auto i = 0; i < count; i++, out++, in++)
	{
		out->planenum    = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}
}

/*
=================
Mod_MakeHull0

Deplicate the drawing hull structure as a clipping hull
=================
*/
void Mod_MakeHull0()
{
	const auto hull  = &loadmodel->hulls[0];
	auto       in    = loadmodel->nodes;
	const auto count = loadmodel->numnodes;
	auto       out   = static_cast<dclipnode_t*>(Hunk_AllocName(count * sizeof(dclipnode_t), loadname));

	hull->clipnodes     = out;
	hull->firstclipnode = 0;
	hull->lastclipnode  = count - 1;
	hull->planes        = loadmodel->planes;

	for (auto i = 0; i < count; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (auto j   = 0; j < 2; j++)
		{
			const auto child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces(lump_t* l)
{
	const auto in = reinterpret_cast<short *>(mod_base + l->fileofs);
	if (l->filelen % sizeof(short))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	const int  count = l->filelen / sizeof(short);
	const auto out   = static_cast<msurface_t **>(Hunk_AllocName(count * sizeof(msurface_t*), loadname));

	loadmodel->marksurfaces    = out;
	loadmodel->nummarksurfaces = count;

	for (auto i = 0; i < count; i++)
	{
		const int j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			Sys_Error("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges(lump_t* l)
{
	const auto in = reinterpret_cast<int *>(mod_base + l->fileofs);
	if (l->filelen % sizeof(int))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	const int  count = l->filelen / sizeof(int);
	const auto out   = static_cast<int*>(Hunk_AllocName(count * sizeof(int), loadname));

	loadmodel->surfedges    = out;
	loadmodel->numsurfedges = count;

	for (auto i = 0; i < count; i++)
		out[i]  = LittleLong(in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes(lump_t* l)
{
	auto in = reinterpret_cast<dplane_t *>(mod_base + l->fileofs);
	if (l->filelen % sizeof(dplane_t))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	const int count = l->filelen / sizeof(dplane_t);
	auto      out   = static_cast<mplane_t*>(Hunk_AllocName(count * 2 * sizeof(mplane_t), loadname));

	loadmodel->planes    = out;
	loadmodel->numplanes = count;

	for (auto i = 0; i < count; i++, in++, out++)
	{
		auto      bits = 0;
		for (auto j    = 0; j < 3; j++)
		{
			out->normal[j] = LittleFloat(in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist     = LittleFloat(in->dist);
		out->type     = LittleLong(in->type);
		out->signbits = bits;
	}
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds(vec3_t mins, vec3_t maxs)
{
	vec3_t corner;

	for (auto i = 0; i < 3; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return Length(corner);
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel(model_t* mod, void* buffer)
{
	dmodel_t* bm;

	loadmodel->type = modtype_t::mod_brush;

	auto header = static_cast<dheader_t *>(buffer);

	auto i = LittleLong(header->version);
	if (i != BSPVERSION)
		Sys_Error("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

	// swap all the lumps
	mod_base = reinterpret_cast<byte *>(header);

	for (i = 0; i < sizeof(dheader_t) / 4; i++)
	{
		reinterpret_cast<int *>(header)[i] = LittleLong(reinterpret_cast<int *>(header)[i]);
	}

	// load into heap

	Mod_LoadVertexes(&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges(&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures(&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting(&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes(&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces(&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces(&header->lumps[LUMP_MARKSURFACES]);
	Mod_LoadVisibility(&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs(&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes(&header->lumps[LUMP_NODES]);
	Mod_LoadClipnodes(&header->lumps[LUMP_CLIPNODES]);
	Mod_LoadEntities(&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels(&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0();

	mod->numframes = 2; // regular and alternate animation

	//
	// set up the submodels (FIXME: this is confusing)
	//
	for (i = 0; i < mod->numsubmodels; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (auto j                 = 1; j < MAX_MAP_HULLS; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode  = mod->numclipnodes - 1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces  = bm->numfaces;

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels - 1)
		{
			// duplicate the basic information
			char name[10];

			sprintf(name, "*%i", i + 1);
			loadmodel  = Mod_FindName(name);
			*loadmodel = *mod;
			strcpy(loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

aliashdr_t* pheader;

stvert_t    stverts[MAXALIASVERTS];
mtriangle_t triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t* poseverts[MAXALIASFRAMES];
int         posenum;

byte** player_8bit_texels_tbl;
byte*  player_8bit_texels;

/*
=================
Mod_LoadAliasFrame
=================
*/
void* Mod_LoadAliasFrame(void* pin, maliasframedesc_t* frame)
{
	auto pdaliasframe = static_cast<daliasframe_t *>(pin);

	strcpy(frame->name, pdaliasframe->name);
	frame->firstpose = posenum;
	frame->numposes  = 1;

	for (auto i = 0; i < 3; i++)
	{
		// these are byte values, so we don't have to worry about
		// endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmin.v[i] = pdaliasframe->bboxmax.v[i];
	}

	auto pinframe = reinterpret_cast<trivertx_t *>(pdaliasframe + 1);

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->numverts;

	return static_cast<void *>(pinframe);
}


/*
=================
Mod_LoadAliasGroup
=================
*/
void* Mod_LoadAliasGroup(void* pin, maliasframedesc_t* frame)
{
	int i;

	auto pingroup = static_cast<daliasgroup_t *>(pin);

	const auto numframes = LittleLong(pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes  = numframes;

	for (i = 0; i < 3; i++)
	{
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmin.v[i] = pingroup->bboxmax.v[i];
	}

	auto pin_intervals = reinterpret_cast<daliasinterval_t *>(pingroup + 1);

	frame->interval = LittleFloat(pin_intervals->interval);

	pin_intervals += numframes;

	auto ptemp = static_cast<void *>(pin_intervals);

	for (i = 0; i < numframes; i++)
	{
		poseverts[posenum] = reinterpret_cast<trivertx_t *>(static_cast<daliasframe_t *>(ptemp) + 1);
		posenum++;

		ptemp = reinterpret_cast<trivertx_t *>(static_cast<daliasframe_t *>(ptemp) + 1) + pheader->numverts;
	}

	return ptemp;
}

//=========================================================

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

struct floodfill_t
{
	short x, y;
};

extern unsigned d_8to24table[];

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void Mod_FloodFillSkin(byte* skin, int skinwidth, int skinheight)
{
	const auto  fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	auto        inpt        = 0;
	auto        outpt       = 0;
	auto        filledcolor = -1;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (auto i = 0; i < 256; ++i)
			if (d_8to24table[i] == 255 << 0) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if (fillcolor == filledcolor || fillcolor == 255)
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt         = inpt + 1 & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		const int  x   = fifo[outpt].x;
		const int  y   = fifo[outpt].y;
		auto       fdc = filledcolor;
		const auto pos = &skin[x + skinwidth * y];

		outpt = outpt + 1 & FLOODFILL_FIFO_MASK;

		if (x > 0)
		FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)
		FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)
		FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)
		FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

/*
===============
Mod_LoadAllSkins
===============
*/
void* Mod_LoadAllSkins(int numskins, daliasskintype_t* pskintype)
{
	int   j;
	char  name[32];
	byte* texels;

	const auto skin = reinterpret_cast<byte *>(pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Sys_Error("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	const auto s = pheader->skinwidth * pheader->skinheight;

	for (auto i = 0; i < numskins; i++)
	{
		if (pskintype->type == aliasskintype_t::ALIAS_SKIN_SINGLE)
		{
			Mod_FloodFillSkin(skin, pheader->skinwidth, pheader->skinheight);

			// save 8 bit texels for the player model to remap
			//		if (!strcmp(loadmodel->name,"progs/player.mdl")) {
			texels             = static_cast<byte*>(Hunk_AllocName(s, loadname));
			pheader->texels[i] = texels - reinterpret_cast<byte *>(pheader);
			memcpy(texels, reinterpret_cast<byte *>(pskintype + 1), s);
			//		}
			sprintf(name, "%s_%i", loadmodel->name, i);

			const auto texnum = GL_LoadTexture(name, pheader->skinwidth, pheader->skinheight, reinterpret_cast<byte *>(pskintype + 1), qtrue, qfalse);

			pheader->gl_texturenum[i][0] = texnum;
			pheader->gl_texturenum[i][1] = texnum;
			pheader->gl_texturenum[i][2] = texnum;
			pheader->gl_texturenum[i][3] = texnum;

			pskintype = reinterpret_cast<daliasskintype_t *>(reinterpret_cast<byte *>(pskintype + 1) + s);
		}
		else
		{
			// animating skin group.  yuck.
			pskintype++;
			const auto pinskingroup     = reinterpret_cast<daliasskingroup_t *>(pskintype);
			const auto groupskins       = LittleLong(pinskingroup->numskins);
			const auto pinskinintervals = reinterpret_cast<daliasskininterval_t *>(pinskingroup + 1);

			pskintype = reinterpret_cast<daliasskintype_t *>(pinskinintervals + groupskins);

			for (j = 0; j < groupskins; j++)
			{
				Mod_FloodFillSkin(skin, pheader->skinwidth, pheader->skinheight);
				if (j == 0)
				{
					texels             = static_cast<byte*>(Hunk_AllocName(s, loadname));
					pheader->texels[i] = texels - reinterpret_cast<byte *>(pheader);
					memcpy(texels, reinterpret_cast<byte *>(pskintype), s);
				}
				sprintf(name, "%s_%i_%i", loadmodel->name, i, j);
				pheader->gl_texturenum[i][j & 3] = GL_LoadTexture(name, pheader->skinwidth, pheader->skinheight, reinterpret_cast<byte *>(pskintype), qtrue, qfalse);
				pskintype                        = reinterpret_cast<daliasskintype_t *>(reinterpret_cast<byte *>(pskintype) + s);
			}
			const auto k = j;
			for (/* */; j < 4; j++)
				pheader->gl_texturenum[i][j & 3] =
					pheader->gl_texturenum[i][j - k];
		}
	}

	return static_cast<void *>(pskintype);
}

//=========================================================================

/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel(model_t* mod, void* buffer)
{
	int i;

	const auto start    = Hunk_LowMark();
	auto       pinmodel = static_cast<mdl_t *>(buffer);
	const auto version  = LittleLong(pinmodel->version);

	if (version != ALIAS_VERSION)
		Sys_Error("%s has wrong version number (%i should be %i)", mod->name, version, ALIAS_VERSION);

	//
	// allocate space for a working header, plus all the data except the frames,
	// skin and group info
	//
	const int size = sizeof(aliashdr_t) + (LittleLong(pinmodel->numframes) - 1) * sizeof pheader->frames[0];

	pheader = static_cast<aliashdr_t*>(Hunk_AllocName(size, loadname));

	mod->flags = LittleLong(pinmodel->flags);

	//
	// endian-adjust and copy the data, starting with the alias model header
	//
	pheader->boundingradius = LittleFloat(pinmodel->boundingradius);
	pheader->numskins       = LittleLong(pinmodel->numskins);
	pheader->skinwidth      = LittleLong(pinmodel->skinwidth);
	pheader->skinheight     = LittleLong(pinmodel->skinheight);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		Sys_Error("model %s has a skin taller than %d", mod->name,
		          MAX_LBM_HEIGHT);

	pheader->numverts = LittleLong(pinmodel->numverts);

	if (pheader->numverts <= 0)
		Sys_Error("model %s has no vertices", mod->name);

	if (pheader->numverts > MAXALIASVERTS)
		Sys_Error("model %s has too many vertices", mod->name);

	pheader->numtris = LittleLong(pinmodel->numtris);

	if (pheader->numtris <= 0)
		Sys_Error("model %s has no triangles", mod->name);

	pheader->numframes   = LittleLong(pinmodel->numframes);
	const auto numframes = pheader->numframes;
	if (numframes < 1)
		Sys_Error("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pheader->size  = LittleFloat(pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype  = static_cast<synctype_t>(LittleLong(static_cast<int>(pinmodel->synctype)));
	mod->numframes = pheader->numframes;

	for (i = 0; i < 3; i++)
	{
		pheader->scale[i]        = LittleFloat(pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat(pinmodel->scale_origin[i]);
		pheader->eyeposition[i]  = LittleFloat(pinmodel->eyeposition[i]);
	}


	//
	// load the skins
	//
	auto pskintype = reinterpret_cast<daliasskintype_t *>(&pinmodel[1]);
	pskintype      = static_cast<daliasskintype_t*>(Mod_LoadAllSkins(pheader->numskins, pskintype));

	//
	// load base s and t vertices
	//
	const auto pinstverts = reinterpret_cast<stvert_t *>(pskintype);

	for (i = 0; i < pheader->numverts; i++)
	{
		stverts[i].onseam = LittleLong(pinstverts[i].onseam);
		stverts[i].s      = LittleLong(pinstverts[i].s);
		stverts[i].t      = LittleLong(pinstverts[i].t);
	}

	//
	// load triangle lists
	//
	const auto pintriangles = reinterpret_cast<dtriangle_t *>(&pinstverts[pheader->numverts]);

	for (i = 0; i < pheader->numtris; i++)
	{
		triangles[i].facesfront = LittleLong(pintriangles[i].facesfront);

		for (auto j = 0; j < 3; j++)
		{
			triangles[i].vertindex[j] = LittleLong(pintriangles[i].vertindex[j]);
		}
	}

	//
	// load the frames
	//
	posenum         = 0;
	auto pframetype = reinterpret_cast<daliasframetype_t *>(&pintriangles[pheader->numtris]);

	for (i = 0; i < numframes; i++)
	{
		const auto frametype = static_cast<aliasframetype_t>(LittleLong(static_cast<int>(pframetype->type)));

		if (frametype == aliasframetype_t::ALIAS_SINGLE)
		{
			pframetype = static_cast<daliasframetype_t *>(Mod_LoadAliasFrame(pframetype + 1, &pheader->frames[i]));
		}
		else
		{
			pframetype = static_cast<daliasframetype_t *>(Mod_LoadAliasGroup(pframetype + 1, &pheader->frames[i]));
		}
	}

	pheader->numposes = posenum;

	mod->type = modtype_t::mod_alias;

	// FIXME: do this right
	mod->mins[0] = mod->mins[1] = mod->mins[2] = -16;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 16;

	//
	// build the draw lists
	//
	GL_MakeAliasModelDisplayLists(mod, pheader);

	//
	// move the complete, relocatable alias model to the cache
	//	
	const auto end   = Hunk_LowMark();
	const auto total = end - start;

	Cache_Alloc(&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy(mod->cache.data, pheader, total);

	Hunk_FreeToLowMark(start);
}

//=============================================================================

/*
=================
Mod_LoadSpriteFrame
=================
*/
void* Mod_LoadSpriteFrame(void* pin, mspriteframe_t** ppframe, int framenum)
{
	int  origin[2];
	char name[64];

	auto pinframe = static_cast<dspriteframe_t *>(pin);

	const auto width  = LittleLong(pinframe->width);
	const auto height = LittleLong(pinframe->height);
	const auto size   = width * height;

	const auto pspriteframe = static_cast<mspriteframe_t *>(Hunk_AllocName(sizeof(mspriteframe_t), loadname));

	Q_memset(pspriteframe, 0, sizeof(mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width  = width;
	pspriteframe->height = height;
	origin[0]            = LittleLong(pinframe->origin[0]);
	origin[1]            = LittleLong(pinframe->origin[1]);

	pspriteframe->up    = origin[1];
	pspriteframe->down  = origin[1] - height;
	pspriteframe->left  = origin[0];
	pspriteframe->right = width + origin[0];

	sprintf(name, "%s_%i", loadmodel->name, framenum);
	pspriteframe->gl_texturenum = GL_LoadTexture(name, width, height, reinterpret_cast<byte *>(pinframe + 1), qtrue, qtrue);

	return static_cast<void *>(reinterpret_cast<byte *>(pinframe) + sizeof(dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void* Mod_LoadSpriteGroup(void* pin, mspriteframe_t** ppframe, int framenum)
{
	int i;

	const auto pingroup     = static_cast<dspritegroup_t *>(pin);
	const auto numframes    = LittleLong(pingroup->numframes);
	auto       pspritegroup = static_cast<mspritegroup_t *>(Hunk_AllocName(sizeof(mspritegroup_t) + (numframes - 1) * sizeof(mspritegroup_t), loadname));

	pspritegroup->numframes = numframes;
	*ppframe                = reinterpret_cast<mspriteframe_t *>(pspritegroup);

	auto pin_intervals = reinterpret_cast<dspriteinterval_t *>(pingroup + 1);
	auto poutintervals = static_cast<float *>(Hunk_AllocName(numframes * sizeof(float), loadname));

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++)
	{
		*poutintervals = LittleFloat(pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	auto ptemp = static_cast<void *>(pin_intervals);

	for (i = 0; i < numframes; i++)
	{
		ptemp = Mod_LoadSpriteFrame(ptemp, &pspritegroup->frames[i], framenum * 100 + i);
	}

	return ptemp;
}


/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel(model_t* mod, void* buffer)
{
	auto pin = static_cast<dsprite_t *>(buffer);

	const auto version = LittleLong(pin->version);
	if (version != SPRITE_VERSION)
		Sys_Error("%s has wrong version number (%i should be %i)", mod->name, version, SPRITE_VERSION);

	const auto numframes = LittleLong(pin->numframes);

	const int size = sizeof(msprite_t) + (numframes - 1) * sizeof msprite_t::frames;

	auto psprite = static_cast<msprite_t*>(Hunk_AllocName(size, loadname));

	mod->cache.data = psprite;

	psprite->type       = LittleLong(pin->type);
	psprite->maxwidth   = LittleLong(pin->width);
	psprite->maxheight  = LittleLong(pin->height);
	psprite->beamlength = LittleFloat(pin->beamlength);
	mod->synctype       = static_cast<synctype_t>(LittleLong(static_cast<int>(pin->synctype)));
	psprite->numframes  = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth / 2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth / 2;
	mod->mins[2] = -psprite->maxheight / 2;
	mod->maxs[2] = psprite->maxheight / 2;

	//
	// load the frames
	//
	if (numframes < 1)
		Sys_Error("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	auto pframetype = reinterpret_cast<dspriteframetype_t *>(pin + 1);

	for (auto i = 0; i < numframes; i++)
	{
		const auto frametype    = static_cast<spriteframetype_t>(LittleLong(static_cast<int>(pframetype->type)));
		psprite->frames[i].type = frametype;

		if (frametype == spriteframetype_t::SPR_SINGLE)
		{
			pframetype = static_cast<dspriteframetype_t *>(Mod_LoadSpriteFrame(pframetype + 1, &psprite->frames[i].frameptr, i));
		}
		else
		{
			pframetype = static_cast<dspriteframetype_t *>(Mod_LoadSpriteGroup(pframetype + 1, &psprite->frames[i].frameptr, i));
		}
	}

	mod->type = modtype_t::mod_sprite;
}

//=============================================================================

/*
================
Mod_Print
================
*/
void Mod_Print()
{
	int      i;
	model_t* mod;

	Con_Printf("Cached models:\n");
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		Con_Printf("%8p : %s\n", mod->cache.data, mod->name);
	}
}

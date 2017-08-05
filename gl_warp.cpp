#include "quakedef.h"

extern cvar_t r_drawflat;

cvar_t r_oldwater = {"r_oldwater", "1"};
cvar_t r_waterquality = {"r_waterquality", "8"};
cvar_t r_waterwarp = {"r_waterwarp", "1"};

float load_subdivide_size; //johnfitz -- remember what subdivide_size value was when this map was loaded

float turbsin[] =
{
#include "gl_warp_sin.h"
};

#define WARPCALC(s,t) ((s + turbsin[(int)((t*2)+(cl.time*(128.0/M_PI))) & 255]) * (1.0/64)) //johnfitz -- correct warp
#define WARPCALC2(s,t) ((s + turbsin[(int)((t*0.125+cl.time)*(128.0/M_PI)) & 255]) * (1.0/64)) //johnfitz -- old warp

//==============================================================================
//
//  OLD-STYLE WATER
//
//==============================================================================

extern model_t* loadmodel;

msurface_t* warpface;

cvar_t gl_subdivide_size = {"gl_subdivide_size", "128", true};

void BoundPoly(int numverts, float* verts, vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	auto v = verts;
	for (auto i = 0; i < numverts; i++)
		for (auto j = 0; j < 3; j++ , v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void SubdividePolygon(int numverts, float* verts)
{
	int i, j;
	vec3_t mins, maxs;
	float* v;
	vec3_t front[64], back[64];
	int f, b;
	float dist[64];
	glpoly_t* poly;

	if (numverts > 60)
		Sys_Error("numverts = %i", numverts);

	BoundPoly(numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++)
	{
		float m = (mins[i] + maxs[i]) * 0.5;
		m = gl_subdivide_size.value * floor(m / gl_subdivide_size.value + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++ , v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++ , v += 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if (dist[j] > 0 != dist[j + 1] > 0)
			{
				// clip point
				auto frac = dist[j] / (dist[j] - dist[j + 1]);
				for (auto k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon(f, front[0]);
		SubdividePolygon(b, back[0]);
		return;
	}

	poly = static_cast<glpoly_t*>(Hunk_Alloc(sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float)));
	poly->next = warpface->polys->next;
	warpface->polys->next = poly;
	poly->numverts = numverts;
	for (i = 0; i < numverts; i++ , verts += 3)
	{
		VectorCopy (verts, poly->verts[i]);
		float s = DotProduct (verts, warpface->texinfo->vecs[0]);
		float t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

/*
================
GL_SubdivideSurface
================
*/
void GL_SubdivideSurface(msurface_t* fa)
{
	vec3_t verts[64];
	int i;

	warpface = fa;

	//the first poly in the chain is the undivided poly for newwater rendering.
	//grab the verts from that.
	for (i = 0; i < fa->polys->numverts; i++)
	VectorCopy (fa->polys->verts[i], verts[i]);

	SubdividePolygon(fa->polys->numverts, verts[0]);
}

/*
================
DrawWaterPoly -- johnfitz
================
*/
void DrawWaterPoly(glpoly_t* p)
{
	float* v;
	int i;

	if (load_subdivide_size > 48)
	{
		glBegin(GL_POLYGON);
		v = p->verts[0];
		for (i = 0; i < p->numverts; i++ , v += VERTEXSIZE)
		{
			glTexCoord2f(WARPCALC2(v[3],v[4]), WARPCALC2(v[4],v[3]));
			glVertex3fv(v);
		}
		glEnd();
	}
	else
	{
		glBegin(GL_POLYGON);
		v = p->verts[0];
		for (i = 0; i < p->numverts; i++ , v += VERTEXSIZE)
		{
			glTexCoord2f(WARPCALC(v[3],v[4]), WARPCALC(v[4],v[3]));
			glVertex3fv(v);
		}
		glEnd();
	}
}

//==============================================================================
//
//  RENDER-TO-FRAMEBUFFER WATER
//
//==============================================================================

/*
=============
R_UpdateWarpTextures -- johnfitz -- each frame, update warping textures
=============
*/
void R_UpdateWarpTextures()
{
	texture_t* tx;
	float x, y, x2;

	if (r_oldwater.value || cl.paused || r_drawflat_cheatsafe || r_lightmap_cheatsafe)
		return;

	float warptess = 128.0 / CLAMP (3.0, floor(r_waterquality.value), 64.0);

	for (auto i = 0; i < cl.worldmodel->numtextures; i++)
	{
		if (!((tx = cl.worldmodel->textures[i])))
			continue;

		if (!tx->update_warp)
			continue;

		//render warp
		GL_SetCanvas(canvastype::CANVAS_WARPIMAGE);
		GL_Bind(tx->gltexture);
		for (x = 0.0; x < 128.0; x = x2)
		{
			x2 = x + warptess;
			glBegin(GL_TRIANGLE_STRIP);
			for (y = 0.0; y < 128.01; y += warptess) // .01 for rounding errors
			{
				glTexCoord2f(WARPCALC(x,y), WARPCALC(y,x));
				glVertex2f(x, y);
				glTexCoord2f(WARPCALC(x2,y), WARPCALC(y,x2));
				glVertex2f(x2, y);
			}
			glEnd();
		}

		//copy to texture
		GL_Bind(tx->warpimage);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, glx, gly + glheight - gl_warpimagesize, gl_warpimagesize, gl_warpimagesize);

		tx->update_warp = false;
	}

	//if warp render went down into sbar territory, we need to be sure to refresh it next frame
	if (gl_warpimagesize + sb_lines > glheight)
		Sbar_Changed();

	//if viewsize is less than 100, we need to redraw the frame around the viewport
	scr_tileclear_updates = 0;
}

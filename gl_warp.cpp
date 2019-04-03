#include "quakedef.h"

extern model_t* loadmodel;

int   solidskytexture;
int   alphaskytexture;
float speedscale; // for top sky and bottom sky

msurface_t* warpface;


void BoundPoly(int numverts, float* verts, vec3_t mins, vec3_t maxs)
{
	mins[0]         = mins[1] = mins[2] = 9999;
	maxs[0]         = maxs[1] = maxs[2] = -9999;
	auto          v = verts;
	for (auto     i = 0; i < numverts; i++)
		for (auto j = 0; j < 3; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void SubdividePolygon(int numverts, float* verts)
{
	int       i;
	int       j;
	vec3_t    mins;
	vec3_t    maxs;
	float*    v;
	vec3_t    front[64];
	vec3_t    back[64];
	int       f, b;
	float     dist[64];
	glpoly_t* poly;

	if (numverts > 60)
		Sys_Error("numverts = %i", numverts);

	BoundPoly(numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++)
	{
		float m = (mins[i] + maxs[i]) * 0.5;
		m       = gl_subdivide_size.value * floor(m / gl_subdivide_size.value + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v           = verts + i;
		for (j      = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy (verts, v);

		f      = b = 0;
		v      = verts;
		for (j = 0; j < numverts; j++, v += 3)
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
				const auto frac = dist[j] / (dist[j] - dist[j + 1]);
				for (auto  k    = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon(f, front[0]);
		SubdividePolygon(b, back[0]);
		return;
	}

	poly            = static_cast<glpoly_t*>(Hunk_Alloc(sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float)));
	poly->next      = warpface->polys;
	warpface->polys = poly;
	poly->numverts  = numverts;
	for (i          = 0; i < numverts; i++, verts += 3)
	{
		VectorCopy (verts, poly->verts[i]);
		const auto s      = DotProduct (verts, warpface->texinfo->vecs[0]);
		const auto t      = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface(msurface_t* fa)
{
	vec3_t verts[64];
	int    numverts;
	float* vec;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts    = 0;
	for (auto i = 0; i < fa->numedges; i++)
	{
		const auto lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon(numverts, verts[0]);
}

//=========================================================


// speed up sin calculations - Ed
float turbsin[] =
{
	0, 0.19633, 0.392541, 0.588517, 0.784137, 0.979285, 1.17384, 1.3677,
	1.56072, 1.75281, 1.94384, 2.1337, 2.32228, 2.50945, 2.69512, 2.87916,
	3.06147, 3.24193, 3.42044, 3.59689, 3.77117, 3.94319, 4.11282, 4.27998,
	4.44456, 4.60647, 4.76559, 4.92185, 5.07515, 5.22538, 5.37247, 5.51632,
	5.65685, 5.79398, 5.92761, 6.05767, 6.18408, 6.30677, 6.42566, 6.54068,
	6.65176, 6.75883, 6.86183, 6.9607, 7.05537, 7.14579, 7.23191, 7.31368,
	7.39104, 7.46394, 7.53235, 7.59623, 7.65552, 7.71021, 7.76025, 7.80562,
	7.84628, 7.88222, 7.91341, 7.93984, 7.96148, 7.97832, 7.99036, 7.99759,
	8, 7.99759, 7.99036, 7.97832, 7.96148, 7.93984, 7.91341, 7.88222,
	7.84628, 7.80562, 7.76025, 7.71021, 7.65552, 7.59623, 7.53235, 7.46394,
	7.39104, 7.31368, 7.23191, 7.14579, 7.05537, 6.9607, 6.86183, 6.75883,
	6.65176, 6.54068, 6.42566, 6.30677, 6.18408, 6.05767, 5.92761, 5.79398,
	5.65685, 5.51632, 5.37247, 5.22538, 5.07515, 4.92185, 4.76559, 4.60647,
	4.44456, 4.27998, 4.11282, 3.94319, 3.77117, 3.59689, 3.42044, 3.24193,
	3.06147, 2.87916, 2.69512, 2.50945, 2.32228, 2.1337, 1.94384, 1.75281,
	1.56072, 1.3677, 1.17384, 0.979285, 0.784137, 0.588517, 0.392541, 0.19633,
	9.79717e-16, -0.19633, -0.392541, -0.588517, -0.784137, -0.979285, -1.17384, -1.3677,
	-1.56072, -1.75281, -1.94384, -2.1337, -2.32228, -2.50945, -2.69512, -2.87916,
	-3.06147, -3.24193, -3.42044, -3.59689, -3.77117, -3.94319, -4.11282, -4.27998,
	-4.44456, -4.60647, -4.76559, -4.92185, -5.07515, -5.22538, -5.37247, -5.51632,
	-5.65685, -5.79398, -5.92761, -6.05767, -6.18408, -6.30677, -6.42566, -6.54068,
	-6.65176, -6.75883, -6.86183, -6.9607, -7.05537, -7.14579, -7.23191, -7.31368,
	-7.39104, -7.46394, -7.53235, -7.59623, -7.65552, -7.71021, -7.76025, -7.80562,
	-7.84628, -7.88222, -7.91341, -7.93984, -7.96148, -7.97832, -7.99036, -7.99759,
	-8, -7.99759, -7.99036, -7.97832, -7.96148, -7.93984, -7.91341, -7.88222,
	-7.84628, -7.80562, -7.76025, -7.71021, -7.65552, -7.59623, -7.53235, -7.46394,
	-7.39104, -7.31368, -7.23191, -7.14579, -7.05537, -6.9607, -6.86183, -6.75883,
	-6.65176, -6.54068, -6.42566, -6.30677, -6.18408, -6.05767, -5.92761, -5.79398,
	-5.65685, -5.51632, -5.37247, -5.22538, -5.07515, -4.92185, -4.76559, -4.60647,
	-4.44456, -4.27998, -4.11282, -3.94319, -3.77117, -3.59689, -3.42044, -3.24193,
	-3.06147, -2.87916, -2.69512, -2.50945, -2.32228, -2.1337, -1.94384, -1.75281,
	-1.56072, -1.3677, -1.17384, -0.979285, -0.784137, -0.588517, -0.392541, -0.19633,
};
#define TURBSCALE (256.0 / (2 * M_PI))

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys(msurface_t* fa)
{
	float* v;
	int    i;

	for (auto p = fa->polys; p; p = p->next)
	{
		glBegin(GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE)
		{
			const auto os = v[3];
			const auto ot = v[4];

			auto s = os + turbsin[static_cast<int>((ot * 0.125 + realtime) * TURBSCALE) & 255];
			s *= 1.0 / 64;

			auto t = ot + turbsin[static_cast<int>((os * 0.125 + realtime) * TURBSCALE) & 255];
			t *= 1.0 / 64;

			glTexCoord2f(s, t);
			glVertex3fv(v);
		}
		glEnd();
	}
}


/*
=============
EmitSkyPolys
=============
*/
void EmitSkyPolys(msurface_t* fa)
{
	float* v;
	int    i;
	vec3_t dir;

	for (auto p = fa->polys; p; p = p->next)
	{
		glBegin(GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE)
		{
			VectorSubtract (v, r_origin, dir);
			dir[2] *= 3; // flatten the sphere

			auto length = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
			length      = sqrt(length);
			length      = 6 * 63 / length;

			dir[0] *= length;
			dir[1] *= length;

			const float s = (speedscale + dir[0]) * (1.0 / 128);
			const float t = (speedscale + dir[1]) * (1.0 / 128);

			glTexCoord2f(s, t);
			glVertex3fv(v);
		}
		glEnd();
	}
}

/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void EmitBothSkyLayers(msurface_t* fa)
{
	GL_DisableMultitexture();

	GL_Bind(solidskytexture);
	speedscale = realtime * 8;
	speedscale -= static_cast<int>(speedscale) & ~127;

	EmitSkyPolys(fa);

	glEnable(GL_BLEND);
	GL_Bind(alphaskytexture);
	speedscale = realtime * 16;
	speedscale -= static_cast<int>(speedscale) & ~127;

	EmitSkyPolys(fa);

	glDisable(GL_BLEND);
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain(msurface_t* s)
{
	msurface_t* fa;

	GL_DisableMultitexture();

	// used when gl_texsort is on
	GL_Bind(solidskytexture);
	speedscale = realtime * 8;
	speedscale -= static_cast<int>(speedscale) & ~127;

	for (fa = s; fa; fa = fa->texturechain)
		EmitSkyPolys(fa);

	glEnable(GL_BLEND);
	GL_Bind(alphaskytexture);
	speedscale = realtime * 16;
	speedscale -= static_cast<int>(speedscale) & ~127;

	for (fa = s; fa; fa = fa->texturechain)
		EmitSkyPolys(fa);

	glDisable(GL_BLEND);
}

//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky(texture_t* mt)
{
	int      i;
	int      j;
	int      p;
	unsigned trans[128 * 128];
	unsigned transpix;
	int      g;
	int      b;

	const auto src = reinterpret_cast<byte *>(mt) + mt->offsets[0];

	// make an average value for the back to avoid
	// a fringe on the top level

	auto r     = g = b = 0;
	for (i     = 0; i < 128; i++)
		for (j = 0; j < 128; j++)
		{
			p                  = src[i * 256 + j + 128];
			const auto rgba    = &d_8to24table[p];
			trans[i * 128 + j] = *rgba;
			r += reinterpret_cast<byte *>(rgba)[0];
			g += reinterpret_cast<byte *>(rgba)[1];
			b += reinterpret_cast<byte *>(rgba)[2];
		}

	reinterpret_cast<byte *>(&transpix)[0] = r / (128 * 128);
	reinterpret_cast<byte *>(&transpix)[1] = g / (128 * 128);
	reinterpret_cast<byte *>(&transpix)[2] = b / (128 * 128);
	reinterpret_cast<byte *>(&transpix)[3] = 0;


	if (!solidskytexture)
		solidskytexture = texture_extension_number++;
	GL_Bind(solidskytexture);
	glTexImage2D(GL_TEXTURE_2D, 0, gl_solid_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


	for (i     = 0; i < 128; i++)
		for (j = 0; j < 128; j++)
		{
			p = src[i * 256 + j];
			if (p == 0)
				trans[i * 128 + j] = transpix;
			else
				trans[i * 128 + j] = d_8to24table[p];
		}

	if (!alphaskytexture)
		alphaskytexture = texture_extension_number++;
	GL_Bind(alphaskytexture);
	glTexImage2D(GL_TEXTURE_2D, 0, gl_alpha_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

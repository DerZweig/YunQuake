#include "quakedef.h"

int r_dlightframecount;

extern cvar_t r_flatlightstyles; //johnfitz

/*
==================
R_AnimateLight
==================
*/
void R_AnimateLight()
{
	int k;

	//
	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	auto i = static_cast<int>(cl.time * 10);
	for (int j = 0; j < MAX_LIGHTSTYLES; j++)
	{
		if (!cl_lightstyle[j].length)
		{
			d_lightstylevalue[j] = 256;
			continue;
		}
		//johnfitz -- r_flatlightstyles
		if (r_flatlightstyles.value == 2)
			k = cl_lightstyle[j].peak - 'a';
		else if (r_flatlightstyles.value == 1)
			k = cl_lightstyle[j].average - 'a';
		else
		{
			k = i % cl_lightstyle[j].length;
			k = cl_lightstyle[j].map[k] - 'a';
		}
		d_lightstylevalue[j] = k * 22;
		//johnfitz
	}
}

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING (gl_flashblend 1)

=============================================================================
*/

void AddLightBlend(float r, float g, float b, float a2)
{
	float a;

	v_blend[3] = a = v_blend[3] + a2 * (1 - v_blend[3]);

	a2 = a2 / a;

	v_blend[0] = v_blend[1] * (1 - a2) + r * a2;
	v_blend[1] = v_blend[1] * (1 - a2) + g * a2;
	v_blend[2] = v_blend[2] * (1 - a2) + b * a2;
}

void R_RenderDlight(dlight_t* light)
{
	int i;
	vec3_t v;

	float rad = light->radius * 0.35;

	VectorSubtract (light->origin, r_origin, v);
	if (Length(v) < rad)
	{ // view is inside the dlight
		AddLightBlend(1, 0.5, 0, light->radius * 0.0003);
		return;
	}

	glBegin(GL_TRIANGLE_FAN);
	glColor3f(0.2, 0.1, 0.0);
	for (i = 0; i < 3; i++)
		v[i] = light->origin[i] - vpn[i] * rad;
	glVertex3fv(v);
	glColor3f(0, 0, 0);
	for (i = 16; i >= 0; i--)
	{
		float a = i / 16.0 * M_PI * 2;
		for (int j = 0; j < 3; j++)
			v[j] = light->origin[j] + vright[j] * cos(a) * rad
				+ vup[j] * sin(a) * rad;
		glVertex3fv(v);
	}
	glEnd();
}

/*
=============
R_RenderDlights
=============
*/
void R_RenderDlights()
{
	if (!gl_flashblend.value)
		return;

	r_dlightframecount = r_framecount + 1; // because the count hasn't
	//  advanced yet for this frame
	glDepthMask(0);
	glDisable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	auto l = cl_dlights;
	for (auto i = 0; i < MAX_DLIGHTS; i++ , l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		R_RenderDlight(l);
	}

	glColor3f(1, 1, 1);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(1);
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights -- johnfitz -- rewritten to use LordHavoc's lighting speedup
=============
*/
void R_MarkLights(dlight_t* light, int bit, mnode_t* node)
{
	mplane_t* splitplane;
	msurface_t* surf;
	vec3_t impact;
	float dist;

start:

	if (node->contents < 0)
		return;

	splitplane = node->plane;
	if (splitplane->type < 3)
		dist = light->origin[splitplane->type] - splitplane->dist;
	else
		dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;

	if (dist > light->radius)
	{
		node = node->children[0];
		goto start;
	}
	if (dist < -light->radius)
	{
		node = node->children[1];
		goto start;
	}

	auto maxdist = light->radius * light->radius;
	// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (auto i = 0; i < node->numsurfaces; i++ , surf++)
	{
		for (auto j = 0; j < 3; j++)
			impact[j] = light->origin[j] - surf->plane->normal[j] * dist;
		// clamp center of light to corner and check brightness
		auto l = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
		int s = l + 0.5;
		if (s < 0) s = 0;
		else if (s > surf->extents[0]) s = surf->extents[0];
		s = l - s;
		l = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
		int t = l + 0.5;
		if (t < 0) t = 0;
		else if (t > surf->extents[1]) t = surf->extents[1];
		t = l - t;
		// compare to minimum light
		if (s * s + t * t + dist * dist < maxdist)
		{
			if (surf->dlightframe != r_dlightframecount) // not dynamic until now
			{
				surf->dlightbits = bit;
				surf->dlightframe = r_dlightframecount;
			}
			else // already dynamic
				surf->dlightbits |= bit;
		}
	}

	if (node->children[0]->contents >= 0)
		R_MarkLights(light, bit, node->children[0]);
	if (node->children[1]->contents >= 0)
		R_MarkLights(light, bit, node->children[1]);
}

/*
=============
R_PushDlights
=============
*/
void R_PushDlights()
{
	if (gl_flashblend.value)
		return;

	r_dlightframecount = r_framecount + 1; // because the count hasn't
	//  advanced yet for this frame
	auto l = cl_dlights;

	for (auto i = 0; i < MAX_DLIGHTS; i++ , l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		R_MarkLights(l, 1 << i, cl.worldmodel->nodes);
	}
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

mplane_t* lightplane;
vec3_t lightspot;
vec3_t lightcolor; //johnfitz -- lit support via lordhavoc

/*
=============
RecursiveLightPoint -- johnfitz -- replaced entire function for lit support via lordhavoc
=============
*/
int RecursiveLightPoint(vec3_t color, mnode_t* node, vec3_t start, vec3_t end)
{
	float front, back;
	vec3_t mid;

loc0:
	if (node->contents < 0)
		return false; // didn't hit anything

	// calculate mid point
	if (node->plane->type < 3)
	{
		front = start[node->plane->type] - node->plane->dist;
		back = end[node->plane->type] - node->plane->dist;
	}
	else
	{
		front = DotProduct(start, node->plane->normal) - node->plane->dist;
		back = DotProduct(end, node->plane->normal) - node->plane->dist;
	}

	// LordHavoc: optimized recursion
	if (back < 0 == front < 0)
	//		return RecursiveLightPoint (color, node->children[front < 0], start, end);
	{
		node = node->children[front < 0];
		goto loc0;
	}

	auto frac = front / (front - back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	// go down front side
	if (RecursiveLightPoint(color, node->children[front < 0], start, mid))
		return true; // hit something
	msurface_t* surf;
	// check for impact on this node
	VectorCopy (mid, lightspot);
	lightplane = node->plane;

	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (auto i = 0; i < node->numsurfaces; i++ , surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue; // no lightmaps

		auto ds = static_cast<int>(static_cast<float>(DotProduct (mid, surf->texinfo->vecs[0])) + surf->texinfo->vecs[0][3]);
		auto dt = static_cast<int>(static_cast<float>(DotProduct (mid, surf->texinfo->vecs[1])) + surf->texinfo->vecs[1][3]);

		if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
			continue;

		ds -= surf->texturemins[0];
		dt -= surf->texturemins[1];

		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		if (surf->samples)
		{
			auto dsfrac = ds & 15, dtfrac = dt & 15, r00 = 0, g00 = 0, b00 = 0, r01 = 0, g01 = 0, b01 = 0, r10 = 0, g10 = 0, b10 = 0, r11 = 0, g11 = 0, b11 = 0;
			auto line3 = ((surf->extents[0] >> 4) + 1) * 3;

			auto lightmap = surf->samples + ((dt >> 4) * ((surf->extents[0] >> 4) + 1) + (ds >> 4)) * 3; // LordHavoc: *3 for color

			for (auto maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				float scale = static_cast<float>(d_lightstylevalue[surf->styles[maps]]) * 1.0 / 256.0;
				r00 += static_cast<float>(lightmap[0]) * scale;
				g00 += static_cast<float>(lightmap[1]) * scale;
				b00 += static_cast<float>(lightmap[2]) * scale;
				r01 += static_cast<float>(lightmap[3]) * scale;
				g01 += static_cast<float>(lightmap[4]) * scale;
				b01 += static_cast<float>(lightmap[5]) * scale;
				r10 += static_cast<float>(lightmap[line3 + 0]) * scale;
				g10 += static_cast<float>(lightmap[line3 + 1]) * scale;
				b10 += static_cast<float>(lightmap[line3 + 2]) * scale;
				r11 += static_cast<float>(lightmap[line3 + 3]) * scale;
				g11 += static_cast<float>(lightmap[line3 + 4]) * scale;
				b11 += static_cast<float>(lightmap[line3 + 5]) * scale;
				lightmap += ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1) * 3; // LordHavoc: *3 for colored lighting
			}

			color[0] += static_cast<float>(static_cast<int>(((((r11 - r10) * dsfrac >> 4) + r10 - (((r01 - r00) * dsfrac >> 4) + r00)) * dtfrac >> 4) + (((r01 - r00) * dsfrac >> 4) + r00)));
			color[1] += static_cast<float>(static_cast<int>(((((g11 - g10) * dsfrac >> 4) + g10 - (((g01 - g00) * dsfrac >> 4) + g00)) * dtfrac >> 4) + (((g01 - g00) * dsfrac >> 4) + g00)));
			color[2] += static_cast<float>(static_cast<int>(((((b11 - b10) * dsfrac >> 4) + b10 - (((b01 - b00) * dsfrac >> 4) + b00)) * dtfrac >> 4) + (((b01 - b00) * dsfrac >> 4) + b00)));
		}
		return true; // success
	}

	// go down back side
	return RecursiveLightPoint(color, node->children[front >= 0], mid, end);
}

/*
=============
R_LightPoint -- johnfitz -- replaced entire function for lit support via lordhavoc
=============
*/
int R_LightPoint(vec3_t p)
{
	vec3_t end;

	if (!cl.worldmodel->lightdata)
	{
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 255;
		return 255;
	}

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 8192; //johnfitz -- was 2048

	lightcolor[0] = lightcolor[1] = lightcolor[2] = 0;
	RecursiveLightPoint(lightcolor, cl.worldmodel->nodes, p, end);
	return (lightcolor[0] + lightcolor[1] + lightcolor[2]) * (1.0f / 3.0f);
}

#include "quakedef.h"

cvar_t chase_back = {"chase_back", "100"};
cvar_t chase_up = {"chase_up", "16"};
cvar_t chase_right = {"chase_right", "0"};
cvar_t chase_active = {"chase_active", "0"};

/*
==============
Chase_Init
==============
*/
void Chase_Init()
{
	Cvar_RegisterVariable(&chase_back, nullptr);
	Cvar_RegisterVariable(&chase_up, nullptr);
	Cvar_RegisterVariable(&chase_right, nullptr);
	Cvar_RegisterVariable(&chase_active, nullptr);
}

bool SV_RecursiveHullCheck(hull_t* hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t* trace);

/*
==============
TraceLine

TODO: impact on bmodels, monsters
==============
*/
void TraceLine(vec3_t start, vec3_t end, vec3_t impact)
{
	trace_t trace;

	memset(&trace, 0, sizeof trace);
	SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);

	VectorCopy (trace.endpos, impact);
}

/*
==============
Chase_UpdateForClient -- johnfitz -- orient client based on camera. called after input
==============
*/
void Chase_UpdateForClient()
{
	//place camera

	//assign client angles to camera

	//see where camera points

	//adjust client angles to point at the same place
}

/*
==============
Chase_UpdateForDrawing -- johnfitz -- orient camera based on client. called before drawing

TODO: stay at least 8 units away from all walls in this leaf
==============
*/
void Chase_UpdateForDrawing()
{
	vec3_t forward, up, right;
	vec3_t ideal, crosshair, temp;

	AngleVectors(cl.viewangles, forward, right, up);

	// calc ideal camera location before checking for walls
	for (int i = 0; i < 3; i++)
		ideal[i] = cl.viewent.origin[i]
			- forward[i] * chase_back.value
			+ right[i] * chase_right.value;
	//+ up[i]*chase_up.value;
	ideal[2] = cl.viewent.origin[2] + chase_up.value;

	// make sure camera is not in or behind a wall
	TraceLine(r_refdef.vieworg, ideal, temp);
	if (Length(temp) != 0)
	VectorCopy(temp, ideal);

	// place camera
	VectorCopy (ideal, r_refdef.vieworg);

	// find the spot the player is looking at
	VectorMA(cl.viewent.origin, 4096, forward, temp);
	TraceLine(cl.viewent.origin, temp, crosshair);

	// calculate camera angles to look at the same spot
	VectorSubtract (crosshair, r_refdef.vieworg, temp);
	VectorAngles(temp, r_refdef.viewangles);
	if (r_refdef.viewangles[PITCH] == 90 || r_refdef.viewangles[PITCH] == -90)
		r_refdef.viewangles[YAW] = cl.viewangles[YAW];
}

#include "quakedef.h"

mnode_t* r_pefragtopnode;


//===========================================================================

/*
===============================================================================

					ENTITY FRAGMENT FUNCTIONS

===============================================================================
*/

efrag_t** lastlink;

vec3_t r_emins, r_emaxs;

entity_t* r_addent;


/*
================
R_RemoveEfrags

Call when removing an object from the world or moving it to another position
================
*/
void R_RemoveEfrags(entity_t* ent)
{
	auto ef = ent->efrag;

	while (ef)
	{
		auto prev = &ef->leaf->efrags;
		while (true)
		{
			auto walk = *prev;
			if (!walk)
				break;
			if (walk == ef)
			{ // remove this fragment
				*prev = ef->leafnext;
				break;
			}
			prev = &walk->leafnext;
		}

		auto old = ef;
		ef = ef->entnext;

		// put it on the free list
		old->entnext = cl.free_efrags;
		cl.free_efrags = old;
	}

	ent->efrag = nullptr;
}

/*
===================
R_SplitEntityOnNode
===================
*/
void R_SplitEntityOnNode(mnode_t* node)
{
	mplane_t* splitplane;

	if (node->contents == CONTENTS_SOLID)
	{
		return;
	}

	// add an efrag if the node is a leaf

	if (node->contents < 0)
	{
		if (!r_pefragtopnode)
			r_pefragtopnode = node;

		auto leaf = reinterpret_cast<mleaf_t *>(node);

		// grab an efrag off the free list
		auto ef = cl.free_efrags;
		if (!ef)
		{
			//johnfitz -- less spammy overflow message
			if (!dev_overflows.efrags || dev_overflows.efrags + CONSOLE_RESPAM_TIME < realtime)
			{
				Con_Printf("Too many efrags!\n");
				dev_overflows.efrags = realtime;
			}
			//johnfitz
			return; // no free fragments...
		}
		cl.free_efrags = cl.free_efrags->entnext;

		ef->entity = r_addent;

		// add the entity link
		*lastlink = ef;
		lastlink = &ef->entnext;
		ef->entnext = nullptr;

		// set the leaf links
		ef->leaf = leaf;
		ef->leafnext = leaf->efrags;
		leaf->efrags = ef;

		return;
	}

	// NODE_MIXED

	splitplane = node->plane;
	int sides = BOX_ON_PLANE_SIDE(r_emins, r_emaxs, splitplane);

	if (sides == 3)
	{
		// split on this plane
		// if this is the first splitter of this bmodel, remember it
		if (!r_pefragtopnode)
			r_pefragtopnode = node;
	}

	// recurse down the contacted sides
	if (sides & 1)
		R_SplitEntityOnNode(node->children[0]);

	if (sides & 2)
		R_SplitEntityOnNode(node->children[1]);
}

/*
===========
R_CheckEfrags -- johnfitz -- check for excessive efrag count
===========
*/
void R_CheckEfrags()
{
	efrag_t* ef;
	int count;

	if (cls.signon < 2)
		return; //don't spam when still parsing signon packet full of static ents

	for (count = MAX_EFRAGS , ef = cl.free_efrags; ef; count-- , ef = ef->entnext);

	if (count > 640 && dev_peakstats.efrags <= 640)
		Con_Warning("%i efrags exceeds standard limit of 640.\n", count);

	dev_stats.efrags = count;
	dev_peakstats.efrags = max(count, dev_peakstats.efrags);
}

/*
===========
R_AddEfrags
===========
*/
void R_AddEfrags(entity_t* ent)
{
	if (!ent->model)
		return;

	r_addent = ent;

	lastlink = &ent->efrag;
	r_pefragtopnode = nullptr;

	auto entmodel = ent->model;

	for (auto i = 0; i < 3; i++)
	{
		r_emins[i] = ent->origin[i] + entmodel->mins[i];
		r_emaxs[i] = ent->origin[i] + entmodel->maxs[i];
	}

	R_SplitEntityOnNode(cl.worldmodel->nodes);

	ent->topnode = r_pefragtopnode;

	R_CheckEfrags(); //johnfitz
}


/*
================
R_StoreEfrags -- johnfitz -- pointless switch statement removed.
================
*/
void R_StoreEfrags(efrag_t** ppefrag)
{
	efrag_t* pefrag;

	while ((pefrag = *ppefrag) != nullptr)
	{
		auto pent = pefrag->entity;

		if (pent->visframe != r_framecount && cl_numvisedicts < MAX_VISEDICTS)
		{
			cl_visedicts[cl_numvisedicts++] = pent;
			pent->visframe = r_framecount;
		}

		ppefrag = &pefrag->leafnext;
	}
}

#include "quakedef.h"


/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/

model_t* aliasmodel;
aliashdr_t* paliashdr;

bool used[8192];

// the command list holds counts and s/t values that are valid for
// every frame
int commands[8192];
int numcommands;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
int vertexorder[8192];
int numorder;

int allverts, alltris;

int stripverts[128];
int striptris[128];
int stripcount;

/*
================
StripLength
================
*/
int StripLength(int starttri, int startv)
{
	int j;
	mtriangle_t* check;

	used[starttri] = true;

	auto last = &triangles[starttri];

	stripverts[0] = last->vertindex[startv % 3];
	stripverts[1] = last->vertindex[(startv + 1) % 3];
	stripverts[2] = last->vertindex[(startv + 2) % 3];

	striptris[0] = starttri;
	stripcount = 1;

	auto m1 = last->vertindex[(startv + 2) % 3];
	auto m2 = last->vertindex[(startv + 1) % 3];

	// look for a matching triangle
nexttri:
	for (j = starttri + 1 , check = &triangles[starttri + 1]; j < pheader->numtris; j++ , check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (int k = 0; k < 3; k++)
		{
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[(k + 1) % 3] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			if (stripcount & 1)
				m2 = check->vertindex[(k + 2) % 3];
			else
				m1 = check->vertindex[(k + 2) % 3];

			stripverts[stripcount + 2] = check->vertindex[(k + 2) % 3];
			striptris[stripcount] = j;
			stripcount++;

			used[j] = true;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j = starttri + 1; j < pheader->numtris; j++)
		if (used[j])
			used[j] = false;

	return stripcount;
}

/*
===========
FanLength
===========
*/
int FanLength(int starttri, int startv)
{
	int j;
	mtriangle_t* check;

	used[starttri] = true;

	auto last = &triangles[starttri];

	stripverts[0] = last->vertindex[startv % 3];
	stripverts[1] = last->vertindex[(startv + 1) % 3];
	stripverts[2] = last->vertindex[(startv + 2) % 3];

	striptris[0] = starttri;
	stripcount = 1;

	auto m1 = last->vertindex[(startv + 0) % 3];
	auto m2 = last->vertindex[(startv + 2) % 3];


	// look for a matching triangle
nexttri:
	for (j = starttri + 1 , check = &triangles[starttri + 1]; j < pheader->numtris; j++ , check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (int k = 0; k < 3; k++)
		{
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[(k + 1) % 3] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			m2 = check->vertindex[(k + 2) % 3];

			stripverts[stripcount + 2] = m2;
			striptris[stripcount] = j;
			stripcount++;

			used[j] = true;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j = starttri + 1; j < pheader->numtris; j++)
		if (used[j])
			used[j] = false;

	return stripcount;
}


/*
================
BuildTris

Generate a list of trifans or strips
for the model, which holds for all frames
================
*/
void BuildTris()
{
	int j;
	int len, besttype = 0;
	int bestverts[1024];
	int besttris[1024];

	//
	// build tristrips
	//
	numorder = 0;
	numcommands = 0;
	memset(used, 0, sizeof used);
	for (int i = 0; i < pheader->numtris; i++)
	{
		// pick an unused triangle and start the trifan
		if (used[i])
			continue;

		int bestlen = 0;
		for (int type = 0; type < 2; type++)
		//	type = 1;
		{
			for (int startv = 0; startv < 3; startv++)
			{
				if (type == 1)
					len = StripLength(i, startv);
				else
					len = FanLength(i, startv);
				if (len > bestlen)
				{
					besttype = type;
					bestlen = len;
					for (j = 0; j < bestlen + 2; j++)
						bestverts[j] = stripverts[j];
					for (j = 0; j < bestlen; j++)
						besttris[j] = striptris[j];
				}
			}
		}

		// mark the tris on the best strip as used
		for (j = 0; j < bestlen; j++)
			used[besttris[j]] = true;

		if (besttype == 1)
			commands[numcommands++] = bestlen + 2;
		else
			commands[numcommands++] = -(bestlen + 2);

		for (j = 0; j < bestlen + 2; j++)
		{
			// emit a vertex into the reorder buffer
			int k = bestverts[j];
			vertexorder[numorder++] = k;

			// emit s/t coords into the commands stream
			float s = stverts[k].s;
			float t = stverts[k].t;
			if (!triangles[besttris[0]].facesfront && stverts[k].onseam)
				s += pheader->skinwidth / 2; // on back side
			s = (s + 0.5) / pheader->skinwidth;
			t = (t + 0.5) / pheader->skinheight;

			*reinterpret_cast<float *>(&commands[numcommands++]) = s;
			*reinterpret_cast<float *>(&commands[numcommands++]) = t;
		}
	}

	commands[numcommands++] = 0; // end of list marker

	Con_DPrintf("%3i tri %3i vert %3i cmd\n", pheader->numtris, numorder, numcommands);

	allverts += numorder;
	alltris += pheader->numtris;
}


/*
================
GL_MakeAliasModelDisplayLists
================
*/
void GL_MakeAliasModelDisplayLists(model_t* m, aliashdr_t* hdr)
{
	int count; //johnfitz -- precompute texcoords for padded skins

	//johnfitz -- padded skins
	auto hscale = static_cast<float>(hdr->skinwidth) / static_cast<float>(TexMgr_PadConditional(hdr->skinwidth));
	auto vscale = static_cast<float>(hdr->skinheight) / static_cast<float>(TexMgr_PadConditional(hdr->skinheight));
	//johnfitz

	aliasmodel = m;
	paliashdr = hdr; // (aliashdr_t *)Mod_Extradata (m);

	//johnfitz -- generate meshes
	Con_DPrintf("meshing %s...\n", m->name);
	BuildTris();

	// save the data out

	paliashdr->poseverts = numorder;

	auto cmds = static_cast<int *>(Hunk_Alloc(numcommands * 4));
	paliashdr->commands = reinterpret_cast<byte *>(cmds) - reinterpret_cast<byte *>(paliashdr);

	//johnfitz -- precompute texcoords for padded skins
	auto loadcmds = commands;
	while (true)
	{
		*cmds++ = count = *loadcmds++;

		if (!count)
			break;

		if (count < 0)
			count = -count;

		do
		{
			*reinterpret_cast<float *>(cmds++) = hscale * *reinterpret_cast<float *>(loadcmds++);
			*reinterpret_cast<float *>(cmds++) = vscale * *reinterpret_cast<float *>(loadcmds++);
		}
		while (--count);
	}
	//johnfitz

	auto verts = static_cast<trivertx_t *>(Hunk_Alloc(paliashdr->numposes * paliashdr->poseverts * sizeof(trivertx_t)));
	paliashdr->posedata = reinterpret_cast<byte *>(verts) - reinterpret_cast<byte *>(paliashdr);
	for (auto i = 0; i < paliashdr->numposes; i++)
		for (auto j = 0; j < numorder; j++)
			*verts++ = poseverts[i][vertexorder[j]];
}

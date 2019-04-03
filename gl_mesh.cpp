#include "quakedef.h"

/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/

model_t*    aliasmodel;
aliashdr_t* paliashdr;

qboolean used[8192];

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
	int          j;
	mtriangle_t* check;

	used[starttri] = 2;

	auto last = &triangles[starttri];

	stripverts[0] = last->vertindex[startv % 3];
	stripverts[1] = last->vertindex[(startv + 1) % 3];
	stripverts[2] = last->vertindex[(startv + 2) % 3];

	striptris[0] = starttri;
	stripcount   = 1;

	auto m1 = last->vertindex[(startv + 2) % 3];
	auto m2 = last->vertindex[(startv + 1) % 3];

	// look for a matching triangle
nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1]; j < pheader->numtris; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (auto k = 0; k < 3; k++)
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
			striptris[stripcount]      = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j = starttri + 1; j < pheader->numtris; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

/*
===========
FanLength
===========
*/
int FanLength(int starttri, int startv)
{
	int          j;
	mtriangle_t* check;

	used[starttri] = 2;

	auto last = &triangles[starttri];

	stripverts[0] = last->vertindex[startv % 3];
	stripverts[1] = last->vertindex[(startv + 1) % 3];
	stripverts[2] = last->vertindex[(startv + 2) % 3];

	striptris[0] = starttri;
	stripcount   = 1;

	const auto m1 = last->vertindex[(startv + 0) % 3];
	auto       m2 = last->vertindex[(startv + 2) % 3];


	// look for a matching triangle
nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1]; j < pheader->numtris; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (auto k = 0; k < 3; k++)
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
			striptris[stripcount]      = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j = starttri + 1; j < pheader->numtris; j++)
		if (used[j] == 2)
			used[j] = 0;

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
	int  j;
	int  len;
	auto besttype = 0;
	int  bestverts[1024];
	int  besttris[1024];

	//
	// build tristrips
	//
	numorder    = 0;
	numcommands = 0;
	memset(used, 0, sizeof used);
	for (auto i = 0; i < pheader->numtris; i++)
	{
		// pick an unused triangle and start the trifan
		if (used[i])
			continue;

		auto      bestlen = 0;
		for (auto type    = 0; type < 2; type++)
			//	type = 1;
		{
			for (auto startv = 0; startv < 3; startv++)
			{
				if (type == 1)
					len = StripLength(i, startv);
				else
					len = FanLength(i, startv);
				if (len > bestlen)
				{
					besttype         = type;
					bestlen          = len;
					for (j           = 0; j < bestlen + 2; j++)
						bestverts[j] = stripverts[j];
					for (j           = 0; j < bestlen; j++)
						besttris[j]  = striptris[j];
				}
			}
		}

		// mark the tris on the best strip as used
		for (j                = 0; j < bestlen; j++)
			used[besttris[j]] = 1;

		if (besttype == 1)
			commands[numcommands++] = bestlen + 2;
		else
			commands[numcommands++] = -(bestlen + 2);

		for (j = 0; j < bestlen + 2; j++)
		{
			// emit a vertex into the reorder buffer
			const auto k            = bestverts[j];
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
	char  cache[MAX_QPATH];
	char  fullpath[MAX_OSPATH];
	int   f;
	FILE* fp;
	aliasmodel = m;
	paliashdr  = hdr; // (aliashdr_t *)Mod_Extradata (m);

	//
	// look for a cached version
	//
	strcpy(cache, "glquake/");
	COM_StripExtension(m->name + strlen("progs/"), cache + strlen("glquake/"));
	strcat(cache, ".ms2");

	COM_FOpenFile(cache, &fp);
	if (fp)
	{
		fread(&numcommands, 4, 1, fp);
		fread(&numorder, 4, 1, fp);
		fread(&commands, numcommands * sizeof commands[0], 1, fp);
		fread(&vertexorder, numorder * sizeof vertexorder[0], 1, fp);
		fclose(fp);
	}
	else
	{
		//
		// build it from scratch
		//
		Con_Printf("meshing %s...\n", m->name);

		BuildTris(); // trifans or lists

		//
		// save out the cached version
		//
		sprintf(fullpath, "%s/%s", com_gamedir, cache);
		f = Sys_FileOpenWrite(fullpath);
		if (f)
		{
			Sys_FileWrite(f, &numcommands, 4);
			Sys_FileWrite(f, &numorder, 4);
			Sys_FileWrite(f, &commands, numcommands * sizeof commands[0]);
			Sys_FileWrite(f, &vertexorder, numorder * sizeof vertexorder[0]);
			Sys_FileClose(f);
		}
	}


	// save the data out

	paliashdr->poseverts = numorder;

	const auto cmds     = static_cast<int*>(Hunk_Alloc(numcommands * 4));
	paliashdr->commands = reinterpret_cast<byte *>(cmds) - reinterpret_cast<byte *>(paliashdr);
	memcpy(cmds, commands, numcommands * 4);

	auto verts          = static_cast<trivertx_t *>(Hunk_Alloc(paliashdr->numposes * paliashdr->poseverts * sizeof(trivertx_t)));
	paliashdr->posedata = reinterpret_cast<byte *>(verts) - reinterpret_cast<byte *>(paliashdr);
	for (auto     i     = 0; i < paliashdr->numposes; i++)
		for (auto j     = 0; j < numorder; j++)
			*verts++    = poseverts[i][vertexorder[j]];
}

#include "quakedef.h"

bool pr_alpha_supported; //johnfitz

dprograms_t* progs;
dfunction_t* pr_functions;
char* pr_strings;
ddef_t* pr_fielddefs;
ddef_t* pr_globaldefs;
dstatement_t* pr_statements;
globalvars_t* pr_global_struct;
float* pr_globals; // same as pr_global_struct
int pr_edict_size; // in bytes

unsigned short pr_crc;

int type_size[8] = {1,sizeof(string_t) / 4,1,3,1,1,sizeof(func_t) / 4,sizeof(void *) / 4};

ddef_t* ED_FieldAtOfs(int ofs);
bool ED_ParseEpair(void* base, ddef_t* key, char* s);

cvar_t nomonsters = {"nomonsters", "0"};
cvar_t gamecfg = {"gamecfg", "0"};
cvar_t scratch1 = {"scratch1", "0"};
cvar_t scratch2 = {"scratch2", "0"};
cvar_t scratch3 = {"scratch3", "0"};
cvar_t scratch4 = {"scratch4", "0"};
cvar_t savedgamecfg = {"savedgamecfg", "0", true};
cvar_t saved1 = {"saved1", "0", true};
cvar_t saved2 = {"saved2", "0", true};
cvar_t saved3 = {"saved3", "0", true};
cvar_t saved4 = {"saved4", "0", true};

#define	MAX_FIELD_LEN	64
#define GEFV_CACHESIZE	2

struct gefv_cache
{
	ddef_t* pcache;
	char field[MAX_FIELD_LEN];
};

static gefv_cache gefvCache[GEFV_CACHESIZE] = {{nullptr, ""}, {nullptr, ""}};

/*
=================
ED_ClearEdict

Sets everything to nullptr
=================
*/
void ED_ClearEdict(edict_t* e)
{
	memset(&e->v, 0, progs->entityfields * 4);
	e->free = false;
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t* ED_Alloc()
{
	int i;
	edict_t* e;

	for (i = svs.maxclients + 1; i < sv.num_edicts; i++)
	{
		e = EDICT_NUM(i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->free && (e->freetime < 2 || sv.time - e->freetime > 0.5))
		{
			ED_ClearEdict(e);
			return e;
		}
	}

	if (i == sv.max_edicts) //johnfitz -- use sv.max_edicts instead of MAX_EDICTS
		Host_Error("ED_Alloc: no free edicts (max_edicts is %i)", sv.max_edicts); //johnfitz -- was Sys_Error

	sv.num_edicts++;
	e = EDICT_NUM(i);
	ED_ClearEdict(e);

	return e;
}

/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and nullptr out references to this entity
=================
*/
void ED_Free(edict_t* ed)
{
	SV_UnlinkEdict(ed); // unlink from world bsp

	ed->free = true;
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorCopy (vec3_origin, ed->v.origin);
	VectorCopy (vec3_origin, ed->v.angles);
	ed->v.nextthink = -1;
	ed->v.solid = 0;
	ed->alpha = ENTALPHA_DEFAULT; //johnfitz -- reset alpha for next entity

	ed->freetime = sv.time;
}

//===========================================================================

/*
============
ED_GlobalAtOfs
============
*/
ddef_t* ED_GlobalAtOfs(int ofs)
{
	for (auto i = 0; i < progs->numglobaldefs; i++)
	{
		auto def = &pr_globaldefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return nullptr;
}

/*
============
ED_FieldAtOfs
============
*/
ddef_t* ED_FieldAtOfs(int ofs)
{
	for (auto i = 0; i < progs->numfielddefs; i++)
	{
		auto def = &pr_fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return nullptr;
}

/*
============
ED_FindField
============
*/
ddef_t* ED_FindField(char* name)
{
	for (auto i = 0; i < progs->numfielddefs; i++)
	{
		auto def = &pr_fielddefs[i];
		if (!strcmp(pr_strings + def->s_name, name))
			return def;
	}
	return nullptr;
}


/*
============
ED_FindGlobal
============
*/
ddef_t* ED_FindGlobal(char* name)
{
	for (auto i = 0; i < progs->numglobaldefs; i++)
	{
		auto def = &pr_globaldefs[i];
		if (!strcmp(pr_strings + def->s_name, name))
			return def;
	}
	return nullptr;
}


/*
============
ED_FindFunction
============
*/
dfunction_t* ED_FindFunction(char* name)
{
	for (auto i = 0; i < progs->numfunctions; i++)
	{
		auto func = &pr_functions[i];
		if (!strcmp(pr_strings + func->s_name, name))
			return func;
	}
	return nullptr;
}

/*
============
GetEdictFieldValue
============
*/
eval_t* GetEdictFieldValue(edict_t* ed, char* field)
{
	ddef_t* def = nullptr;
	static auto rep = 0;

	for (auto i = 0; i < GEFV_CACHESIZE; i++)
	{
		if (!strcmp(field, gefvCache[i].field))
		{
			def = gefvCache[i].pcache;
			goto Done;
		}
	}

	def = ED_FindField(field);

	if (strlen(field) < MAX_FIELD_LEN)
	{
		gefvCache[rep].pcache = def;
		strcpy(gefvCache[rep].field, field);
		rep ^= 1;
	}

Done:
	if (!def)
		return nullptr;

	return reinterpret_cast<eval_t *>(reinterpret_cast<char *>(&ed->v) + def->ofs * 4);
}


/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/
char* PR_ValueString(etype_t type, eval_t* val)
{
	static char line[256];
	*reinterpret_cast<std::underlying_type<etype_t>::type*>(&type) &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case etype_t::ev_string:
		sprintf(line, "%s", pr_strings + val->string);
		break;
	case etype_t::ev_entity:
		sprintf(line, "entity %i", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
		break;
	case etype_t::ev_function:
		auto f = pr_functions + val->function;
		sprintf(line, "%s()", pr_strings + f->s_name);
		break;
	case etype_t::ev_field:
		auto def = ED_FieldAtOfs(val->_int);
		sprintf(line, ".%s", pr_strings + def->s_name);
		break;
	case etype_t::ev_void:
		sprintf(line, "void");
		break;
	case etype_t::ev_float:
		sprintf(line, "%5.1f", val->_float);
		break;
	case etype_t::ev_vector:
		sprintf(line, "'%5.1f %5.1f %5.1f'", val->vector[0], val->vector[1], val->vector[2]);
		break;
	case etype_t::ev_pointer:
		sprintf(line, "pointer");
		break;
	default:
		sprintf(line, "bad type %i", type);
		break;
	}

	return line;
}

/*
============
PR_UglyValueString

Returns a string describing *data in a type specific manner
Easier to parse than PR_ValueString
=============
*/
char* PR_UglyValueString(etype_t type, eval_t* val)
{
	static char line[256];

	*reinterpret_cast<std::underlying_type<etype_t>::type*>(&type) &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case etype_t::ev_string:
		sprintf(line, "%s", pr_strings + val->string);
		break;
	case etype_t::ev_entity:
		sprintf(line, "%i", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
		break;
	case etype_t::ev_function:
		auto f = pr_functions + val->function;
		sprintf(line, "%s", pr_strings + f->s_name);
		break;
	case etype_t::ev_field:
		ddef_t* def = ED_FieldAtOfs(val->_int);
		sprintf(line, "%s", pr_strings + def->s_name);
		break;
	case etype_t::ev_void:
		sprintf(line, "void");
		break;
	case etype_t::ev_float:
		sprintf(line, "%f", val->_float);
		break;
	case etype_t::ev_vector:
		sprintf(line, "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
		break;
	default:
		sprintf(line, "bad type %i", type);
		break;
	}

	return line;
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
char* PR_GlobalString(int ofs)
{
	static char line[128];

	auto val = reinterpret_cast<eval_t *>(&pr_globals[ofs]);
	auto def = ED_GlobalAtOfs(ofs);
	if (!def)
		sprintf(line, "%i(???)", ofs);
	else
	{
		auto s = PR_ValueString(static_cast<etype_t>(def->type), val);
		sprintf(line, "%i(%s)%s", ofs, pr_strings + def->s_name, s);
	}

	int i = strlen(line);
	for (; i < 20; i++)
		strcat(line, " ");
	strcat(line, " ");

	return line;
}

char* PR_GlobalStringNoContents(int ofs)
{
	static char line[128];

	auto def = ED_GlobalAtOfs(ofs);
	if (!def)
		sprintf(line, "%i(???)", ofs);
	else
		sprintf(line, "%i(%s)", ofs, pr_strings + def->s_name);

	int i = strlen(line);
	for (; i < 20; i++)
		strcat(line, " ");
	strcat(line, " ");

	return line;
}


/*
=============
ED_Print

For debugging
=============
*/
void ED_Print(edict_t* ed)
{
	int j;

	if (ed->free)
	{
		Con_Printf("FREE\n");
		return;
	}

	Con_SafePrintf("\nEDICT %i:\n", NUM_FOR_EDICT(ed)); //johnfitz -- was Con_Printf
	for (auto i = 1; i < progs->numfielddefs; i++)
	{
		auto d = &pr_fielddefs[i];
		auto name = pr_strings + d->s_name;
		if (name[strlen(name) - 2] == '_')
			continue; // skip _x, _y, _z vars

		auto v = reinterpret_cast<int *>(reinterpret_cast<char *>(&ed->v) + d->ofs * 4);

		// if the value is still all 0, skip the field
		auto type = d->type & ~DEF_SAVEGLOBAL;

		for (j = 0; j < type_size[type]; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		Con_SafePrintf("%s", name); //johnfitz -- was Con_Printf
		int l = strlen(name);
		while (l++ < 15)
			Con_SafePrintf(" "); //johnfitz -- was Con_Printf

		Con_SafePrintf("%s\n", PR_ValueString(static_cast<etype_t>(d->type), reinterpret_cast<eval_t *>(v))); //johnfitz -- was Con_Printf
	}
}

/*
=============
ED_Write

For savegames
=============
*/
void ED_Write(FILE* f, edict_t* ed)
{
	int j;

	fprintf(f, "{\n");

	if (ed->free)
	{
		fprintf(f, "}\n");
		return;
	}

	for (auto i = 1; i < progs->numfielddefs; i++)
	{
		auto d = &pr_fielddefs[i];
		auto name = pr_strings + d->s_name;
		if (name[strlen(name) - 2] == '_')
			continue; // skip _x, _y, _z vars

		auto v = reinterpret_cast<int *>(reinterpret_cast<char *>(&ed->v) + d->ofs * 4);

		// if the value is still all 0, skip the field
		auto type = d->type & ~DEF_SAVEGLOBAL;
		for (j = 0; j < type_size[type]; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		fprintf(f, "\"%s\" ", name);
		fprintf(f, "\"%s\"\n", PR_UglyValueString(static_cast<etype_t>(d->type), reinterpret_cast<eval_t *>(v)));
	}

	//johnfitz -- save entity alpha manually when progs.dat doesn't know about alpha
	if (!pr_alpha_supported && ed->alpha != ENTALPHA_DEFAULT)
		fprintf(f, "\"alpha\" \"%f\"\n", ENTALPHA_TOSAVE(ed->alpha));
	//johnfitz

	fprintf(f, "}\n");
}

void ED_PrintNum(int ent)
{
	ED_Print(EDICT_NUM(ent));
}

/*
=============
ED_PrintEdicts

For debugging, prints all the entities in the current server
=============
*/
void ED_PrintEdicts()
{
	Con_Printf("%i entities\n", sv.num_edicts);
	for (auto i = 0; i < sv.num_edicts; i++)
		ED_PrintNum(i);
}

/*
=============
ED_PrintEdict_f

For debugging, prints a single edicy
=============
*/
void ED_PrintEdict_f()
{
	auto i = Q_atoi(Cmd_Argv(1));
	if (i >= sv.num_edicts)
	{
		Con_Printf("Bad edict number\n");
		return;
	}
	ED_PrintNum(i);
}

/*
=============
ED_Count

For debugging
=============
*/
void ED_Count()
{
	int models, solid, step;

	auto active = models = solid = step = 0;
	for (auto i = 0; i < sv.num_edicts; i++)
	{
		auto ent = EDICT_NUM(i);
		if (ent->free)
			continue;
		active++;
		if (ent->v.solid)
			solid++;
		if (ent->v.model)
			models++;
		if (ent->v.movetype == MOVETYPE_STEP)
			step++;
	}

	Con_Printf("num_edicts:%3i\n", sv.num_edicts);
	Con_Printf("active    :%3i\n", active);
	Con_Printf("view      :%3i\n", models);
	Con_Printf("touch     :%3i\n", solid);
	Con_Printf("step      :%3i\n", step);
}

/*
==============================================================================

					ARCHIVING GLOBALS

FIXME: need to tag constants, doesn't really work
==============================================================================
*/

/*
=============
ED_WriteGlobals
=============
*/
void ED_WriteGlobals(FILE* f)
{
	fprintf(f, "{\n");
	for (auto i = 0; i < progs->numglobaldefs; i++)
	{
		auto def = &pr_globaldefs[i];
		int type = def->type;
		if (!(def->type & DEF_SAVEGLOBAL))
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != static_cast<int>(etype_t::ev_string)
			&& type != static_cast<int>(etype_t::ev_float)
			&& type != static_cast<int>(etype_t::ev_entity))
			continue;

		char* name = pr_strings + def->s_name;
		fprintf(f, "\"%s\" ", name);
		fprintf(f, "\"%s\"\n", PR_UglyValueString(static_cast<etype_t>(type), reinterpret_cast<eval_t *>(&pr_globals[def->ofs])));
	}
	fprintf(f, "}\n");
}

/*
=============
ED_ParseGlobals
=============
*/
void ED_ParseGlobals(char* data)
{
	char keyname[64];

	while (true)
	{
		// parse key
		data = COM_Parse(data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Sys_Error("ED_ParseEntity: EOF without closing brace");

		strcpy(keyname, com_token);

		// parse value
		data = COM_Parse(data);
		if (!data)
			Sys_Error("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Sys_Error("ED_ParseEntity: closing brace without data");

		auto key = ED_FindGlobal(keyname);
		if (!key)
		{
			Con_Printf("'%s' is not a global\n", keyname);
			continue;
		}

		if (!ED_ParseEpair(static_cast<void *>(pr_globals), key, com_token))
			Host_Error("ED_ParseGlobals: parse error");
	}
}

//============================================================================


/*
=============
ED_NewString
=============
*/
char* ED_NewString(char* string)
{
	int l = strlen(string) + 1;
	auto new_x = reinterpret_cast<char*>(Hunk_Alloc(l));
	auto new_p = new_x;

	for (auto i = 0; i < l; i++)
	{
		if (string[i] == '\\' && i < l - 1)
		{
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		}
		else
			*new_p++ = string[i];
	}

	return new_x;
}


/*
=============
ED_ParseEval

Can parse either fields or globals
returns false if error
=============
*/
bool ED_ParseEpair(void* base, ddef_t* key, char* s)
{
	char string[128];
	ddef_t* def;

	auto d = static_cast<void *>(static_cast<int *>(base) + key->ofs);

	switch (static_cast<etype_t>(key->type & ~DEF_SAVEGLOBAL))
	{
	case etype_t::ev_string:
		*static_cast<string_t *>(d) = ED_NewString(s) - pr_strings;
		break;

	case etype_t::ev_float:
		*static_cast<float *>(d) = atof(s);
		break;

	case etype_t::ev_vector:
		strcpy(string, s);
		auto v = string;
		auto w = string;
		for (auto i = 0; i < 3; i++)
		{
			while (*v && *v != ' ')
				v++;
			*v = 0;
			static_cast<float *>(d)[i] = atof(w);
			w = v = v + 1;
		}
		break;

	case etype_t::ev_entity:
		*static_cast<int *>(d) = EDICT_TO_PROG(EDICT_NUM(atoi (s)));
		break;

	case etype_t::ev_field:
		def = ED_FindField(s);
		if (!def)
		{
			//johnfitz -- HACK -- suppress error becuase fog/sky fields might not be mentioned in defs.qc
			if (strncmp(s, "sky", 3) && strcmp(s, "fog"))
				Con_DPrintf("Can't find field %s\n", s);
			return false;
		}
		*static_cast<int *>(d) = G_INT(def->ofs);
		break;

	case etype_t::ev_function:
		dfunction_t* func = ED_FindFunction(s);
		if (!func)
		{
			Con_Printf("Can't find function %s\n", s);
			return false;
		}
		*static_cast<func_t *>(d) = func - pr_functions;
		break;

	default:
		break;
	}
	return true;
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Used for initial level load and for savegames.
====================
*/
char* ED_ParseEdict(char* data, edict_t* ent)
{
	bool anglehack;
	char keyname[256];

	auto init = false;

	// clear it
	if (ent != sv.edicts) // hack
		memset(&ent->v, 0, progs->entityfields * 4);

	// go through all the dictionary pairs
	while (true)
	{
		// parse key
		data = COM_Parse(data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Sys_Error("ED_ParseEntity: EOF without closing brace");

		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if (!strcmp(com_token, "angle"))
		{
			strcpy(com_token, "angles");
			anglehack = true;
		}
		else
			anglehack = false;

		// FIXME: change light to _light to get rid of this hack
		if (!strcmp(com_token, "light"))
			strcpy(com_token, "light_lev"); // hack for single light def

		strcpy(keyname, com_token);

		// another hack to fix keynames with trailing spaces
		int n = strlen(keyname);
		while (n && keyname[n - 1] == ' ')
		{
			keyname[n - 1] = 0;
			n--;
		}

		// parse value
		data = COM_Parse(data);
		if (!data)
			Sys_Error("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Sys_Error("ED_ParseEntity: closing brace without data");

		init = true;

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;

		//johnfitz -- hack to support .alpha even when progs.dat doesn't know about it
		if (!strcmp(keyname, "alpha"))
			ent->alpha = ENTALPHA_ENCODE(atof(com_token));
		//johnfitz

		ddef_t* key = ED_FindField(keyname);
		if (!key)
		{
			//johnfitz -- HACK -- suppress error becuase fog/sky/alpha fields might not be mentioned in defs.qc
			if (strncmp(keyname, "sky", 3) && strcmp(keyname, "fog") && strcmp(keyname, "alpha"))
				Con_DPrintf("\"%s\" is not a field\n", keyname); //johnfitz -- was Con_Printf
			continue;
		}

		if (anglehack)
		{
			char temp[32];
			strcpy(temp, com_token);
			sprintf(com_token, "0 %s 0", temp);
		}

		if (!ED_ParseEpair(static_cast<void *>(&ent->v), key, com_token))
			Host_Error("ED_ParseEdict: parse error");
	}

	if (!init)
		ent->free = true;

	return data;
}


/*
================
ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call ED_CallSpawnFunctions () to let the objects initialize themselves.
================
*/
void ED_LoadFromFile(char* data)
{
	edict_t* ent = nullptr;
	auto inhibit = 0;
	pr_global_struct->time = sv.time;

	// parse ents
	while (true)
	{
		// parse the opening brace
		data = COM_Parse(data);
		if (!data)
			break;
		if (com_token[0] != '{')
			Sys_Error("ED_LoadFromFile: found %s when expecting {", com_token);

		if (!ent)
			ent = EDICT_NUM(0);
		else
			ent = ED_Alloc();
		data = ED_ParseEdict(data, ent);

		// remove things from different skill levels or deathmatch
		if (deathmatch.value)
		{
			if (static_cast<int>(ent->v.spawnflags) & SPAWNFLAG_NOT_DEATHMATCH)
			{
				ED_Free(ent);
				inhibit++;
				continue;
			}
		}
		else if (current_skill == 0 && static_cast<int>(ent->v.spawnflags) & SPAWNFLAG_NOT_EASY
			|| current_skill == 1 && static_cast<int>(ent->v.spawnflags) & SPAWNFLAG_NOT_MEDIUM
			|| current_skill >= 2 && static_cast<int>(ent->v.spawnflags) & SPAWNFLAG_NOT_HARD)
		{
			ED_Free(ent);
			inhibit++;
			continue;
		}

		//
		// immediately call spawn function
		//
		if (!ent->v.classname)
		{
			Con_SafePrintf("No classname for:\n"); //johnfitz -- was Con_Printf
			ED_Print(ent);
			ED_Free(ent);
			continue;
		}

		// look for the spawn function
		dfunction_t* func = ED_FindFunction(pr_strings + ent->v.classname);

		if (!func)
		{
			Con_SafePrintf("No spawn function for:\n"); //johnfitz -- was Con_Printf
			ED_Print(ent);
			ED_Free(ent);
			continue;
		}

		pr_global_struct->self = EDICT_TO_PROG(ent);
		PR_ExecuteProgram(func - pr_functions);
	}

	Con_DPrintf("%i entities inhibited\n", inhibit);
}


/*
===============
PR_LoadProgs
===============
*/
void PR_LoadProgs()
{
	int i;

	// flush the non-C variable lookup cache
	for (i = 0; i < GEFV_CACHESIZE; i++)
		gefvCache[i].field[0] = 0;

	CRC_Init(&pr_crc);

	progs = reinterpret_cast<dprograms_t *>(COM_LoadHunkFile("progs.dat"));
	if (!progs)
		Sys_Error("PR_LoadProgs: couldn't load progs.dat");
	Con_DPrintf("Programs occupy %iK.\n", com_filesize / 1024);

	for (i = 0; i < com_filesize; i++)
		CRC_ProcessByte(&pr_crc, reinterpret_cast<byte *>(progs)[i]);

	// byte swap the header
	for (i = 0; i < sizeof*progs / 4; i++)
		reinterpret_cast<int *>(progs)[i] = LittleLong(reinterpret_cast<int *>(progs)[i]);

	if (progs->version != PROG_VERSION)
		Sys_Error("progs.dat has wrong version number (%i should be %i)", progs->version, PROG_VERSION);
	if (progs->crc != PROGHEADER_CRC)
		Sys_Error("progs.dat system vars have been modified, progdefs.h is out of date");

	pr_functions = reinterpret_cast<dfunction_t *>(reinterpret_cast<byte *>(progs) + progs->ofs_functions);
	pr_strings = reinterpret_cast<char *>(progs) + progs->ofs_strings;
	pr_globaldefs = reinterpret_cast<ddef_t *>(reinterpret_cast<byte *>(progs) + progs->ofs_globaldefs);
	pr_fielddefs = reinterpret_cast<ddef_t *>(reinterpret_cast<byte *>(progs) + progs->ofs_fielddefs);
	pr_statements = reinterpret_cast<dstatement_t *>(reinterpret_cast<byte *>(progs) + progs->ofs_statements);

	pr_global_struct = reinterpret_cast<globalvars_t *>(reinterpret_cast<byte *>(progs) + progs->ofs_globals);
	pr_globals = reinterpret_cast<float *>(pr_global_struct);

	pr_edict_size = progs->entityfields * 4 + sizeof (edict_t) - sizeof(entvars_t);

	// byte swap the lumps
	for (i = 0; i < progs->numstatements; i++)
	{
		pr_statements[i].op = LittleShort(pr_statements[i].op);
		pr_statements[i].a = LittleShort(pr_statements[i].a);
		pr_statements[i].b = LittleShort(pr_statements[i].b);
		pr_statements[i].c = LittleShort(pr_statements[i].c);
	}

	for (i = 0; i < progs->numfunctions; i++)
	{
		pr_functions[i].first_statement = LittleLong(pr_functions[i].first_statement);
		pr_functions[i].parm_start = LittleLong(pr_functions[i].parm_start);
		pr_functions[i].s_name = LittleLong(pr_functions[i].s_name);
		pr_functions[i].s_file = LittleLong(pr_functions[i].s_file);
		pr_functions[i].numparms = LittleLong(pr_functions[i].numparms);
		pr_functions[i].locals = LittleLong(pr_functions[i].locals);
	}

	for (i = 0; i < progs->numglobaldefs; i++)
	{
		pr_globaldefs[i].type = LittleShort(pr_globaldefs[i].type);
		pr_globaldefs[i].ofs = LittleShort(pr_globaldefs[i].ofs);
		pr_globaldefs[i].s_name = LittleLong(pr_globaldefs[i].s_name);
	}

	pr_alpha_supported = false; //johnfitz

	for (i = 0; i < progs->numfielddefs; i++)
	{
		pr_fielddefs[i].type = LittleShort(pr_fielddefs[i].type);
		if (pr_fielddefs[i].type & DEF_SAVEGLOBAL)
			Sys_Error("PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");
		pr_fielddefs[i].ofs = LittleShort(pr_fielddefs[i].ofs);
		pr_fielddefs[i].s_name = LittleLong(pr_fielddefs[i].s_name);

		//johnfitz -- detect alpha support in progs.dat
		if (!strcmp(pr_strings + pr_fielddefs[i].s_name, "alpha"))
			pr_alpha_supported = true;
		//johnfitz
	}

	for (i = 0; i < progs->numglobals; i++)
		reinterpret_cast<int *>(pr_globals)[i] = LittleLong(reinterpret_cast<int *>(pr_globals)[i]);
}


/*
===============
PR_Init
===============
*/
void PR_Init()
{
	Cmd_AddCommand("edict", ED_PrintEdict_f);
	Cmd_AddCommand("edicts", ED_PrintEdicts);
	Cmd_AddCommand("edictcount", ED_Count);
	Cmd_AddCommand("profile", PR_Profile_f);
	Cvar_RegisterVariable(&nomonsters, nullptr);
	Cvar_RegisterVariable(&gamecfg, nullptr);
	Cvar_RegisterVariable(&scratch1, nullptr);
	Cvar_RegisterVariable(&scratch2, nullptr);
	Cvar_RegisterVariable(&scratch3, nullptr);
	Cvar_RegisterVariable(&scratch4, nullptr);
	Cvar_RegisterVariable(&savedgamecfg, nullptr);
	Cvar_RegisterVariable(&saved1, nullptr);
	Cvar_RegisterVariable(&saved2, nullptr);
	Cvar_RegisterVariable(&saved3, nullptr);
	Cvar_RegisterVariable(&saved4, nullptr);
}


edict_t* EDICT_NUM(int n)
{
	if (n < 0 || n >= sv.max_edicts)
		Sys_Error("EDICT_NUM: bad number %i", n);
	return reinterpret_cast<edict_t *>(reinterpret_cast<byte *>(sv.edicts) + n * pr_edict_size);
}

int NUM_FOR_EDICT(edict_t* e)
{
	auto b = reinterpret_cast<byte *>(e) - reinterpret_cast<byte *>(sv.edicts);
	b = b / pr_edict_size;

	if (b < 0 || b >= sv.num_edicts)
		Sys_Error("NUM_FOR_EDICT: bad pointer");
	return b;
}

#include "quakedef.h"

//==============================================================================
//
//  GLOBAL FOG
//
//==============================================================================

float fog_density;
float fog_red;
float fog_green;
float fog_blue;

float old_density;
float old_red;
float old_green;
float old_blue;

float fade_time; //duration of fade
float fade_done; //time when fade will be done

/*
=============
Fog_Update

update internal variables
=============
*/
void Fog_Update(float density, float red, float green, float blue, float time)
{
	//save previous settings for fade
	if (time > 0)
	{
		//check for a fade in progress
		if (fade_done > cl.time)
		{
			float f = (fade_done - cl.time) / fade_time;
			old_density = f * old_density + (1.0 - f) * fog_density;
			old_red = f * old_red + (1.0 - f) * fog_red;
			old_green = f * old_green + (1.0 - f) * fog_green;
			old_blue = f * old_blue + (1.0 - f) * fog_blue;
		}
		else
		{
			old_density = fog_density;
			old_red = fog_red;
			old_green = fog_green;
			old_blue = fog_blue;
		}
	}

	fog_density = density;
	fog_red = red;
	fog_green = green;
	fog_blue = blue;
	fade_time = time;
	fade_done = cl.time + time;
}

/*
=============
Fog_ParseServerMessage

handle an SVC_FOG message from server
=============
*/
void Fog_ParseServerMessage()
{
	float density = MSG_ReadByte() / 255.0;
	float red = MSG_ReadByte() / 255.0;
	float green = MSG_ReadByte() / 255.0;
	float blue = MSG_ReadByte() / 255.0;
	float time = max(0.0, MSG_ReadShort() / 100.0);

	Fog_Update(density, red, green, blue, time);
}

/*
=============
Fog_FogCommand_f

handle the 'fog' console command
=============
*/
void Fog_FogCommand_f()
{
	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf("usage:\n");
		Con_Printf("   fog <density>\n");
		Con_Printf("   fog <red> <green> <blue>\n");
		Con_Printf("   fog <density> <red> <green> <blue>\n");
		Con_Printf("current values:\n");
		Con_Printf("   \"density\" is \"%f\"\n", fog_density);
		Con_Printf("   \"red\" is \"%f\"\n", fog_red);
		Con_Printf("   \"green\" is \"%f\"\n", fog_green);
		Con_Printf("   \"blue\" is \"%f\"\n", fog_blue);
		break;
	case 2:
		Fog_Update(max(0.0, atof(Cmd_Argv(1))),
		           fog_red,
		           fog_green,
		           fog_blue,
		           0.0);
		break;
	case 3: //TEST
		Fog_Update(max(0.0, atof(Cmd_Argv(1))),
		           fog_red,
		           fog_green,
		           fog_blue,
		           atof(Cmd_Argv(2)));
		break;
	case 4:
		Fog_Update(fog_density,
		           CLAMP(0.0, atof(Cmd_Argv(1)), 1.0),
		           CLAMP(0.0, atof(Cmd_Argv(2)), 1.0),
		           CLAMP(0.0, atof(Cmd_Argv(3)), 1.0),
		           0.0);
		break;
	case 5:
		Fog_Update(max(0.0, atof(Cmd_Argv(1))),
		           CLAMP(0.0, atof(Cmd_Argv(2)), 1.0),
		           CLAMP(0.0, atof(Cmd_Argv(3)), 1.0),
		           CLAMP(0.0, atof(Cmd_Argv(4)), 1.0),
		           0.0);
		break;
	case 6: //TEST
		Fog_Update(max(0.0, atof(Cmd_Argv(1))),
		           CLAMP(0.0, atof(Cmd_Argv(2)), 1.0),
		           CLAMP(0.0, atof(Cmd_Argv(3)), 1.0),
		           CLAMP(0.0, atof(Cmd_Argv(4)), 1.0),
		           atof(Cmd_Argv(5)));
		break;
	}
}

/*
=============
Fog_ParseWorldspawn

called at map load
=============
*/
void Fog_ParseWorldspawn()
{
	char key[128], value[4096];

	//initially no fog
	fog_density = 0.0;
	old_density = 0.0;
	fade_time = 0.0;
	fade_done = 0.0;

	auto data = COM_Parse(cl.worldmodel->entities);
	if (!data)
		return; // error
	if (com_token[0] != '{')
		return; // error
	while (true)
	{
		data = COM_Parse(data);
		if (!data)
			return; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strcpy(key, com_token + 1);
		else
			strcpy(key, com_token);
		while (key[strlen(key) - 1] == ' ') // remove trailing spaces
			key[strlen(key) - 1] = 0;
		data = COM_Parse(data);
		if (!data)
			return; // error
		strcpy(value, com_token);

		if (!strcmp("fog", key))
		{
			sscanf(value, "%f %f %f %f", &fog_density, &fog_red, &fog_green, &fog_blue);
		}
	}
}

/*
=============
Fog_GetColor

calculates fog color for this frame, taking into account fade times
=============
*/
float* Fog_GetColor()
{
	static float c[4];
	int i;

	if (fade_done > cl.time)
	{
		float f = (fade_done - cl.time) / fade_time;
		c[0] = f * old_red + (1.0 - f) * fog_red;
		c[1] = f * old_green + (1.0 - f) * fog_green;
		c[2] = f * old_blue + (1.0 - f) * fog_blue;
		c[3] = 1.0;
	}
	else
	{
		c[0] = fog_red;
		c[1] = fog_green;
		c[2] = fog_blue;
		c[3] = 1.0;
	}

	//find closest 24-bit RGB value, so solid-colored sky can match the fog perfectly
	for (i = 0; i < 3; i++)
		c[i] = static_cast<float>(Q_rint(c[i] * 255)) / 255.0f;

	return c;
}

/*
=============
Fog_GetDensity

returns current density of fog
=============
*/
float Fog_GetDensity()
{
	if (fade_done > cl.time)
	{
		float f = (fade_done - cl.time) / fade_time;
		return f * old_density + (1.0 - f) * fog_density;
	}
	return fog_density;
}

/*
=============
Fog_SetupFrame

called at the beginning of each frame
=============
*/
void Fog_SetupFrame()
{
	glFogfv(GL_FOG_COLOR, Fog_GetColor());
	glFogf(GL_FOG_DENSITY, Fog_GetDensity() / 64.0);
}

/*
=============
Fog_EnableGFog

called before drawing stuff that should be fogged
=============
*/
void Fog_EnableGFog()
{
	if (Fog_GetDensity() > 0)
		glEnable(GL_FOG);
}

/*
=============
Fog_DisableGFog

called after drawing stuff that should be fogged
=============
*/
void Fog_DisableGFog()
{
	if (Fog_GetDensity() > 0)
		glDisable(GL_FOG);
}

/*
=============
Fog_StartAdditive

called before drawing stuff that is additive blended -- sets fog color to black
=============
*/
void Fog_StartAdditive()
{
	vec3_t color = {0,0,0};

	if (Fog_GetDensity() > 0)
		glFogfv(GL_FOG_COLOR, color);
}

/*
=============
Fog_StopAdditive

called after drawing stuff that is additive blended -- restores fog color
=============
*/
void Fog_StopAdditive()
{
	if (Fog_GetDensity() > 0)
		glFogfv(GL_FOG_COLOR, Fog_GetColor());
}

//==============================================================================
//
//  VOLUMETRIC FOG
//
//==============================================================================

cvar_t r_vfog = {"r_vfog", "1"};

void Fog_DrawVFog()
{
}

void Fog_MarkModels()
{
}

//==============================================================================
//
//  INIT
//
//==============================================================================

/*
=============
Fog_NewMap

called whenever a map is loaded
=============
*/
void Fog_NewMap()
{
	Fog_ParseWorldspawn(); //for global fog
	Fog_MarkModels(); //for volumetric fog
}

/*
=============
Fog_Init

called when quake initializes
=============
*/
void Fog_Init()
{
	Cmd_AddCommand("fog", Fog_FogCommand_f);

	//Cvar_RegisterVariable (&r_vfog, nullptr);

	//set up global fog
	fog_density = 0.0;
	fog_red = 0.3;
	fog_green = 0.3;
	fog_blue = 0.3;

	glFogi(GL_FOG_MODE, GL_EXP2);
}

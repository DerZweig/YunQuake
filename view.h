#pragma once


extern byte  gammatable[256]; // palette is sent through this
extern byte  ramps[3][256];
extern float v_blend[4];


void  V_Init();
void  V_RenderView();
float V_CalcRoll(vec3_t angles, vec3_t velocity);
void  V_UpdatePalette();
void  V_CalcBlend();

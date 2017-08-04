extern cvar_t vid_gamma;

extern float v_blend[4];

void V_Init();
void V_RenderView();
float V_CalcRoll(vec3_t angles, vec3_t velocity);
//void V_UpdatePalette(); //johnfitz

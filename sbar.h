#pragma once
#define	SBAR_HEIGHT		24

extern int sb_lines; // scan lines to draw

void Sbar_Init();

void Sbar_Changed();
// call whenever any of the client stats represented on the sbar changes

void Sbar_Draw();
// called every frame by screen

void Sbar_IntermissionOverlay();
// called each frame after the level has been completed

void Sbar_FinaleOverlay();

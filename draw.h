#pragma once
enum class canvastype;
extern	qpic_t		*draw_disc;	// also used on sbar

void Draw_Init();
void Draw_Character (int x, int y, int num);
void Draw_DebugChar (char num);
void Draw_Pic (int x, int y, qpic_t *pic);
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, int top, int bottom); //johnfitz -- more parameters
void Draw_ConsoleBackground(); //johnfitz -- removed parameter int lines
void Draw_BeginDisc();
void Draw_TileClear (int x, int y, int w, int h);
void Draw_Fill (int x, int y, int w, int h, int c, float alpha); //johnfitz -- added alpha
void Draw_FadeScreen();
void Draw_String (int x, int y, char *str);
qpic_t *Draw_PicFromWad (char *name);
qpic_t *Draw_CachePic (char *path);

void GL_SetCanvas (canvastype canvastype); //johnfitz

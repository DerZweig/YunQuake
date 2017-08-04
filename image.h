#pragma once
byte* Image_LoadTGA(FILE* f, int* width, int* height);
byte* Image_LoadPCX(FILE* f, int* width, int* height);
byte* Image_LoadImage(char* name, int* width, int* height);

bool Image_WriteTGA(char* name, byte* data, int width, int height, int bpp, bool upsidedown);

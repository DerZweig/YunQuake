#include "quakedef.h"

extern bool mtexenabled; //johnfitz
extern cvar_t r_drawflat, gl_overbright_models, gl_fullbrights, r_lerpmodels, r_lerpmove; //johnfitz

//up to 16 color translated skins
gltexture_t* playertextures[MAX_SCOREBOARD]; //johnfitz -- changed to an array of pointers

#define NUMVERTEXNORMALS	162

float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t shadevector;

extern vec3_t lightcolor; //johnfitz -- replaces "float shadelight" for lit support

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float r_avertexnormal_dots[SHADEDOT_QUANT][256] = {
	{1.23,1.30,1.47,1.35,1.56,1.71,1.37,1.38,1.59,1.60,1.79,1.97,1.88,1.92,1.79,1.02,0.93,1.07,0.82,0.87,0.88,0.94,0.96,1.14,1.11,0.82,0.83,0.89,0.89,0.86,0.94,0.91,1.00,1.21,0.98,1.48,1.30,1.57,0.96,1.07,1.14,1.60,1.61,1.40,1.37,1.72,1.78,1.79,1.93,1.99,1.90,1.68,1.71,1.86,1.60,1.68,1.78,1.86,1.93,1.99,1.97,1.44,1.22,1.49,0.93,0.99,0.99,1.23,1.22,1.44,1.49,0.89,0.89,0.97,0.91,0.98,1.19,0.82,0.76,0.82,0.71,0.72,0.73,0.76,0.79,0.86,0.83,0.72,0.76,0.76,0.89,0.82,0.89,0.82,0.89,0.91,0.83,0.96,1.14,0.97,1.40,1.19,0.98,0.94,1.00,1.07,1.37,1.21,1.48,1.30,1.57,1.61,1.37,0.86,0.83,0.91,0.82,0.82,0.88,0.89,0.96,1.14,0.98,0.87,0.93,0.94,1.02,1.30,1.07,1.35,1.38,1.11,1.56,1.92,1.79,1.79,1.59,1.60,1.72,1.90,1.79,0.80,0.85,0.79,0.93,0.80,0.85,0.77,0.74,0.72,0.77,0.74,0.72,0.70,0.70,0.71,0.76,0.73,0.79,0.79,0.73,0.76,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.26,1.26,1.48,1.23,1.50,1.71,1.14,1.19,1.38,1.46,1.64,1.94,1.87,1.84,1.71,1.02,0.92,1.00,0.79,0.85,0.84,0.91,0.90,0.98,0.99,0.77,0.77,0.83,0.82,0.79,0.86,0.84,0.92,0.99,0.91,1.24,1.03,1.33,0.88,0.94,0.97,1.41,1.39,1.18,1.11,1.51,1.61,1.59,1.80,1.91,1.76,1.54,1.65,1.76,1.70,1.70,1.85,1.85,1.97,1.99,1.93,1.28,1.09,1.39,0.92,0.97,0.99,1.18,1.26,1.52,1.48,0.83,0.85,0.90,0.88,0.93,1.00,0.77,0.73,0.78,0.72,0.71,0.74,0.75,0.79,0.86,0.81,0.75,0.81,0.79,0.96,0.88,0.94,0.86,0.93,0.92,0.85,1.08,1.33,1.05,1.55,1.31,1.01,1.05,1.27,1.31,1.60,1.47,1.70,1.54,1.76,1.76,1.57,0.93,0.90,0.99,0.88,0.88,0.95,0.97,1.11,1.39,1.20,0.92,0.97,1.01,1.10,1.39,1.22,1.51,1.58,1.32,1.64,1.97,1.85,1.91,1.77,1.74,1.88,1.99,1.91,0.79,0.86,0.80,0.94,0.84,0.88,0.74,0.74,0.71,0.82,0.77,0.76,0.70,0.73,0.72,0.73,0.70,0.74,0.85,0.77,0.82,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.34,1.27,1.53,1.17,1.46,1.71,0.98,1.05,1.20,1.34,1.48,1.86,1.82,1.71,1.62,1.09,0.94,0.99,0.79,0.85,0.82,0.90,0.87,0.93,0.96,0.76,0.74,0.79,0.76,0.74,0.79,0.78,0.85,0.92,0.85,1.00,0.93,1.06,0.81,0.86,0.89,1.16,1.12,0.97,0.95,1.28,1.38,1.35,1.60,1.77,1.57,1.33,1.50,1.58,1.69,1.63,1.82,1.74,1.91,1.92,1.80,1.04,0.97,1.21,0.90,0.93,0.97,1.05,1.21,1.48,1.37,0.77,0.80,0.84,0.85,0.88,0.92,0.73,0.71,0.74,0.74,0.71,0.75,0.73,0.79,0.84,0.78,0.79,0.86,0.81,1.05,0.94,0.99,0.90,0.95,0.92,0.86,1.24,1.44,1.14,1.59,1.34,1.02,1.27,1.50,1.49,1.80,1.69,1.86,1.72,1.87,1.80,1.69,1.00,0.98,1.23,0.95,0.96,1.09,1.16,1.37,1.63,1.46,0.99,1.10,1.25,1.24,1.51,1.41,1.67,1.77,1.55,1.72,1.95,1.89,1.98,1.91,1.86,1.97,1.99,1.94,0.81,0.89,0.85,0.98,0.90,0.94,0.75,0.78,0.73,0.89,0.83,0.82,0.72,0.77,0.76,0.72,0.70,0.71,0.91,0.83,0.89,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.46,1.34,1.60,1.16,1.46,1.71,0.94,0.99,1.05,1.26,1.33,1.74,1.76,1.57,1.54,1.23,0.98,1.05,0.83,0.89,0.84,0.92,0.87,0.91,0.96,0.78,0.74,0.79,0.72,0.72,0.75,0.76,0.80,0.88,0.83,0.94,0.87,0.95,0.76,0.80,0.82,0.97,0.96,0.89,0.88,1.08,1.11,1.10,1.37,1.59,1.37,1.07,1.27,1.34,1.57,1.45,1.69,1.55,1.77,1.79,1.60,0.93,0.90,0.99,0.86,0.87,0.93,0.96,1.07,1.35,1.18,0.73,0.76,0.77,0.81,0.82,0.85,0.70,0.71,0.72,0.78,0.73,0.77,0.73,0.79,0.82,0.76,0.83,0.90,0.84,1.18,0.98,1.03,0.92,0.95,0.90,0.86,1.32,1.45,1.15,1.53,1.27,0.99,1.42,1.65,1.58,1.93,1.83,1.94,1.81,1.88,1.74,1.70,1.19,1.17,1.44,1.11,1.15,1.36,1.41,1.61,1.81,1.67,1.22,1.34,1.50,1.42,1.65,1.61,1.82,1.91,1.75,1.80,1.89,1.89,1.98,1.99,1.94,1.98,1.92,1.87,0.86,0.95,0.92,1.14,0.98,1.03,0.79,0.84,0.77,0.97,0.90,0.89,0.76,0.82,0.82,0.74,0.72,0.71,0.98,0.89,0.97,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.60,1.44,1.68,1.22,1.49,1.71,0.93,0.99,0.99,1.23,1.22,1.60,1.68,1.44,1.49,1.40,1.14,1.19,0.89,0.96,0.89,0.97,0.89,0.91,0.98,0.82,0.76,0.82,0.71,0.72,0.73,0.76,0.79,0.86,0.83,0.91,0.83,0.89,0.72,0.76,0.76,0.89,0.89,0.82,0.82,0.98,0.96,0.97,1.14,1.40,1.19,0.94,1.00,1.07,1.37,1.21,1.48,1.30,1.57,1.61,1.37,0.86,0.83,0.91,0.82,0.82,0.88,0.89,0.96,1.14,0.98,0.70,0.72,0.73,0.77,0.76,0.79,0.70,0.72,0.71,0.82,0.77,0.80,0.74,0.79,0.80,0.74,0.87,0.93,0.85,1.23,1.02,1.02,0.93,0.93,0.87,0.85,1.30,1.35,1.07,1.38,1.11,0.94,1.47,1.71,1.56,1.97,1.88,1.92,1.79,1.79,1.59,1.60,1.30,1.35,1.56,1.37,1.38,1.59,1.60,1.79,1.92,1.79,1.48,1.57,1.72,1.61,1.78,1.79,1.93,1.99,1.90,1.86,1.78,1.86,1.93,1.99,1.97,1.90,1.79,1.72,0.94,1.07,1.00,1.37,1.21,1.30,0.86,0.91,0.83,1.14,0.98,0.96,0.82,0.88,0.89,0.79,0.76,0.73,1.07,0.94,1.11,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.74,1.57,1.76,1.33,1.54,1.71,0.94,1.05,0.99,1.26,1.16,1.46,1.60,1.34,1.46,1.59,1.37,1.37,0.97,1.11,0.96,1.10,0.95,0.94,1.08,0.89,0.82,0.88,0.72,0.76,0.75,0.80,0.80,0.88,0.87,0.91,0.83,0.87,0.72,0.76,0.74,0.83,0.84,0.78,0.79,0.96,0.89,0.92,0.98,1.23,1.05,0.86,0.92,0.95,1.11,0.98,1.22,1.03,1.34,1.42,1.14,0.79,0.77,0.84,0.78,0.76,0.82,0.82,0.89,0.97,0.90,0.70,0.71,0.71,0.73,0.72,0.74,0.73,0.76,0.72,0.86,0.81,0.82,0.76,0.79,0.77,0.73,0.90,0.95,0.86,1.18,1.03,0.98,0.92,0.90,0.83,0.84,1.19,1.17,0.98,1.15,0.97,0.89,1.42,1.65,1.44,1.93,1.83,1.81,1.67,1.61,1.36,1.41,1.32,1.45,1.58,1.57,1.53,1.74,1.70,1.88,1.94,1.81,1.69,1.77,1.87,1.79,1.89,1.92,1.98,1.99,1.98,1.89,1.65,1.80,1.82,1.91,1.94,1.75,1.61,1.50,1.07,1.34,1.27,1.60,1.45,1.55,0.93,0.99,0.90,1.35,1.18,1.07,0.87,0.93,0.96,0.85,0.82,0.77,1.15,0.99,1.27,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.86,1.71,1.82,1.48,1.62,1.71,0.98,1.20,1.05,1.34,1.17,1.34,1.53,1.27,1.46,1.77,1.60,1.57,1.16,1.38,1.12,1.35,1.06,1.00,1.28,0.97,0.89,0.95,0.76,0.81,0.79,0.86,0.85,0.92,0.93,0.93,0.85,0.87,0.74,0.78,0.74,0.79,0.82,0.76,0.79,0.96,0.85,0.90,0.94,1.09,0.99,0.81,0.85,0.89,0.95,0.90,0.99,0.94,1.10,1.24,0.98,0.75,0.73,0.78,0.74,0.72,0.77,0.76,0.82,0.89,0.83,0.73,0.71,0.71,0.71,0.70,0.72,0.77,0.80,0.74,0.90,0.85,0.84,0.78,0.79,0.75,0.73,0.92,0.95,0.86,1.05,0.99,0.94,0.90,0.86,0.79,0.81,1.00,0.98,0.91,0.96,0.89,0.83,1.27,1.50,1.23,1.80,1.69,1.63,1.46,1.37,1.09,1.16,1.24,1.44,1.49,1.69,1.59,1.80,1.69,1.87,1.86,1.72,1.82,1.91,1.94,1.92,1.95,1.99,1.98,1.91,1.97,1.89,1.51,1.72,1.67,1.77,1.86,1.55,1.41,1.25,1.33,1.58,1.50,1.80,1.63,1.74,1.04,1.21,0.97,1.48,1.37,1.21,0.93,0.97,1.05,0.92,0.88,0.84,1.14,1.02,1.34,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.94,1.84,1.87,1.64,1.71,1.71,1.14,1.38,1.19,1.46,1.23,1.26,1.48,1.26,1.50,1.91,1.80,1.76,1.41,1.61,1.39,1.59,1.33,1.24,1.51,1.18,0.97,1.11,0.82,0.88,0.86,0.94,0.92,0.99,1.03,0.98,0.91,0.90,0.79,0.84,0.77,0.79,0.84,0.77,0.83,0.99,0.85,0.91,0.92,1.02,1.00,0.79,0.80,0.86,0.88,0.84,0.92,0.88,0.97,1.10,0.94,0.74,0.71,0.74,0.72,0.70,0.73,0.72,0.76,0.82,0.77,0.77,0.73,0.74,0.71,0.70,0.73,0.83,0.85,0.78,0.92,0.88,0.86,0.81,0.79,0.74,0.75,0.92,0.93,0.85,0.96,0.94,0.88,0.86,0.81,0.75,0.79,0.93,0.90,0.85,0.88,0.82,0.77,1.05,1.27,0.99,1.60,1.47,1.39,1.20,1.11,0.95,0.97,1.08,1.33,1.31,1.70,1.55,1.76,1.57,1.76,1.70,1.54,1.85,1.97,1.91,1.99,1.97,1.99,1.91,1.77,1.88,1.85,1.39,1.64,1.51,1.58,1.74,1.32,1.22,1.01,1.54,1.76,1.65,1.93,1.70,1.85,1.28,1.39,1.09,1.52,1.48,1.26,0.97,0.99,1.18,1.00,0.93,0.90,1.05,1.01,1.31,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.97,1.92,1.88,1.79,1.79,1.71,1.37,1.59,1.38,1.60,1.35,1.23,1.47,1.30,1.56,1.99,1.93,1.90,1.60,1.78,1.61,1.79,1.57,1.48,1.72,1.40,1.14,1.37,0.89,0.96,0.94,1.07,1.00,1.21,1.30,1.14,0.98,0.96,0.86,0.91,0.83,0.82,0.88,0.82,0.89,1.11,0.87,0.94,0.93,1.02,1.07,0.80,0.79,0.85,0.82,0.80,0.87,0.85,0.93,1.02,0.93,0.77,0.72,0.74,0.71,0.70,0.70,0.71,0.72,0.77,0.74,0.82,0.76,0.79,0.72,0.73,0.76,0.89,0.89,0.82,0.93,0.91,0.86,0.83,0.79,0.73,0.76,0.91,0.89,0.83,0.89,0.89,0.82,0.82,0.76,0.72,0.76,0.86,0.83,0.79,0.82,0.76,0.73,0.94,1.00,0.91,1.37,1.21,1.14,0.98,0.96,0.88,0.89,0.96,1.14,1.07,1.60,1.40,1.61,1.37,1.57,1.48,1.30,1.78,1.93,1.79,1.99,1.92,1.90,1.79,1.59,1.72,1.79,1.30,1.56,1.35,1.38,1.60,1.11,1.07,0.94,1.68,1.86,1.71,1.97,1.68,1.86,1.44,1.49,1.22,1.44,1.49,1.22,0.99,0.99,1.23,1.19,0.98,0.97,0.97,0.98,1.19,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.94,1.97,1.87,1.91,1.85,1.71,1.60,1.77,1.58,1.74,1.51,1.26,1.48,1.39,1.64,1.99,1.97,1.99,1.70,1.85,1.76,1.91,1.76,1.70,1.88,1.55,1.33,1.57,0.96,1.08,1.05,1.31,1.27,1.47,1.54,1.39,1.20,1.11,0.93,0.99,0.90,0.88,0.95,0.88,0.97,1.32,0.92,1.01,0.97,1.10,1.22,0.84,0.80,0.88,0.79,0.79,0.85,0.86,0.92,1.02,0.94,0.82,0.76,0.77,0.72,0.73,0.70,0.72,0.71,0.74,0.74,0.88,0.81,0.85,0.75,0.77,0.82,0.94,0.93,0.86,0.92,0.92,0.86,0.85,0.79,0.74,0.79,0.88,0.85,0.81,0.82,0.83,0.77,0.78,0.73,0.71,0.75,0.79,0.77,0.74,0.77,0.73,0.70,0.86,0.92,0.84,1.14,0.99,0.98,0.91,0.90,0.84,0.83,0.88,0.97,0.94,1.41,1.18,1.39,1.11,1.33,1.24,1.03,1.61,1.80,1.59,1.91,1.84,1.76,1.64,1.38,1.51,1.71,1.26,1.50,1.23,1.19,1.46,0.99,1.00,0.91,1.70,1.85,1.65,1.93,1.54,1.76,1.52,1.48,1.26,1.28,1.39,1.09,0.99,0.97,1.18,1.31,1.01,1.05,0.90,0.93,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.86,1.95,1.82,1.98,1.89,1.71,1.80,1.91,1.77,1.86,1.67,1.34,1.53,1.51,1.72,1.92,1.91,1.99,1.69,1.82,1.80,1.94,1.87,1.86,1.97,1.59,1.44,1.69,1.05,1.24,1.27,1.49,1.50,1.69,1.72,1.63,1.46,1.37,1.00,1.23,0.98,0.95,1.09,0.96,1.16,1.55,0.99,1.25,1.10,1.24,1.41,0.90,0.85,0.94,0.79,0.81,0.85,0.89,0.94,1.09,0.98,0.89,0.82,0.83,0.74,0.77,0.72,0.76,0.73,0.75,0.78,0.94,0.86,0.91,0.79,0.83,0.89,0.99,0.95,0.90,0.90,0.92,0.84,0.86,0.79,0.75,0.81,0.85,0.80,0.78,0.76,0.77,0.73,0.74,0.71,0.71,0.73,0.74,0.74,0.71,0.76,0.72,0.70,0.79,0.85,0.78,0.98,0.92,0.93,0.85,0.87,0.82,0.79,0.81,0.89,0.86,1.16,0.97,1.12,0.95,1.06,1.00,0.93,1.38,1.60,1.35,1.77,1.71,1.57,1.48,1.20,1.28,1.62,1.27,1.46,1.17,1.05,1.34,0.96,0.99,0.90,1.63,1.74,1.50,1.80,1.33,1.58,1.48,1.37,1.21,1.04,1.21,0.97,0.97,0.93,1.05,1.34,1.02,1.14,0.84,0.88,0.92,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.74,1.89,1.76,1.98,1.89,1.71,1.93,1.99,1.91,1.94,1.82,1.46,1.60,1.65,1.80,1.79,1.77,1.92,1.57,1.69,1.74,1.87,1.88,1.94,1.98,1.53,1.45,1.70,1.18,1.32,1.42,1.58,1.65,1.83,1.81,1.81,1.67,1.61,1.19,1.44,1.17,1.11,1.36,1.15,1.41,1.75,1.22,1.50,1.34,1.42,1.61,0.98,0.92,1.03,0.83,0.86,0.89,0.95,0.98,1.23,1.14,0.97,0.89,0.90,0.78,0.82,0.76,0.82,0.77,0.79,0.84,0.98,0.90,0.98,0.83,0.89,0.97,1.03,0.95,0.92,0.86,0.90,0.82,0.86,0.79,0.77,0.84,0.81,0.76,0.76,0.72,0.73,0.70,0.72,0.71,0.73,0.73,0.72,0.74,0.71,0.78,0.74,0.72,0.75,0.80,0.76,0.94,0.88,0.91,0.83,0.87,0.84,0.79,0.76,0.82,0.80,0.97,0.89,0.96,0.88,0.95,0.94,0.87,1.11,1.37,1.10,1.59,1.57,1.37,1.33,1.05,1.08,1.54,1.34,1.46,1.16,0.99,1.26,0.96,1.05,0.92,1.45,1.55,1.27,1.60,1.07,1.34,1.35,1.18,1.07,0.93,0.99,0.90,0.93,0.87,0.96,1.27,0.99,1.15,0.77,0.82,0.85,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.60,1.78,1.68,1.93,1.86,1.71,1.97,1.99,1.99,1.97,1.93,1.60,1.68,1.78,1.86,1.61,1.57,1.79,1.37,1.48,1.59,1.72,1.79,1.92,1.90,1.38,1.35,1.60,1.23,1.30,1.47,1.56,1.71,1.88,1.79,1.92,1.79,1.79,1.30,1.56,1.35,1.37,1.59,1.38,1.60,1.90,1.48,1.72,1.57,1.61,1.79,1.21,1.00,1.30,0.89,0.94,0.96,1.07,1.14,1.40,1.37,1.14,0.96,0.98,0.82,0.88,0.82,0.89,0.83,0.86,0.91,1.02,0.93,1.07,0.87,0.94,1.11,1.02,0.93,0.93,0.82,0.87,0.80,0.85,0.79,0.80,0.85,0.77,0.72,0.74,0.71,0.70,0.70,0.71,0.72,0.77,0.74,0.72,0.76,0.73,0.82,0.79,0.76,0.73,0.79,0.76,0.93,0.86,0.91,0.83,0.89,0.89,0.82,0.72,0.76,0.76,0.89,0.82,0.89,0.82,0.89,0.91,0.83,0.96,1.14,0.97,1.40,1.44,1.19,1.22,0.99,0.98,1.49,1.44,1.49,1.22,0.99,1.23,0.98,1.19,0.97,1.21,1.30,1.00,1.37,0.94,1.07,1.14,0.98,0.96,0.86,0.91,0.83,0.88,0.82,0.89,1.11,0.94,1.07,0.73,0.76,0.79,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.46,1.65,1.60,1.82,1.80,1.71,1.93,1.91,1.99,1.94,1.98,1.74,1.76,1.89,1.89,1.42,1.34,1.61,1.11,1.22,1.36,1.50,1.61,1.81,1.75,1.15,1.17,1.41,1.18,1.19,1.42,1.44,1.65,1.83,1.67,1.94,1.81,1.88,1.32,1.58,1.45,1.57,1.74,1.53,1.70,1.98,1.69,1.87,1.77,1.79,1.92,1.45,1.27,1.55,0.97,1.07,1.11,1.34,1.37,1.59,1.60,1.35,1.07,1.18,0.86,0.93,0.87,0.96,0.90,0.93,0.99,1.03,0.95,1.15,0.90,0.99,1.27,0.98,0.90,0.92,0.78,0.83,0.77,0.84,0.79,0.82,0.86,0.73,0.71,0.73,0.72,0.70,0.73,0.72,0.76,0.81,0.76,0.76,0.82,0.77,0.89,0.85,0.82,0.75,0.80,0.80,0.94,0.88,0.94,0.87,0.95,0.96,0.88,0.72,0.74,0.76,0.83,0.78,0.84,0.79,0.87,0.91,0.83,0.89,0.98,0.92,1.23,1.34,1.05,1.16,0.99,0.96,1.46,1.57,1.54,1.33,1.05,1.26,1.08,1.37,1.10,0.98,1.03,0.92,1.14,0.86,0.95,0.97,0.90,0.89,0.79,0.84,0.77,0.82,0.76,0.82,0.97,0.89,0.98,0.71,0.72,0.74,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.34,1.51,1.53,1.67,1.72,1.71,1.80,1.77,1.91,1.86,1.98,1.86,1.82,1.95,1.89,1.24,1.10,1.41,0.95,0.99,1.09,1.25,1.37,1.63,1.55,0.96,0.98,1.16,1.05,1.00,1.27,1.23,1.50,1.69,1.46,1.86,1.72,1.87,1.24,1.49,1.44,1.69,1.80,1.59,1.69,1.97,1.82,1.94,1.91,1.92,1.99,1.63,1.50,1.74,1.16,1.33,1.38,1.58,1.60,1.77,1.80,1.48,1.21,1.37,0.90,0.97,0.93,1.05,0.97,1.04,1.21,0.99,0.95,1.14,0.92,1.02,1.34,0.94,0.86,0.90,0.74,0.79,0.75,0.81,0.79,0.84,0.86,0.71,0.71,0.73,0.76,0.73,0.77,0.74,0.80,0.85,0.78,0.81,0.89,0.84,0.97,0.92,0.88,0.79,0.85,0.86,0.98,0.92,1.00,0.93,1.06,1.12,0.95,0.74,0.74,0.78,0.79,0.76,0.82,0.79,0.87,0.93,0.85,0.85,0.94,0.90,1.09,1.27,0.99,1.17,1.05,0.96,1.46,1.71,1.62,1.48,1.20,1.34,1.28,1.57,1.35,0.90,0.94,0.85,0.98,0.81,0.89,0.89,0.83,0.82,0.75,0.78,0.73,0.77,0.72,0.76,0.89,0.83,0.91,0.71,0.70,0.72,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00},
	{1.26,1.39,1.48,1.51,1.64,1.71,1.60,1.58,1.77,1.74,1.91,1.94,1.87,1.97,1.85,1.10,0.97,1.22,0.88,0.92,0.95,1.01,1.11,1.39,1.32,0.88,0.90,0.97,0.96,0.93,1.05,0.99,1.27,1.47,1.20,1.70,1.54,1.76,1.08,1.31,1.33,1.70,1.76,1.55,1.57,1.88,1.85,1.91,1.97,1.99,1.99,1.70,1.65,1.85,1.41,1.54,1.61,1.76,1.80,1.91,1.93,1.52,1.26,1.48,0.92,0.99,0.97,1.18,1.09,1.28,1.39,0.94,0.93,1.05,0.92,1.01,1.31,0.88,0.81,0.86,0.72,0.75,0.74,0.79,0.79,0.86,0.85,0.71,0.73,0.75,0.82,0.77,0.83,0.78,0.85,0.88,0.81,0.88,0.97,0.90,1.18,1.00,0.93,0.86,0.92,0.94,1.14,0.99,1.24,1.03,1.33,1.39,1.11,0.79,0.77,0.84,0.79,0.77,0.84,0.83,0.90,0.98,0.91,0.85,0.92,0.91,1.02,1.26,1.00,1.23,1.19,0.99,1.50,1.84,1.71,1.64,1.38,1.46,1.51,1.76,1.59,0.84,0.88,0.80,0.94,0.79,0.86,0.82,0.77,0.76,0.74,0.74,0.71,0.73,0.70,0.72,0.82,0.77,0.85,0.74,0.70,0.73,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00,1.00}
};


extern vec3_t lightspot;

float* shadedots = r_avertexnormal_dots[0];

float entalpha; //johnfitz

bool overbright; //johnfitz

bool shading = true; //johnfitz -- if false, disable vertex shading for various reasons (fullbright, r_lightmap, showtris, etc)

//johnfitz -- struct for passing lerp information to drawing functions
struct lerpdata_t
{
	short pose1;
	short pose2;
	float blend;
	vec3_t origin;
	vec3_t angles;
};

//johnfitz

/*
=============
GL_DrawAliasFrame -- johnfitz -- rewritten to support colored light, lerping, entalpha, multitexture, and r_drawflat
=============
*/
void GL_DrawAliasFrame(aliashdr_t* paliashdr, lerpdata_t lerpdata)
{
	float vertcolor[4];
	trivertx_t *verts1, *verts2 = nullptr;
	float blend = 0, iblend = 0;
	bool lerping;

	if (lerpdata.pose1 != lerpdata.pose2)
	{
		lerping = true;
		verts1 = reinterpret_cast<trivertx_t *>(reinterpret_cast<byte *>(paliashdr) + paliashdr->posedata);
		verts2 = verts1;
		verts1 += lerpdata.pose1 * paliashdr->poseverts;
		verts2 += lerpdata.pose2 * paliashdr->poseverts;
		blend = lerpdata.blend;
		iblend = 1.0f - blend;
	}
	else // poses the same means either 1. the entity has paused its animation, or 2. r_lerpmodels is disabled
	{
		lerping = false;
		verts1 = reinterpret_cast<trivertx_t *>(reinterpret_cast<byte *>(paliashdr) + paliashdr->posedata);
		verts1 += lerpdata.pose1 * paliashdr->poseverts;
	}

	auto commands = reinterpret_cast<int *>(reinterpret_cast<byte *>(paliashdr) + paliashdr->commands);

	vertcolor[3] = entalpha; //never changes, so there's no need to put this inside the loop

	while (true)
	{
		// get the vertex count and primitive type
		int count = *commands++;
		if (!count)
			break; // done

		if (count < 0)
		{
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		}
		else
			glBegin(GL_TRIANGLE_STRIP);

		do
		{
			auto u = reinterpret_cast<float *>(commands)[0];
			auto v = reinterpret_cast<float *>(commands)[1];
			if (mtexenabled)
			{
				GL_MTexCoord2fFunc(TEXTURE0, u, v);
				GL_MTexCoord2fFunc(TEXTURE1, u, v);
			}
			else
				glTexCoord2f(u, v);

			commands += 2;

			if (shading)
			{
				if (r_drawflat_cheatsafe)
				{
					srand(count * reinterpret_cast<unsigned int>(commands));
					glColor3f(rand() % 256 / 255.0, rand() % 256 / 255.0, rand() % 256 / 255.0);
				}
				else if (lerping)
				{
					vertcolor[0] = (shadedots[verts1->lightnormalindex] * iblend + shadedots[verts2->lightnormalindex] * blend) * lightcolor[0];
					vertcolor[1] = (shadedots[verts1->lightnormalindex] * iblend + shadedots[verts2->lightnormalindex] * blend) * lightcolor[1];
					vertcolor[2] = (shadedots[verts1->lightnormalindex] * iblend + shadedots[verts2->lightnormalindex] * blend) * lightcolor[2];
					glColor4fv(vertcolor);
				}
				else
				{
					vertcolor[0] = shadedots[verts1->lightnormalindex] * lightcolor[0];
					vertcolor[1] = shadedots[verts1->lightnormalindex] * lightcolor[1];
					vertcolor[2] = shadedots[verts1->lightnormalindex] * lightcolor[2];
					glColor4fv(vertcolor);
				}
			}

			if (lerping)
			{
				glVertex3f(verts1->v[0] * iblend + verts2->v[0] * blend,
				           verts1->v[1] * iblend + verts2->v[1] * blend,
				           verts1->v[2] * iblend + verts2->v[2] * blend);
				verts1++;
				verts2++;
			}
			else
			{
				glVertex3f(verts1->v[0], verts1->v[1], verts1->v[2]);
				verts1++;
			}
		}
		while (--count);

		glEnd();
	}

	rs_aliaspasses += paliashdr->numtris;
}

/*
=================
R_SetupAliasFrame -- johnfitz -- rewritten to support lerping
=================
*/
void R_SetupAliasFrame(aliashdr_t* paliashdr, int frame, lerpdata_t* lerpdata)
{
	auto e = currententity;

	if (frame >= paliashdr->numframes || frame < 0)
	{
		Con_DPrintf("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	auto posenum = paliashdr->frames[frame].firstpose;
	auto numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		e->lerptime = paliashdr->frames[frame].interval;
		posenum += static_cast<int>(cl.time / e->lerptime) % numposes;
	}
	else
		e->lerptime = 0.1;

	if (e->lerpflags & LERP_RESETANIM) //kill any lerp in progress
	{
		e->lerpstart = 0;
		e->previouspose = posenum;
		e->currentpose = posenum;
		e->lerpflags -= LERP_RESETANIM;
	}
	else if (e->currentpose != posenum) // pose changed, start new lerp
	{
		if (e->lerpflags & LERP_RESETANIM2) //defer lerping one more time
		{
			e->lerpstart = 0;
			e->previouspose = posenum;
			e->currentpose = posenum;
			e->lerpflags -= LERP_RESETANIM2;
		}
		else
		{
			e->lerpstart = cl.time;
			e->previouspose = e->currentpose;
			e->currentpose = posenum;
		}
	}

	//set up values
	if (r_lerpmodels.value && !(e->model->flags & MOD_NOLERP && r_lerpmodels.value != 2))
	{
		if (e->lerpflags & LERP_FINISH && numposes == 1)
			lerpdata->blend = CLAMP (0, (cl.time - e->lerpstart) / (e->lerpfinish - e->lerpstart), 1);
		else
			lerpdata->blend = CLAMP (0, (cl.time - e->lerpstart) / e->lerptime, 1);
		lerpdata->pose1 = e->previouspose;
		lerpdata->pose2 = e->currentpose;
	}
	else //don't lerp
	{
		lerpdata->blend = 1;
		lerpdata->pose1 = posenum;
		lerpdata->pose2 = posenum;
	}
}

/*
=================
R_SetupEntityTransform -- johnfitz -- set up transform part of lerpdata
=================
*/
void R_SetupEntityTransform(entity_t* e, lerpdata_t* lerpdata)
{
	float blend;
	vec3_t d;

	// if LERP_RESETMOVE, kill any lerps in progress
	if (e->lerpflags & LERP_RESETMOVE)
	{
		e->movelerpstart = 0;
		VectorCopy (e->origin, e->previousorigin);
		VectorCopy (e->origin, e->currentorigin);
		VectorCopy (e->angles, e->previousangles);
		VectorCopy (e->angles, e->currentangles);
		e->lerpflags -= LERP_RESETMOVE;
	}
	else if (!VectorCompare(e->origin, e->currentorigin) || !VectorCompare(e->angles, e->currentangles)) // origin/angles changed, start new lerp
	{
		e->movelerpstart = cl.time;
		VectorCopy (e->currentorigin, e->previousorigin);
		VectorCopy (e->origin, e->currentorigin);
		VectorCopy (e->currentangles, e->previousangles);
		VectorCopy (e->angles, e->currentangles);
	}

	//set up values
	if (r_lerpmove.value && e != &cl.viewent && e->lerpflags & LERP_MOVESTEP)
	{
		if (e->lerpflags & LERP_FINISH)
			blend = CLAMP (0, (cl.time - e->movelerpstart) / (e->lerpfinish - e->movelerpstart), 1);
		else
			blend = CLAMP (0, (cl.time - e->movelerpstart) / 0.1, 1);

		//translation
		VectorSubtract (e->currentorigin, e->previousorigin, d);
		lerpdata->origin[0] = e->previousorigin[0] + d[0] * blend;
		lerpdata->origin[1] = e->previousorigin[1] + d[1] * blend;
		lerpdata->origin[2] = e->previousorigin[2] + d[2] * blend;

		//rotation
		VectorSubtract (e->currentangles, e->previousangles, d);
		for (int i = 0; i < 3; i++)
		{
			if (d[i] > 180) d[i] -= 360;
			if (d[i] < -180) d[i] += 360;
		}
		lerpdata->angles[0] = e->previousangles[0] + d[0] * blend;
		lerpdata->angles[1] = e->previousangles[1] + d[1] * blend;
		lerpdata->angles[2] = e->previousangles[2] + d[2] * blend;
	}
	else //don't lerp
	{
		VectorCopy (e->origin, lerpdata->origin);
		VectorCopy (e->angles, lerpdata->angles);
	}
}

int R_LightPoint(vec3_t p);

/*
=================
R_SetupAliasLighting -- johnfitz -- broken out from R_DrawAliasModel and rewritten
=================
*/
void R_SetupAliasLighting(entity_t* e)
{
	vec3_t dist;
	float add;
	int i;

	R_LightPoint(e->origin);

	//add dlights
	for (i = 0; i < MAX_DLIGHTS; i++)
	{
		if (cl_dlights[i].die >= cl.time)
		{
			VectorSubtract (currententity->origin, cl_dlights[i].origin, dist);
			add = cl_dlights[i].radius - Length(dist);
			if (add > 0)
				VectorMA(lightcolor, add, cl_dlights[i].color, lightcolor);
		}
	}

	// minimum light value on gun (24)
	if (e == &cl.viewent)
	{
		add = 72.0f - (lightcolor[0] + lightcolor[1] + lightcolor[2]);
		if (add > 0.0f)
		{
			lightcolor[0] += add / 3.0f;
			lightcolor[1] += add / 3.0f;
			lightcolor[2] += add / 3.0f;
		}
	}

	// minimum light value on players (8)
	if (currententity > cl_entities && currententity <= cl_entities + cl.maxclients)
	{
		add = 24.0f - (lightcolor[0] + lightcolor[1] + lightcolor[2]);
		if (add > 0.0f)
		{
			lightcolor[0] += add / 3.0f;
			lightcolor[1] += add / 3.0f;
			lightcolor[2] += add / 3.0f;
		}
	}

	// clamp lighting so it doesn't overbright as much (96)
	if (overbright)
	{
		add = 288.0f / (lightcolor[0] + lightcolor[1] + lightcolor[2]);
		if (add < 1.0f)
			VectorScale(lightcolor, add, lightcolor);
	}

	//hack up the brightness when fullbrights but no overbrights (256)
	if (gl_fullbrights.value && !gl_overbright_models.value)
		if (e->model->flags & MOD_FBRIGHTHACK)
		{
			lightcolor[0] = 256.0f;
			lightcolor[1] = 256.0f;
			lightcolor[2] = 256.0f;
		}

	shadedots = r_avertexnormal_dots[static_cast<int>(e->angles[1] * (SHADEDOT_QUANT / 360.0)) & SHADEDOT_QUANT - 1];
	VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);
}

bool R_CullModelForEntity(entity_t* e);
void R_RotateForEntity(vec3_t origin, vec3_t angles);

/*
=================
R_DrawAliasModel -- johnfitz -- almost completely rewritten
=================
*/
void R_DrawAliasModel(entity_t* e)
{
	lerpdata_t lerpdata;

	//
	// setup pose/lerp data -- do it first so we don't miss updates due to culling
	//
	auto paliashdr = static_cast<aliashdr_t *>(Mod_Extradata(e->model));
	R_SetupAliasFrame(paliashdr, e->frame, &lerpdata);
	R_SetupEntityTransform(e, &lerpdata);

	//
	// cull it
	//
	if (R_CullModelForEntity(e))
		return;

	//
	// transform it
	//
	glPushMatrix();
	R_RotateForEntity(lerpdata.origin, lerpdata.angles);
	glTranslatef(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	glScalef(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	//
	// random stuff
	//
	if (gl_smoothmodels.value && !r_drawflat_cheatsafe)
		glShadeModel(GL_SMOOTH);
	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	overbright = gl_overbright_models.value;
	shading = true;

	//
	// set up for alpha blending
	//
	if (r_drawflat_cheatsafe || r_lightmap_cheatsafe) //no alpha in drawflat or lightmap mode
		entalpha = 1;
	else
		entalpha = ENTALPHA_DECODE(e->alpha);
	if (entalpha == 0)
		goto cleanup;
	if (entalpha < 1)
	{
		if (!gl_texture_env_combine) overbright = false; //overbright can't be done in a single pass without combiners
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
	}

	//
	// set up lighting
	//
	rs_aliaspolys += paliashdr->numtris;
	R_SetupAliasLighting(e);

	//
	// set up textures
	//
	GL_DisableMultitexture();
	auto anim = static_cast<int>(cl.time * 10) & 3;
	auto tx = paliashdr->gltextures[e->skinnum][anim];
	auto fb = paliashdr->fbtextures[e->skinnum][anim];
	if (e->colormap != vid.colormap && !gl_nocolors.value)
	{
		int i = e - cl_entities;
		if (i >= 1 && i <= cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
			tx = playertextures[i - 1];
	}
	if (!gl_fullbrights.value)
		fb = nullptr;

	//
	// draw it
	//
	if (r_drawflat_cheatsafe)
	{
		glDisable(GL_TEXTURE_2D);
		GL_DrawAliasFrame(paliashdr, lerpdata);
		glEnable(GL_TEXTURE_2D);
		srand(static_cast<int>(cl.time * 1000)); //restore randomness
	}
	else if (r_fullbright_cheatsafe)
	{
		GL_Bind(tx);
		shading = false;
		glColor4f(1, 1, 1, entalpha);
		GL_DrawAliasFrame(paliashdr, lerpdata);
		if (fb)
		{
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_Bind(fb);
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			glDepthMask(GL_FALSE);
			glColor3f(entalpha, entalpha, entalpha);
			Fog_StartAdditive();
			GL_DrawAliasFrame(paliashdr, lerpdata);
			Fog_StopAdditive();
			glDepthMask(GL_TRUE);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_BLEND);
		}
	}
	else if (r_lightmap_cheatsafe)
	{
		glDisable(GL_TEXTURE_2D);
		shading = false;
		glColor3f(1, 1, 1);
		GL_DrawAliasFrame(paliashdr, lerpdata);
		glEnable(GL_TEXTURE_2D);
	}
	else if (overbright)
	{
		if (gl_texture_env_combine && gl_mtexable && gl_texture_env_add && fb) //case 1: everything in one pass
		{
			GL_Bind(tx);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
			GL_EnableMultitexture(); // selects TEXTURE1
			GL_Bind(fb);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
			glEnable(GL_BLEND);
			GL_DrawAliasFrame(paliashdr, lerpdata);
			glDisable(GL_BLEND);
			GL_DisableMultitexture();
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
		else if (gl_texture_env_combine) //case 2: overbright in one pass, then fullbright pass
		{
			// first pass
			GL_Bind(tx);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
			GL_DrawAliasFrame(paliashdr, lerpdata);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			// second pass
			if (fb)
			{
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				GL_Bind(fb);
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
				glDepthMask(GL_FALSE);
				shading = false;
				glColor3f(entalpha, entalpha, entalpha);
				Fog_StartAdditive();
				GL_DrawAliasFrame(paliashdr, lerpdata);
				Fog_StopAdditive();
				glDepthMask(GL_TRUE);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glDisable(GL_BLEND);
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			}
		}
		else //case 3: overbright in two passes, then fullbright pass
		{
			// first pass
			GL_Bind(tx);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_DrawAliasFrame(paliashdr, lerpdata);
			// second pass -- additive with black fog, to double the object colors but not the fog color
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			glDepthMask(GL_FALSE);
			Fog_StartAdditive();
			GL_DrawAliasFrame(paliashdr, lerpdata);
			Fog_StopAdditive();
			glDepthMask(GL_TRUE);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_BLEND);
			// third pass
			if (fb)
			{
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				GL_Bind(fb);
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
				glDepthMask(GL_FALSE);
				shading = false;
				glColor3f(entalpha, entalpha, entalpha);
				Fog_StartAdditive();
				GL_DrawAliasFrame(paliashdr, lerpdata);
				Fog_StopAdditive();
				glDepthMask(GL_TRUE);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glDisable(GL_BLEND);
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			}
		}
	}
	else
	{
		if (gl_mtexable && gl_texture_env_add && fb) //case 4: fullbright mask using multitexture
		{
			GL_DisableMultitexture(); // selects TEXTURE0
			GL_Bind(tx);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_EnableMultitexture(); // selects TEXTURE1
			GL_Bind(fb);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
			glEnable(GL_BLEND);
			GL_DrawAliasFrame(paliashdr, lerpdata);
			glDisable(GL_BLEND);
			GL_DisableMultitexture();
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
		else //case 5: fullbright mask without multitexture
		{
			// first pass
			GL_Bind(tx);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_DrawAliasFrame(paliashdr, lerpdata);
			// second pass
			if (fb)
			{
				GL_Bind(fb);
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
				glDepthMask(GL_FALSE);
				shading = false;
				glColor3f(entalpha, entalpha, entalpha);
				Fog_StartAdditive();
				GL_DrawAliasFrame(paliashdr, lerpdata);
				Fog_StopAdditive();
				glDepthMask(GL_TRUE);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glDisable(GL_BLEND);
			}
		}
	}

cleanup:
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glShadeModel(GL_FLAT);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glColor3f(1, 1, 1);
	glPopMatrix();
}

//johnfitz -- values for shadow matrix
#define SHADOW_SKEW_X -0.7 //skew along x axis. -0.7 to mimic glquake shadows
#define SHADOW_SKEW_Y 0 //skew along y axis. 0 to mimic glquake shadows
#define SHADOW_VSCALE 0 //0=completely flat
#define SHADOW_HEIGHT 0.1 //how far above the floor to render the shadow
//johnfitz

/*
=============
GL_DrawAliasShadow -- johnfitz -- rewritten

TODO: orient shadow onto "lightplane" (a global mplane_t*)
=============
*/
void GL_DrawAliasShadow(entity_t* e)
{
	float shadowmatrix[16] = {1, 0, 0, 0,
		0, 1, 0, 0,
		SHADOW_SKEW_X, SHADOW_SKEW_Y, SHADOW_VSCALE, 0,
		0, 0, SHADOW_HEIGHT, 1};
	lerpdata_t lerpdata;

	if (R_CullModelForEntity(e))
		return;

	if (e == &cl.viewent || e->model->flags & MOD_NOSHADOW)
		return;

	entalpha = ENTALPHA_DECODE(e->alpha);
	if (entalpha == 0) return;

	auto paliashdr = static_cast<aliashdr_t *>(Mod_Extradata(e->model));
	R_SetupAliasFrame(paliashdr, e->frame, &lerpdata);
	R_SetupEntityTransform(e, &lerpdata);
	R_LightPoint(e->origin);
	auto lheight = currententity->origin[2] - lightspot[2];

	// set up matrix
	glPushMatrix();
	glTranslatef(lerpdata.origin[0], lerpdata.origin[1], lerpdata.origin[2]);
	glTranslatef(0, 0, -lheight);
	glMultMatrixf(shadowmatrix);
	glTranslatef(0, 0, lheight);
	glRotatef(lerpdata.angles[1], 0, 0, 1);
	glRotatef(-lerpdata.angles[0], 0, 1, 0);
	glRotatef(lerpdata.angles[2], 1, 0, 0);
	glTranslatef(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	glScalef(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	// draw it
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	GL_DisableMultitexture();
	glDisable(GL_TEXTURE_2D);
	shading = false;
	glColor4f(0, 0, 0, entalpha * 0.5);
	GL_DrawAliasFrame(paliashdr, lerpdata);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);

	//clean up
	glPopMatrix();
}

/*
=================
R_DrawAliasModel_ShowTris -- johnfitz
=================
*/
void R_DrawAliasModel_ShowTris(entity_t* e)
{
	lerpdata_t lerpdata;

	if (R_CullModelForEntity(e))
		return;

	auto paliashdr = static_cast<aliashdr_t *>(Mod_Extradata(e->model));
	R_SetupAliasFrame(paliashdr, e->frame, &lerpdata);
	R_SetupEntityTransform(e, &lerpdata);

	glPushMatrix();
	R_RotateForEntity(lerpdata.origin, lerpdata.angles);
	glTranslatef(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	glScalef(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	shading = false;
	glColor3f(1, 1, 1);
	GL_DrawAliasFrame(paliashdr, lerpdata);

	glPopMatrix();
}
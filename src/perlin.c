/* perlin.c - simple 2D Perlin-like gradient noise and octave (fractal) wrapper */
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "perlin.h"

static int perm_table[512];
static PerlinParams g_params;

static float fadef(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static float lerpf(float a, float b, float t) {
    return a + t * (b - a);
}

static float grad2(int hash, float x, float y) {
    int h = hash & 3;
    switch (h) {
        case 0: return  x + y;
        case 1: return -x + y;
        case 2: return  x - y;
        default: return -x - y;
    }
}

static void perlin_init_perm(unsigned int seed) {
    int i;
    int src[256];
    for (i = 0; i < 256; ++i) src[i] = i;
    srand(seed);
    for (i = 255; i > 0; --i) {
        int j = rand() % (i + 1);
        int t = src[i]; src[i] = src[j]; src[j] = t;
    }
    for (i = 0; i < 256; ++i) {
        perm_table[i] = src[i];
        perm_table[i + 256] = src[i];
    }
}

static float perlin_noise(float x, float y) {
    int xi = (int)floorf(x) & 255;
    int yi = (int)floorf(y) & 255;
    float xf = x - floorf(x);
    float yf = y - floorf(y);
    float u = fadef(xf);
    float v = fadef(yf);

    int aa = perm_table[xi + perm_table[yi]];
    int ab = perm_table[xi + perm_table[yi + 1]];
    int ba = perm_table[xi + 1 + perm_table[yi]];
    int bb = perm_table[xi + 1 + perm_table[yi + 1]];

    float x1 = lerpf(grad2(aa, xf, yf), grad2(ba, xf - 1.0f, yf), u);
    float x2 = lerpf(grad2(ab, xf, yf - 1.0f), grad2(bb, xf - 1.0f, yf - 1.0f), u);
    return lerpf(x1, x2, v);
}

static float octave_noise(float x, float y, int octaves, float persistence) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxAmp = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        total += perlin_noise(x * frequency, y * frequency) * amplitude;
        maxAmp += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }
    if (maxAmp == 0.0f) return 0.0f;
    return total / maxAmp;
}

void perlin_init_with_params(const PerlinParams* p) {
    if (!p) return;
    g_params = *p;
    if (g_params.seed == 0) g_params.seed = (unsigned int)time(NULL);
    perlin_init_perm(g_params.seed);
}

float perlin_elevation(float x, float y) {
    /* x,y are treated as grid coordinates; apply scale internally */
    float nx = x * g_params.scale;
    float ny = y * g_params.scale;
    float v = octave_noise(nx, ny, g_params.octaves, g_params.persistence);
    return (v + 1.0f) * 0.5f;
}

float perlin_moisture(float x, float y) {
    float nx = x * (g_params.scale * g_params.moisture_scale / (g_params.scale == 0.0f ? 1.0f : g_params.scale));
    float ny = y * (g_params.scale * g_params.moisture_scale / (g_params.scale == 0.0f ? 1.0f : g_params.scale));
    float v = octave_noise(nx + 100.0f, ny + 100.0f, g_params.moisture_octaves, 0.5f);
    return (v + 1.0f) * 0.5f;
}

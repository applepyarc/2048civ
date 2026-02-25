/* perlin.h - lightweight 2D Perlin-like noise with configurable params */
#ifndef PERLIN_H
#define PERLIN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float scale; /* base spatial scale (lower = larger features) */
    int octaves; /* fractal octaves for elevation */
    float persistence; /* amplitude falloff */
    float moisture_scale; /* scale for moisture/noise relative to base */
    int moisture_octaves;
    unsigned int seed; /* RNG seed for permutation table */
} PerlinParams;

/* initialize internal tables and store params; must be called before
 * perlin_elevation / perlin_moisture
 */
void perlin_init_with_params(const PerlinParams* p);

/* sample elevation in [0,1] for grid coordinate (x,y) */
float perlin_elevation(float x, float y);

/* sample moisture in [0,1] for grid coordinate (x,y) */
float perlin_moisture(float x, float y);

#ifdef __cplusplus
}
#endif

#endif /* PERLIN_H */

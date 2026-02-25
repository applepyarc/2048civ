# 2048civ

## Perlin noise configuration

Terrain generation is driven by a configurable Perlin-like noise implementation. Tune generation by setting environment variables before running the program.

- `2048CIV_PERLIN_SCALE`: default `0.03` — base spatial scale (lower -> larger geographic features).
- `2048CIV_PERLIN_OCTAVES`: default `5` — number of fractal octaves for elevation.
- `2048CIV_PERLIN_PERSISTENCE`: default `0.5` — amplitude falloff between octaves.
- `2048CIV_PERLIN_MOISTURE_SCALE`: default `0.06` — relative scale applied to moisture noise.
- `2048CIV_PERLIN_MOISTURE_OCTAVES`: default `4` — octaves used for moisture noise.
- `2048CIV_PERLIN_SEED`: default `0` — seed for the noise permutation table (0 uses time-based seed).

Example (bash):

```bash
export 2048CIV_PERLIN_SCALE=0.02
export 2048CIV_PERLIN_OCTAVES=6
export 2048CIV_PERLIN_PERSISTENCE=0.45
./bin/2048civ
```

Changing these values alters continent size, terrain roughness, and biome distribution. Experiment to find settings you like.
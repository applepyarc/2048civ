/* hex_utils.h - Hex grid geometry utilities */
#ifndef HEX_UTILS_H
#define HEX_UTILS_H

#include <SDL2/SDL.h>
#include "path.h"   /* Terrain */

#ifdef __cplusplus
extern "C" {
#endif

/* Compute the 6 corner points of a flat-top hex centred at (cx,cy) with given radius. */
void hex_compute_points(int cx, int cy, int radius, SDL_Point *pts);

/* Pixel centre of hex cell (row,col) in world space (camera offset included). */
void hex_center(int row, int col, int radius, int cam_x, int cam_y, int *out_x, int *out_y);

/* Return 1 if screen point (x,y) is inside the polygon defined by pts[n]. */
int point_in_polygon(const SDL_Point *pts, int n, int x, int y);

/* Fill convex polygon with the current render draw color (scanline). */
void fill_polygon(SDL_Renderer *renderer, SDL_Point *pts, int n);

/* Return up to 6 neighbors of (r,c); writes into out_r[],out_c[], returns count. */
int get_neighbors(int r, int c, int rows, int cols, int *out_r, int *out_c);

/* RGBA color for a terrain type. */
void terrain_color(Terrain t, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);

#ifdef __cplusplus
}
#endif
#endif /* HEX_UTILS_H */

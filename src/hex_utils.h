#ifndef HEX_UTILS_H
#define HEX_UTILS_H

#include <SDL2/SDL.h>
#include "path.h"   /* Terrain */

#ifdef __cplusplus
extern "C" {
#endif

void hex_compute_points(int cx, int cy, int radius, SDL_Point *pts);
void hex_center(int row, int col, int radius, int cam_x, int cam_y, int *out_x, int *out_y);
int  point_in_polygon(const SDL_Point *pts, int n, int x, int y);
void fill_polygon(SDL_Renderer *renderer, SDL_Point *pts, int n);
int  get_neighbors(int r, int c, int rows, int cols, int *out_r, int *out_c);

/*
 * Hex grid distance between two cells (row,col) in the odd-q vertical layout
 * used throughout this project.  Returns the minimum number of steps to move
 * from (r1,c1) to (r2,c2) across adjacent cells.
 */
int hex_distance(int r1, int c1, int r2, int c2);

void terrain_color(Terrain t, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);

#ifdef __cplusplus
}
#endif
#endif /* HEX_UTILS_H */

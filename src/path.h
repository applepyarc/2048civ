/* path.h - A* pathfinding - dependency-injected, no extern coupling */
#ifndef PATH_H
#define PATH_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TERRAIN_PLAINS,
    TERRAIN_HILLS,
    TERRAIN_FOREST,
    TERRAIN_DESERT,
    TERRAIN_WATER,
    TERRAIN_MOUNTAIN,
    TERRAIN_COUNT
} Terrain;

/* Neighbor callback: given (r,c,rows,cols), fill out_r/out_c, return count (0..6). */
typedef int (*NeighborFn)(int r, int c, int rows, int cols, int *out_r, int *out_c);

/* Path result buffers (owned by path.c, reset on each compute_path call). */
extern int           *path_nodes; /* ordered cell indices start -> end */
extern int            path_len;
extern unsigned char *in_path;    /* flattened bool per cell */
extern int           *prev_node;  /* predecessor array (exposed for debugging) */

/*
 * Compute shortest path from (sr,sc) to (tr,tc) using A*.
 * Results written into path_nodes / path_len / in_path.
 * map     - flattened terrain array [r * cols + c]
 * get_nbr - neighbor function (pass hex_utils get_neighbors or custom)
 */
void compute_path(int sr, int sc, int tr, int tc,
                  int rows, int cols,
                  const Terrain *map,
                  NeighborFn get_nbr);

/* Free all internal path buffers. */
void path_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* PATH_H */
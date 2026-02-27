/* path.h - A* pathfinding interface and Terrain type */
#ifndef PATH_H
#define PATH_H

#include <stdlib.h>

typedef enum {
    TERRAIN_PLAINS,
    TERRAIN_HILLS,
    TERRAIN_FOREST,
    TERRAIN_DESERT,
    TERRAIN_WATER,
    TERRAIN_MOUNTAIN,
    TERRAIN_COUNT
} Terrain;

/* Path result buffers (owned by path.c) */
extern int *path_nodes; /* ordered indices from start->end */
extern int path_len;
extern unsigned char *in_path; /* flattened bool per cell */
extern int *prev_node; /* internal predecessor array (exposed for debugging) */

/* Compute shortest path from (sr,sc) to (tr,tc) using A*; results are
 * written into `path_nodes`/`path_len`/`in_path` .
 */
void compute_path(int sr, int sc, int tr, int tc);

/* Free any internal path buffers */
void path_cleanup(void);

#endif /* PATH_H */

#include <stdlib.h>
/* Convert odd-q coordinates to cube coords for distance heuristic */
static inline void oddq_to_cube(int col, int row, int *x, int *y, int *z) {
    int q = col;
    int r = row - (col - (col & 1)) / 2;
    *x = q; *z = r; *y = -(*x) - (*z);
}

/* Hex distance (cube coords) */
int hex_distance_cells(int r1, int c1, int r2, int c2) {
    int x1,y1,z1,x2,y2,z2;
    oddq_to_cube(c1, r1, &x1, &y1, &z1);
    oddq_to_cube(c2, r2, &x2, &y2, &z2);
    int dx = abs(x1 - x2), dy = abs(y1 - y2), dz = abs(z1 - z2);
    return (dx + dy + dz) / 2;
}

#include "path.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* Access map data from main program */
extern int g_map_rows;
extern int g_map_cols;
extern Terrain* g_terrain_map; /* flattened [r*cols + c] */

int *path_nodes = NULL;
int path_len = 0;
unsigned char *in_path = NULL;
int *prev_node = NULL;

/* forward declare neighbor helper implemented in main.c */
extern int get_neighbors(int r, int c, int *out_r, int *out_c);

/* cost per terrain (integer) */
static int terrain_cost_local(Terrain t) {
    switch (t) {
        case TERRAIN_PLAINS: return 10;
        case TERRAIN_HILLS: return 30;
        case TERRAIN_FOREST: return 50;
        case TERRAIN_DESERT: return 20;
        case TERRAIN_WATER: return 100;
        case TERRAIN_MOUNTAIN: return 10000;
        default: return 10;
    }
}

/* Minimal binary heap for A* (stores index, g-cost and f=g+h) */
typedef struct { int idx; int g; int f; } HeapNode;
typedef struct { HeapNode *a; int size, cap; } MinHeap;

static void heap_init(MinHeap *h, int cap) { h->a = malloc(sizeof(HeapNode)*cap); h->size = 0; h->cap = cap; }
static void heap_free(MinHeap *h) { free(h->a); h->a = NULL; h->size = h->cap = 0; }
static void heap_swap(HeapNode *x, HeapNode *y) { HeapNode t = *x; *x = *y; *y = t; }
static void heap_push(MinHeap *h, HeapNode v) {
    if (h->size >= h->cap) {
        int nc = h->cap*2 + 16;
        h->a = realloc(h->a, sizeof(HeapNode)*nc);
        h->cap = nc;
    }
    int i = h->size++;
    h->a[i] = v;
    while (i > 0) {
        int p = (i-1)/2;
        if (h->a[p].f <= h->a[i].f) break;
        heap_swap(&h->a[p], &h->a[i]); i = p;
    }
}
static HeapNode heap_pop(MinHeap *h) {
    HeapNode ret = h->a[0];
    h->a[0] = h->a[--h->size];
    int i = 0;
    while (1) {
        int l = i*2+1, r = i*2+2, smallest = i;
        if (l < h->size && h->a[l].f < h->a[smallest].f) smallest = l;
        if (r < h->size && h->a[r].f < h->a[smallest].f) smallest = r;
        if (smallest == i) break;
        heap_swap(&h->a[i], &h->a[smallest]); i = smallest;
    }
    return ret;
}

/* Convert odd-q coordinates to cube coords for distance heuristic */
static inline void oddq_to_cube(int col, int row, int *x, int *y, int *z) {
    int q = col;
    int r = row - (col - (col & 1)) / 2;
    *x = q; *z = r; *y = -(*x) - (*z);
}

/* Hex distance (cube coords) */
static inline int hex_distance_cells(int r1, int c1, int r2, int c2) {
    int x1,y1,z1,x2,y2,z2;
    oddq_to_cube(c1, r1, &x1, &y1, &z1);
    oddq_to_cube(c2, r2, &x2, &y2, &z2);
    int dx = abs(x1 - x2), dy = abs(y1 - y2), dz = abs(z1 - z2);
    return (dx + dy + dz) / 2;
}

void compute_path(int sr, int sc, int tr, int tc) {
    int n = g_map_rows * g_map_cols;
    if (!prev_node) prev_node = malloc(sizeof(int)*n);
    if (!in_path) in_path = calloc(n,1);
    int INF = 0x3f3f3f3f;
    int *gscore = malloc(sizeof(int)*n);
    for (int i = 0; i < n; ++i) { gscore[i] = INF; prev_node[i] = -1; in_path[i] = 0; }

    MinHeap open; heap_init(&open, 256);
    int sidx = sr * g_map_cols + sc, tidx = tr * g_map_cols + tc;

    const int min_cost = 10;
    int h0 = hex_distance_cells(sr, sc, tr, tc) * min_cost;
    gscore[sidx] = 0;
    heap_push(&open, (HeapNode){sidx, 0, 0 + h0});

    int *nbr_r = malloc(sizeof(int)*6);
    int *nbr_c = malloc(sizeof(int)*6);

    while (open.size > 0) {
        HeapNode hn = heap_pop(&open);
        int u = hn.idx;
        int ug = hn.g;
        if (ug != gscore[u]) continue;
        if (u == tidx) break;
        int ur = u / g_map_cols, uc = u % g_map_cols;
        int nc = get_neighbors(ur, uc, nbr_r, nbr_c);
        for (int i = 0; i < nc; ++i) {
            int vr = nbr_r[i], vc = nbr_c[i];
            int v = vr * g_map_cols + vc;
            int w = terrain_cost_local(g_terrain_map[v]);
            if (w >= 10000) continue;
            int tentative_g = gscore[u] + w;
            if (tentative_g < gscore[v]) {
                prev_node[v] = u;
                gscore[v] = tentative_g;
                int h = hex_distance_cells(vr, vc, tr, tc) * min_cost;
                int f = tentative_g + h;
                heap_push(&open, (HeapNode){v, tentative_g, f});
            }
        }
    }

    if (gscore[tidx] < INF) {
        int cur = tidx; int cnt = 0;
        while (cur != -1) { cnt++; cur = prev_node[cur]; }
        if (path_nodes) { free(path_nodes); path_nodes = NULL; }
        path_nodes = malloc(sizeof(int) * cnt);
        path_len = cnt;
        cur = tidx;
        for (int i = cnt - 1; i >= 0; --i) {
            path_nodes[i] = cur;
            in_path[cur] = 1;
            cur = prev_node[cur];
        }
    } else {
        path_len = 0;
        if (path_nodes) { free(path_nodes); path_nodes = NULL; }
    }
    free(gscore); free(nbr_r); free(nbr_c); heap_free(&open);
}

void path_cleanup(void) {
    if (prev_node) { free(prev_node); prev_node = NULL; }
    if (in_path) { free(in_path); in_path = NULL; }
    if (path_nodes) { free(path_nodes); path_nodes = NULL; }
    path_len = 0;
}

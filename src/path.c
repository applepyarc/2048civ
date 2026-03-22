/* path.c - A* pathfinding on a hex grid (dependency-injected) */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "path.h"

/* ---- Public path result buffers ---- */
int           *path_nodes = NULL;
int            path_len   = 0;
unsigned char *in_path    = NULL;
int           *prev_node  = NULL;

/* ---- Terrain movement cost ---- */
static int terrain_cost(Terrain t) {
    switch (t) {
        case TERRAIN_PLAINS:   return 10;
        case TERRAIN_HILLS:    return 30;
        case TERRAIN_FOREST:   return 50;
        case TERRAIN_DESERT:   return 20;
        case TERRAIN_WATER:    return 100;
        case TERRAIN_MOUNTAIN: return 10000; /* impassable */
        default:               return 10;
    }
}

/* ---- Minimal binary min-heap ---- */
typedef struct { int idx; int g; int f; } HeapNode;
typedef struct { HeapNode *a; int size, cap; } MinHeap;

static void heap_init(MinHeap *h, int cap) {
    h->a = malloc(sizeof(HeapNode) * cap);
    h->size = 0; h->cap = cap;
}
static void heap_free(MinHeap *h) { free(h->a); h->a = NULL; h->size = h->cap = 0; }
static void heap_swap(HeapNode *x, HeapNode *y) { HeapNode t = *x; *x = *y; *y = t; }

static void heap_push(MinHeap *h, HeapNode v) {
    if (h->size >= h->cap) {
        int nc = h->cap * 2 + 16;
        h->a = realloc(h->a, sizeof(HeapNode) * nc); h->cap = nc;
    }
    int i = h->size++;
    h->a[i] = v;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->a[p].f <= h->a[i].f) break;
        heap_swap(&h->a[p], &h->a[i]); i = p;
    }
}

static HeapNode heap_pop(MinHeap *h) {
    HeapNode ret = h->a[0];
    h->a[0] = h->a[--h->size];
    int i = 0;
    while (1) {
        int l = i*2+1, r = i*2+2, s = i;
        if (l < h->size && h->a[l].f < h->a[s].f) s = l;
        if (r < h->size && h->a[r].f < h->a[s].f) s = r;
        if (s == i) break;
        heap_swap(&h->a[i], &h->a[s]); i = s;
    }
    return ret;
}

/* ---- Hex distance heuristic (cube coordinates) ---- */
static void oddq_to_cube(int col, int row, int *x, int *y, int *z) {
    int q = col;
    int rr = row - (col - (col & 1)) / 2;
    *x = q; *z = rr; *y = -(*x) - (*z);
}

static int hex_distance(int r1, int c1, int r2, int c2) {
    int x1,y1,z1,x2,y2,z2;
    oddq_to_cube(c1,r1,&x1,&y1,&z1);
    oddq_to_cube(c2,r2,&x2,&y2,&z2);
    int dx=abs(x1-x2), dy=abs(y1-y2), dz=abs(z1-z2);
    return (dx+dy+dz)/2;
}

/* ---- A* ---- */
void compute_path(int sr, int sc, int tr, int tc,
                  int rows, int cols,
                  const Terrain *map,
                  NeighborFn get_nbr) {
    int n = rows * cols;
    int INF = 0x3f3f3f3f;

    if (!prev_node) prev_node = malloc(sizeof(int) * n);
    if (!in_path)   in_path   = calloc(n, 1);

    int *gscore = malloc(sizeof(int) * n);
    for (int i = 0; i < n; ++i) { gscore[i] = INF; prev_node[i] = -1; in_path[i] = 0; }

    int sidx = sr * cols + sc;
    int tidx = tr * cols + tc;
    const int min_cost = 10;

    MinHeap open; heap_init(&open, 256);
    gscore[sidx] = 0;
    heap_push(&open, (HeapNode){ sidx, 0, hex_distance(sr,sc,tr,tc) * min_cost });

    int nbr_r[6], nbr_c[6];

    while (open.size > 0) {
        HeapNode hn = heap_pop(&open);
        int u = hn.idx, ug = hn.g;
        if (ug != gscore[u]) continue; /* stale entry */
        if (u == tidx) break;

        int ur = u / cols, uc = u % cols;
        int nc = get_nbr(ur, uc, rows, cols, nbr_r, nbr_c);

        for (int i = 0; i < nc; ++i) {
            int vr = nbr_r[i], vc = nbr_c[i];
            int v = vr * cols + vc;
            int w = terrain_cost(map[v]);
            if (w >= 10000) continue; /* impassable */
            int tg = gscore[u] + w;
            if (tg < gscore[v]) {
                prev_node[v] = u;
                gscore[v]    = tg;
                int h = hex_distance(vr, vc, tr, tc) * min_cost;
                heap_push(&open, (HeapNode){ v, tg, tg + h });
            }
        }
    }

    /* reconstruct path */
    if (gscore[tidx] < INF) {
        int cur = tidx, cnt = 0;
        while (cur != -1) { cnt++; cur = prev_node[cur]; }
        if (path_nodes) { free(path_nodes); path_nodes = NULL; }
        path_nodes = malloc(sizeof(int) * cnt);
        path_len   = cnt;
        cur = tidx;
        for (int i = cnt - 1; i >= 0; --i) {
            path_nodes[i] = cur;
            in_path[cur]  = 1;
            cur = prev_node[cur];
        }
    } else {
        path_len = 0;
        if (path_nodes) { free(path_nodes); path_nodes = NULL; }
    }

    free(gscore);
    heap_free(&open);
}

void path_cleanup(void) {
    if (prev_node)  { free(prev_node);  prev_node  = NULL; }
    if (in_path)    { free(in_path);    in_path    = NULL; }
    if (path_nodes) { free(path_nodes); path_nodes = NULL; }
    path_len = 0;
}

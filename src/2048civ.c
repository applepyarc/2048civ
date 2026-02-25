/* =====================================================================================
 *       Filename: 2048civ.cpp
 *    Description: 
 *        Created: 2024/03/28 20时12分42秒
 *         Author: archer
 * =====================================================================================*/

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 700

/* runtime window/layout (set from config at startup) */
int g_window_width = WINDOW_WIDTH;
int g_window_height = WINDOW_HEIGHT;
/* width of main map area (left), info panel occupies remaining width */
int g_main_width = WINDOW_WIDTH * 4 / 5;
#define HEX_RADIUS 40 // 默认六边形半径
int current_radius = HEX_RADIUS;
const int MIN_RADIUS = 8;
const int MAX_RADIUS = 120;
/* minimum radius at which to show per-cell coordinates when enabled */
#define COORDS_SHOW_MIN_RADIUS 16
/* pixels threshold to treat mouse press+release as a click (not a drag) */
#define CLICK_DRAG_THRESHOLD 5
/* MAP size is provided by config at runtime */

// 地形枚举
typedef enum {
    TERRAIN_PLAINS,
    TERRAIN_HILLS,
    TERRAIN_FOREST,
    TERRAIN_DESERT,
    TERRAIN_WATER,
    TERRAIN_MOUNTAIN,
    TERRAIN_COUNT
} Terrain;

// 简单地形地图（已改为运行时分配）

/* Include runtime config and runtime-sized terrain buffer */
#include "config.h"
#include "perlin.h"

int g_map_rows = 0;
int g_map_cols = 0;
Terrain* g_terrain_map = NULL; /* flattened [row*cols + col] */
#define TERRAIN_AT(r,c) (g_terrain_map[(r)*g_map_cols + (c)])

// 当前选中的单元格
int selected_row = -1;
int selected_col = -1;
// Pathfinding
int path_start_row = -1, path_start_col = -1;
int path_end_row = -1, path_end_col = -1;
int *prev_node = NULL; /* flattened prev index */
unsigned char *in_path = NULL; /* flattened bool */
int *path_nodes = NULL; /* ordered indices from start->end */
int path_len = 0;
// neighbor highlight toggle and hover tracking
int highlight_neighbors_enabled = 1; /* default enabled */
int hover_row = -1, hover_col = -1;
// show cell coordinates toggle
int show_cell_coords_enabled = 1; /* default enabled */
// TTF font and info texture
TTF_Font* g_font = NULL;
SDL_Texture* g_info_tex = NULL;
int g_info_w = 0, g_info_h = 0;
// Camera / panning
int cam_x = 0;
int cam_y = 0;
int dragging = 0;
int drag_start_x = 0;
int drag_start_y = 0;
int cam_start_x = 0;
int cam_start_y = 0;

// Map bounds (world coords, without cam)
int map_min_x = 0, map_max_x = 0, map_min_y = 0, map_max_y = 0;

void compute_hex_points(int cx, int cy, int radius, SDL_Point* pts);

static inline int idx_of(int r, int c) { return r * g_map_cols + c; }

// neighbor offsets for odd-q vertical layout (cols offset)
int get_neighbors(int r, int c, int *out_r, int *out_c) {
    const int even_d[6][2] = {{-1,-1},{-1,0},{-1,1},{0,1},{1,0},{0,-1}};
    const int odd_d[6][2]  = {{0,-1},{-1,0},{0,1},{1,1},{1,0},{1,-1}};
    int count = 0;
    const int (*d)[2] = (c % 2 == 0) ? even_d : odd_d;
    for (int i = 0; i < 6; ++i) {
        int nr = r + d[i][0];
        int nc = c + d[i][1];
        if (nr >= 0 && nr < g_map_rows && nc >= 0 && nc < g_map_cols) {
            out_r[count] = nr; out_c[count] = nc; count++;
        }
    }
    return count;
}

// cost per terrain (integer)
int terrain_cost(Terrain t) {
    switch (t) {
        case TERRAIN_PLAINS: return 10;
        case TERRAIN_HILLS: return 30;
        case TERRAIN_FOREST: return 50;
        case TERRAIN_DESERT: return 20;
        case TERRAIN_WATER: return 100; /* swimmable but expensive */
        case TERRAIN_MOUNTAIN: return 10000; /* effectively impassable */
        default: return 10;
    }
}

// Minimal binary heap for A* (stores index, g-cost and f=g+h)
typedef struct { int idx; int g; int f; } HeapNode;
typedef struct { HeapNode *a; int size, cap; } MinHeap;

void heap_init(MinHeap *h, int cap) { h->a = malloc(sizeof(HeapNode)*cap); h->size = 0; h->cap = cap; }
void heap_free(MinHeap *h) { free(h->a); h->a = NULL; h->size = h->cap = 0; }
void heap_swap(HeapNode *x, HeapNode *y) { HeapNode t = *x; *x = *y; *y = t; }
void heap_push(MinHeap *h, HeapNode v) {
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
HeapNode heap_pop(MinHeap *h) {
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

// Convert odd-q coordinates to cube coords for distance heuristic
static inline void oddq_to_cube(int col, int row, int *x, int *y, int *z) {
    int q = col;
    int r = row - (col - (col & 1)) / 2;
    *x = q;
    *z = r;
    *y = -(*x) - (*z);
}

// Hex distance (cube coords)
static inline int hex_distance_cells(int r1, int c1, int r2, int c2) {
    int x1,y1,z1,x2,y2,z2;
    oddq_to_cube(c1, r1, &x1, &y1, &z1);
    oddq_to_cube(c2, r2, &x2, &y2, &z2);
    int dx = abs(x1 - x2), dy = abs(y1 - y2), dz = abs(z1 - z2);
    return (dx + dy + dz) / 2;
}

// Compute shortest path using A* from (sr,sc) to (tr,tc)
void compute_path(int sr, int sc, int tr, int tc) {
    int n = g_map_rows * g_map_cols;
    if (!prev_node) prev_node = malloc(sizeof(int)*n);
    if (!in_path) in_path = calloc(n,1);
    int INF = 0x3f3f3f3f;
    int *gscore = malloc(sizeof(int)*n);
    for (int i = 0; i < n; ++i) { gscore[i] = INF; prev_node[i] = -1; in_path[i] = 0; }

    MinHeap open; heap_init(&open, 256);
    int sidx = idx_of(sr,sc), tidx = idx_of(tr,tc);

    // heuristic multiplier: use minimum terrain cost as optimistic factor
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
        if (ug != gscore[u]) continue; // stale entry
        if (u == tidx) break;
        int ur = u / g_map_cols, uc = u % g_map_cols;
        int nc = get_neighbors(ur, uc, nbr_r, nbr_c);
        for (int i = 0; i < nc; ++i) {
            int vr = nbr_r[i], vc = nbr_c[i];
            int v = idx_of(vr,vc);
            int w = terrain_cost(TERRAIN_AT(vr,vc));
            if (w >= 10000) continue; // impassable
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

    // build path if reachable: record ordered nodes start->end in path_nodes
    if (gscore[tidx] < INF) {
        int cur = tidx;
        int cnt = 0;
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

// Compute map pixel bounds (including hex vertices) in world coords (no cam offset)
void compute_map_bounds(int radius) {
    int first = 1;
    for (int row = 0; row < g_map_rows; row++) {
        for (int col = 0; col < g_map_cols; col++) {
            int cx = col * (radius * 3 / 2) + radius + 50;
            int cy = row * (radius * sqrt(3)) + radius + 50;
            if (col % 2) cy += radius * sqrt(3) / 2;
            SDL_Point pts[6];
            compute_hex_points(cx, cy, radius, pts);
            for (int i = 0; i < 6; i++) {
                if (first) {
                    map_min_x = map_max_x = pts[i].x;
                    map_min_y = map_max_y = pts[i].y;
                    first = 0;
                } else {
                    if (pts[i].x < map_min_x) map_min_x = pts[i].x;
                    if (pts[i].x > map_max_x) map_max_x = pts[i].x;
                    if (pts[i].y < map_min_y) map_min_y = pts[i].y;
                    if (pts[i].y > map_max_y) map_max_y = pts[i].y;
                }
            }
        }
    }
}

// Clamp camera so map stays within window
void clamp_camera() {
    int min_cam_x = g_main_width - map_max_x;
    int max_cam_x = -map_min_x;
    if (min_cam_x > max_cam_x) {
        cam_x = (min_cam_x + max_cam_x) / 2; // center if map narrower than window
    } else {
        if (cam_x < min_cam_x) cam_x = min_cam_x;
        if (cam_x > max_cam_x) cam_x = max_cam_x;
    }

    int min_cam_y = g_window_height - map_max_y;
    int max_cam_y = -map_min_y;
    if (min_cam_y > max_cam_y) {
        cam_y = (min_cam_y + max_cam_y) / 2;
    } else {
        if (cam_y < min_cam_y) cam_y = min_cam_y;
        if (cam_y > max_cam_y) cam_y = max_cam_y;
    }
}

// 计算六边形中心点坐标
void hex_center(int row, int col, int radius, int* x, int* y) {
    *x = col * (radius * 3 / 2) + radius + 50 + cam_x;
    *y = row * (radius * sqrt(3)) + radius + 50 + cam_y;
    if (col % 2) {
        *y += radius * sqrt(3) / 2;
    }
}

// 取得地形对应的颜色
void terrain_color(Terrain t, Uint8* r, Uint8* g, Uint8* b, Uint8* a) {
    switch (t) {
        case TERRAIN_PLAINS:   *r=170; *g=210; *b=120; *a=255; break; // 草原
        case TERRAIN_HILLS:    *r=190; *g=170; *b=110; *a=255; break; // 丘陵
        case TERRAIN_FOREST:   *r=60;  *g=140; *b=60;  *a=255; break; // 森林
        case TERRAIN_DESERT:   *r=240; *g=220; *b=120; *a=255; break; // 沙漠
        case TERRAIN_WATER:    *r=70;  *g=130; *b=200; *a=255; break; // 水域
        case TERRAIN_MOUNTAIN: *r=120; *g=120; *b=120; *a=255; break; // 山地
        default:               *r=200; *g=200; *b=200; *a=255; break;
    }
}

// 使用扫描线填充凸多边形（六边形）
void fill_polygon(SDL_Renderer* renderer, SDL_Point* pts, int n) {
    if (n < 3) return;
    int min_y = pts[0].y, max_y = pts[0].y;
    for (int i = 1; i < n; i++) {
        if (pts[i].y < min_y) min_y = pts[i].y;
        if (pts[i].y > max_y) max_y = pts[i].y;
    }
    // 对每一条扫描线，求交点并填充
    for (int y = min_y; y <= max_y; y++) {
        int inter_count = 0;
        int inter_x[16];
        for (int i = 0; i < n; i++) {
            SDL_Point a = pts[i];
            SDL_Point b = pts[(i+1)%n];
            if (a.y == b.y) continue; // 忽略水平边
            int y0 = a.y < b.y ? a.y : b.y;
            int y1 = a.y < b.y ? b.y : a.y;
            if (y < y0 || y >= y1) continue;
            // x = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y)
            float x = a.x + (float)(y - a.y) * (b.x - a.x) / (float)(b.y - a.y);
            inter_x[inter_count++] = (int) (x + 0.5f);
        }
        if (inter_count < 2) continue;
        // 排序交点
        for (int i = 0; i < inter_count-1; i++) {
            for (int j = i+1; j < inter_count; j++) {
                if (inter_x[j] < inter_x[i]) { int tmp = inter_x[i]; inter_x[i] = inter_x[j]; inter_x[j] = tmp; }
            }
        }
        // 连对填充
        for (int i = 0; i < inter_count; i += 2) {
            if (i+1 < inter_count) {
                SDL_RenderDrawLine(renderer, inter_x[i], y, inter_x[i+1], y);
            }
        }
    }
}

// 计算六边形顶点（不绘制）
void compute_hex_points(int cx, int cy, int radius, SDL_Point* pts) {
    double angle = M_PI / 3.0;
    for (int i = 0; i < 6; i++) {
        pts[i].x = (int) (cx + radius * cos(i * angle));
        pts[i].y = (int) (cy + radius * sin(i * angle));
    }
}

// 点是否在多边形内（射线法，凸多边形也可用）
int point_in_polygon(SDL_Point* pts, int n, int x, int y) {
    int inside = 0;
    for (int i = 0, j = n-1; i < n; j = i++) {
        int xi = pts[i].x, yi = pts[i].y;
        int xj = pts[j].x, yj = pts[j].y;
        int intersect = ((yi > y) != (yj > y)) &&
            (x < (float)(xj - xi) * (y - yi) / (float)(yj - yi) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}



// Create texture from text (frees previous texture if non-NULL)
int create_text_texture(SDL_Renderer* renderer, const char* text) {
    if (!g_font) return 0;
    if (g_info_tex) { SDL_DestroyTexture(g_info_tex); g_info_tex = NULL; }
    SDL_Color col = {255,255,255,255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(g_font, text, col);
    if (!surf) return 0;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) { SDL_FreeSurface(surf); return 0; }
    g_info_tex = tex;
    g_info_w = surf->w; g_info_h = surf->h;
    SDL_FreeSurface(surf);
    return 1;
}

// Create a temporary texture from text and return it (caller must destroy)
SDL_Texture* create_text_texture_local(SDL_Renderer* renderer, const char* text, int *out_w, int *out_h) {
    if (!g_font) return NULL;
    SDL_Color col = {255,255,255,255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(g_font, text, col);
    if (!surf) return NULL;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex && out_w) *out_w = surf->w;
    if (tex && out_h) *out_h = surf->h;
    SDL_FreeSurface(surf);
    return tex;
}

// 绘制并填充六边形
void draw_hex_terrain(SDL_Renderer* renderer, int cx, int cy, int radius, Terrain t) {
    SDL_Point pts[6];
    compute_hex_points(cx, cy, radius, pts);
    // 填充颜色
    Uint8 r,g,b,a;
    terrain_color(t, &r, &g, &b, &a);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    fill_polygon(renderer, pts, 6);
    // 绘制边框（稍微深一点）
    SDL_SetRenderDrawColor(renderer, r>40?r-40:0, g>40?g-40:0, b>40?b-40:0, a);
    SDL_RenderDrawLines(renderer, pts, 6);
    // 闭环
    SDL_RenderDrawLine(renderer, pts[5].x, pts[5].y, pts[0].x, pts[0].y);
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
    }

    /* load config early so window size and split can be applied */
    config_init();
    g_window_width = config_get_window_width();
    g_window_height = config_get_window_height();
    float split = config_get_split_ratio();
    if (split <= 0.05f) split = 0.05f; if (split >= 0.95f) split = 0.95f;
    g_main_width = (int)(g_window_width * split);

    SDL_Window* window = SDL_CreateWindow(
        "Hex Terrain Map",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        g_window_width, g_window_height,
        0
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    g_map_rows = config_get_map_rows();
    g_map_cols = config_get_map_cols();
    g_terrain_map = (Terrain*)malloc(sizeof(Terrain) * g_map_rows * g_map_cols);
    if (!g_terrain_map) {
        fprintf(stderr, "Failed to allocate terrain map %dx%d\n", g_map_rows, g_map_cols);
        return 1;
    }

    const char* font_path = config_get_font_path();
    int font_size = config_get_font_size();
    g_font = TTF_OpenFont(font_path, font_size);
    if (!g_font) {
        fprintf(stderr, "Failed to open font '%s': %s\n", font_path, TTF_GetError());
    }

    /* 初始化地形：使用分形 Perlin 噪声生成更真实的地形 */
    unsigned int seed = (unsigned)time(NULL);
    /* initialize perlin with params from config */
    PerlinParams pp;
    config_get_perlin_params(&pp);
    if (pp.seed == 0) pp.seed = seed;
    perlin_init_with_params(&pp);

    for (int r = 0; r < g_map_rows; r++) {
        for (int c = 0; c < g_map_cols; c++) {
            float elev = perlin_elevation((float)c, (float)r);
            float m = perlin_moisture((float)c, (float)r);

            Terrain t;
            if (elev < 0.35f) {
                t = TERRAIN_WATER;
            } else if (elev > 0.85f) {
                t = TERRAIN_MOUNTAIN;
            } else if (elev > 0.6f) {
                /* higher ground: hills or forest depending on moisture */
                if (m > 0.55f) t = TERRAIN_FOREST;
                else t = TERRAIN_HILLS;
            } else {
                /* low/medium ground: desert/plains/forest by moisture */
                if (m < 0.28f) t = TERRAIN_DESERT;
                else if (m > 0.65f) t = TERRAIN_FOREST;
                else t = TERRAIN_PLAINS;
            }
            TERRAIN_AT(r,c) = t;
        }
    }

    /* 计算地图边界并初始化相机限制 */
    compute_map_bounds(current_radius - 1);
    clamp_camera();

    int running = 1;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x;
                int my = event.button.y;
                /* only start dragging when pressing in main map area; selection will be handled on mouse-up if not dragged */
                if (mx < g_main_width) {
                    dragging = 1;
                    drag_start_x = mx;
                    drag_start_y = my;
                    cam_start_x = cam_x;
                    cam_start_y = cam_y;
                }
                /* clicks in info panel are ignored for map interactions */
            }
            else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x;
                int my = event.button.y;
                int dx = mx - drag_start_x; if (dx < 0) dx = -dx;
                int dy = my - drag_start_y; if (dy < 0) dy = -dy;
                int is_click = (dx <= CLICK_DRAG_THRESHOLD && dy <= CLICK_DRAG_THRESHOLD);
                if (is_click && mx < g_main_width) {
                    /* treat as a click: perform selection logic */
                    /* if a path is currently displayed, clear it on any map click */
                    if (path_len > 0) {
                        int total = g_map_rows * g_map_cols;
                        if (path_nodes) { free(path_nodes); path_nodes = NULL; }
                        path_len = 0;
                        if (in_path) memset(in_path, 0, total);
                        create_text_texture(renderer, "Path cleared");
                    }
                    int found = 0;
                    for (int row = 0; row < g_map_rows && !found; row++) {
                        for (int col = 0; col < g_map_cols; col++) {
                            int cx, cy;
                            hex_center(row, col, current_radius, &cx, &cy);
                            SDL_Point pts[6];
                            compute_hex_points(cx, cy, current_radius-1, pts);
                            if (!point_in_polygon(pts, 6, mx, my)) continue;
                            /* path selection logic: first click = start, second click = end (compute path) */
                            Terrain t = TERRAIN_AT(row,col);
                            const char* names[] = {"Plains","Hills","Forest","Desert","Water","Mountain"};
                            char info[256];
                            if (path_start_row == -1) {
                                /* set start */
                                path_start_row = row; path_start_col = col;
                                /* mark selection for highlighting */
                                selected_row = row; selected_col = col;
                                /* clear any previous path */
                                if (in_path) memset(in_path, 0, g_map_rows * g_map_cols);
                                snprintf(info, sizeof(info), "Start: (%d,%d) Terrain: %s", row, col, names[t]);
                            } else if (path_start_row == row && path_start_col == col) {
                                /* clicked start again -> clear start */
                                path_start_row = path_start_col = -1;
                                if (in_path) memset(in_path, 0, g_map_rows * g_map_cols);
                                snprintf(info, sizeof(info), "Start cleared (%d,%d)", row, col);
                                /* clear selection highlight when clearing start */
                                selected_row = selected_col = -1;
                            } else if (path_end_row == -1) {
                                /* set end and compute path */
                                path_end_row = row; path_end_col = col;
                                /* mark selection */
                                selected_row = row; selected_col = col;
                                snprintf(info, sizeof(info), "End: (%d,%d) Terrain: %s", row, col, names[t]);
                                compute_path(path_start_row, path_start_col, path_end_row, path_end_col);
                                /* build path coordinate string for UI (truncate if long) */
                                if (path_len > 0) {
                                    char pbuf[1024];
                                    int pos = snprintf(pbuf, sizeof(pbuf), "Path len: %d  ", path_len);
                                    for (int pi = 0; pi < path_len; ++pi) {
                                        int idx = path_nodes[pi];
                                        int pr = idx / g_map_cols, pc = idx % g_map_cols;
                                        int n = snprintf(pbuf + pos, sizeof(pbuf) - pos, "(%d,%d)%s", pr, pc, (pi + 1 < path_len) ? "->" : "");
                                        pos += n;
                                        if (pos > (int)sizeof(pbuf) - 80) { snprintf(pbuf + pos, sizeof(pbuf) - pos, " ..."); break; }
                                    }
                                    /* append to the end of info */
                                    strncat(info, "  ", sizeof(info) - strlen(info) - 1);
                                    strncat(info, pbuf, sizeof(info) - strlen(info) - 1);
                                } else {
                                    strncat(info, "  No path found", sizeof(info) - strlen(info) - 1);
                                }
                            } else {
                                /* both set: start a new start */
                                path_start_row = row; path_start_col = col;
                                path_end_row = path_end_col = -1;
                                if (in_path) memset(in_path, 0, g_map_rows * g_map_cols);
                                snprintf(info, sizeof(info), "Start: (%d,%d) Terrain: %s", row, col, names[t]);
                            }
                            create_text_texture(renderer, info);
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        /* click on empty map area - clear selection and path start/end */
                        selected_row = selected_col = -1;
                        path_start_row = path_start_col = -1;
                        path_end_row = path_end_col = -1;
                        if (in_path) memset(in_path, 0, g_map_rows * g_map_cols);
                        SDL_SetWindowTitle(window, "Hex Terrain Map");
                        if (g_info_tex) { SDL_DestroyTexture(g_info_tex); g_info_tex = NULL; }
                    }
                }
                /* end dragging */
                dragging = 0;
            }
            else if (event.type == SDL_MOUSEWHEEL) {
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                /* only zoom when mouse is over main map area */
                if (mx < g_main_width) {
                    int old_r = current_radius;
                    float factor = (event.wheel.y > 0) ? 1.1f : 0.9f;
                    int new_r = (int) (old_r * factor + 0.5f);
                    if (new_r < MIN_RADIUS) new_r = MIN_RADIUS;
                    if (new_r > MAX_RADIUS) new_r = MAX_RADIUS;
                    if (new_r != old_r) {
                        float scale = (float)new_r / (float)old_r;
                        // zoom towards mouse: adjust camera so mouse points stays approximately same
                        cam_x = mx - (int)((mx - cam_x) * scale);
                        cam_y = my - (int)((my - cam_y) * scale);
                        current_radius = new_r;
                        compute_map_bounds(current_radius - 1);
                        clamp_camera();
                    }
                }
            }
            else if (event.type == SDL_MOUSEMOTION) {
                int mx = event.motion.x;
                int my = event.motion.y;
                if (dragging) {
                    cam_x = cam_start_x + (mx - drag_start_x);
                    cam_y = cam_start_y + (my - drag_start_y);
                    clamp_camera();
                    hover_row = hover_col = -1;
                } else {
                    /* only hover test when cursor is over main map area */
                    if (mx < g_main_width) {
                        int found = 0;
                        for (int row = 0; row < g_map_rows && !found; row++) {
                            for (int col = 0; col < g_map_cols; col++) {
                                int cx, cy; SDL_Point pts[6];
                                hex_center(row, col, current_radius, &cx, &cy);
                                compute_hex_points(cx, cy, current_radius-1, pts);
                                if (point_in_polygon(pts, 6, mx, my)) {
                                    hover_row = row; hover_col = col; found = 1; break;
                                }
                            }
                        }
                        if (!found) { hover_row = hover_col = -1; }
                    } else {
                        hover_row = hover_col = -1;
                    }
                }
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_n) {
                    highlight_neighbors_enabled = !highlight_neighbors_enabled;
                    char info[128];
                    snprintf(info, sizeof(info), "Highlight neighbors: %s", highlight_neighbors_enabled ? "ON" : "OFF");
                    create_text_texture(renderer, info);
                } else if (event.key.keysym.sym == SDLK_c) {
                    show_cell_coords_enabled = !show_cell_coords_enabled;
                    char info[128];
                    snprintf(info, sizeof(info), "Show cell coords: %s", show_cell_coords_enabled ? "ON" : "OFF");
                    create_text_texture(renderer, info);
                }
            }
        }
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); // 背景色
        SDL_RenderClear(renderer);

        /* restrict drawing of map to main map area (left pane) */
        SDL_Rect main_view = {0, 0, g_main_width, g_window_height};
        SDL_RenderSetViewport(renderer, &main_view);

        // 绘制六边形地图，根据地形设置颜色
        for (int row = 0; row < g_map_rows; row++) {
            for (int col = 0; col < g_map_cols; col++) {
                int cx, cy;
                hex_center(row, col, current_radius, &cx, &cy);
                draw_hex_terrain(renderer, cx, cy, current_radius - 1, TERRAIN_AT(row,col));

                if (show_cell_coords_enabled && current_radius >= COORDS_SHOW_MIN_RADIUS) {
                    char coordbuf[32];
                    snprintf(coordbuf, sizeof(coordbuf), "%d,%d", row, col);
                    int tw=0, th=0;
                    SDL_Texture* ttx = create_text_texture_local(renderer, coordbuf, &tw, &th);
                    if (ttx) {
                        SDL_SetTextureBlendMode(ttx, SDL_BLENDMODE_BLEND);
                        SDL_Rect td = { cx - tw/2, cy - th/2, tw, th };
                        SDL_RenderCopy(renderer, ttx, NULL, &td);
                        SDL_DestroyTexture(ttx);
                    }
                }

                int idx = idx_of(row,col);
                if (in_path && in_path[idx]) {
                    SDL_Point pts[6];
                    compute_hex_points(cx, cy, current_radius - 1, pts);
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(renderer, 0, 200, 200, 140);
                    fill_polygon(renderer, pts, 6);
                }
                // 如果被选中，高亮边框
                if (row == selected_row && col == selected_col) {
                    SDL_Point pts[6];
                    compute_hex_points(cx, cy, current_radius - 1, pts);
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 200);
                    // 再次绘制边框以示高亮
                    SDL_RenderDrawLines(renderer, pts, 6);
                    SDL_RenderDrawLine(renderer, pts[5].x, pts[5].y, pts[0].x, pts[0].y);
                }
            }
        }

        // 绘制邻居高亮（如果启用并有悬停单元）
        if (highlight_neighbors_enabled && hover_row >= 0 && hover_col >= 0) {
            int nbr_r[6], nbr_c[6];
            int nc = get_neighbors(hover_row, hover_col, nbr_r, nbr_c);
            SDL_Point pts[6];
            for (int i = 0; i < nc; ++i) {
                int rr = nbr_r[i], cc = nbr_c[i];
                int cx, cy; hex_center(rr, cc, current_radius, &cx, &cy);
                compute_hex_points(cx, cy, current_radius - 1, pts);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 255, 220, 0, 120);
                fill_polygon(renderer, pts, 6);
                SDL_SetRenderDrawColor(renderer, 255, 220, 0, 200);
                SDL_RenderDrawLines(renderer, pts, 6);
                SDL_RenderDrawLine(renderer, pts[5].x, pts[5].y, pts[0].x, pts[0].y);
            }
            /* highlight hovered cell differently */
            int hcx, hcy; hex_center(hover_row, hover_col, current_radius, &hcx, &hcy);
            compute_hex_points(hcx, hcy, current_radius - 1, pts);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 160);
            SDL_RenderDrawLines(renderer, pts, 6);
            SDL_RenderDrawLine(renderer, pts[5].x, pts[5].y, pts[0].x, pts[0].y);
        }

        // 绘制路径线段（如果存在）
        if (path_len >= 2 && path_nodes) {
            SDL_Point *pline = malloc(sizeof(SDL_Point) * path_len);
            for (int i = 0; i < path_len; ++i) {
                int idx = path_nodes[i];
                int pr = idx / g_map_cols, pc = idx % g_map_cols;
                hex_center(pr, pc, current_radius, &pline[i].x, &pline[i].y);
            }
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 255, 80, 80, 220);
            SDL_RenderDrawLines(renderer, pline, path_len);
            // draw endpoints
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_Rect rs = { pline[0].x - 4, pline[0].y - 4, 8, 8 };
            SDL_Rect re = { pline[path_len-1].x - 4, pline[path_len-1].y - 4, 8, 8 };
            SDL_RenderFillRect(renderer, &rs);
            SDL_RenderFillRect(renderer, &re);
            free(pline);
        }

        /* restore full-window viewport for UI/info panel */
        SDL_RenderSetViewport(renderer, NULL);

        // 绘制信息纹理（信息面板，右侧）
        if (g_info_tex) {
            int info_x = g_main_width + 10;
            SDL_Rect bg = { info_x, 10, g_info_w + 10, g_info_h + 8 };
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
            SDL_RenderFillRect(renderer, &bg);
            SDL_Rect dst = { info_x + 5, 14, g_info_w, g_info_h };
            SDL_RenderCopy(renderer, g_info_tex, NULL, &dst);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    if (g_info_tex) SDL_DestroyTexture(g_info_tex);
    if (g_font) TTF_CloseFont(g_font);
    if (prev_node) { free(prev_node); prev_node = NULL; }
    if (in_path) { free(in_path); in_path = NULL; }
    if (path_nodes) { free(path_nodes); path_nodes = NULL; }
    if (g_terrain_map) { free(g_terrain_map); g_terrain_map = NULL; }
    config_free();
    TTF_Quit();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

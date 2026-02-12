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

#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 700

#define HEX_RADIUS 40 // 六边形半径
#define MAP_ROWS 8
#define MAP_COLS 10

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

// 简单地形地图（可替换为随机或加载）
Terrain terrain_map[MAP_ROWS][MAP_COLS];

// 当前选中的单元格
int selected_row = -1;
int selected_col = -1;
// TTF font and info texture
TTF_Font* g_font = NULL;
SDL_Texture* g_info_tex = NULL;
int g_info_w = 0, g_info_h = 0;

// 计算六边形中心点坐标
void hex_center(int row, int col, int radius, int* x, int* y) {
    *x = col * (radius * 3 / 2) + radius + 50;
    *y = row * (radius * sqrt(3)) + radius + 50;
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

    SDL_Window* window = SDL_CreateWindow(
        "Hex Terrain Map",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        0
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // 尝试加载系统字体（DejaVu），字体大小 16
    const char* font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    g_font = TTF_OpenFont(font_path, 16);
    if (!g_font) {
        fprintf(stderr, "Failed to open font '%s': %s\n", font_path, TTF_GetError());
    }

    // 初始化地形（示例：伪随机或图案）
    srand((unsigned)time(NULL));
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            int v = (r + c*2 + rand()%3) % TERRAIN_COUNT;
            terrain_map[r][c] = (Terrain)v;
        }
    }

    int running = 1;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x;
                int my = event.button.y;
                int found = 0;
                for (int row = 0; row < MAP_ROWS && !found; row++) {
                    for (int col = 0; col < MAP_COLS; col++) {
                        int cx, cy;
                        hex_center(row, col, HEX_RADIUS, &cx, &cy);
                        SDL_Point pts[6];
                        compute_hex_points(cx, cy, HEX_RADIUS-1, pts);
                        if (point_in_polygon(pts, 6, mx, my)) {
                            selected_row = row;
                            selected_col = col;
                            Terrain t = terrain_map[row][col];
                            const char* names[] = {"Plains","Hills","Forest","Desert","Water","Mountain"};
                            char title[128];
                            snprintf(title, sizeof(title), "Hex (%d,%d) - %s", row, col, names[t]);
                            SDL_SetWindowTitle(window, title);
                            char info[256];
                            snprintf(info, sizeof(info), "Cell: (%d,%d)  Terrain: %s", row, col, names[t]);
                            create_text_texture(renderer, info);
                            found = 1;
                            break;
                        }
                    }
                }
                if (!found) {
                    selected_row = selected_col = -1;
                    SDL_SetWindowTitle(window, "Hex Terrain Map");
                    if (g_info_tex) { SDL_DestroyTexture(g_info_tex); g_info_tex = NULL; }
                }
            }
        }
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); // 背景色
        SDL_RenderClear(renderer);

        // 绘制六边形地图，根据地形设置颜色
        for (int row = 0; row < MAP_ROWS; row++) {
            for (int col = 0; col < MAP_COLS; col++) {
                int cx, cy;
                hex_center(row, col, HEX_RADIUS, &cx, &cy);
                draw_hex_terrain(renderer, cx, cy, HEX_RADIUS - 1, terrain_map[row][col]);
                // 如果被选中，高亮边框
                if (row == selected_row && col == selected_col) {
                    SDL_Point pts[6];
                    compute_hex_points(cx, cy, HEX_RADIUS - 1, pts);
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 200);
                    // 再次绘制边框以示高亮
                    SDL_RenderDrawLines(renderer, pts, 6);
                    SDL_RenderDrawLine(renderer, pts[5].x, pts[5].y, pts[0].x, pts[0].y);
                }
            }
        }

        // 绘制信息纹理（左上角）
        if (g_info_tex) {
            SDL_Rect bg = {10, 10, g_info_w + 10, g_info_h + 8};
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
            SDL_RenderFillRect(renderer, &bg);
            SDL_Rect dst = {15, 14, g_info_w, g_info_h};
            SDL_RenderCopy(renderer, g_info_tex, NULL, &dst);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    if (g_info_tex) SDL_DestroyTexture(g_info_tex);
    if (g_font) TTF_CloseFont(g_font);
    TTF_Quit();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

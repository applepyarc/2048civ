/* =====================================================================================
 *       Filename: 2048civ.cpp
 *    Description: 
 *        Created: 2024/03/28 20时12分42秒
 *         Author: archer
 * =====================================================================================*/

#include <SDL2/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

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

// 绘制并填充六边形
void draw_hex_terrain(SDL_Renderer* renderer, int cx, int cy, int radius, Terrain t) {
    double angle = M_PI / 3.0; // 60度
    SDL_Point pts[6];
    for (int i = 0; i < 6; i++) {
        pts[i].x = (int) (cx + radius * cos(i * angle));
        pts[i].y = (int) (cy + radius * sin(i * angle));
    }
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

    SDL_Window* window = SDL_CreateWindow(
        "Hex Terrain Map",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        0
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

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
        }
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); // 背景色
        SDL_RenderClear(renderer);

        // 绘制六边形地图，根据地形设置颜色
        for (int row = 0; row < MAP_ROWS; row++) {
            for (int col = 0; col < MAP_COLS; col++) {
                int cx, cy;
                hex_center(row, col, HEX_RADIUS, &cx, &cy);
                draw_hex_terrain(renderer, cx, cy, HEX_RADIUS - 1, terrain_map[row][col]);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

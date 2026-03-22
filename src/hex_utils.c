/* hex_utils.c - Hex grid geometry utilities */
#include <math.h>
#include "hex_utils.h"

void hex_compute_points(int cx, int cy, int radius, SDL_Point *pts) {
    double angle = M_PI / 3.0;
    for (int i = 0; i < 6; i++) {
        pts[i].x = (int)(cx + radius * cos(i * angle));
        pts[i].y = (int)(cy + radius * sin(i * angle));
    }
}

void hex_center(int row, int col, int radius, int cam_x, int cam_y, int *out_x, int *out_y) {
    *out_x = col * (radius * 3 / 2) + radius + 50 + cam_x;
    *out_y = row * (int)(radius * sqrt(3)) + radius + 50 + cam_y;
    if (col % 2) {
        *out_y += (int)(radius * sqrt(3) / 2);
    }
}

int point_in_polygon(const SDL_Point *pts, int n, int x, int y) {
    int inside = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        int xi = pts[i].x, yi = pts[i].y;
        int xj = pts[j].x, yj = pts[j].y;
        int intersect = ((yi > y) != (yj > y)) &&
            (x < (float)(xj - xi) * (y - yi) / (float)(yj - yi) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

void fill_polygon(SDL_Renderer *renderer, SDL_Point *pts, int n) {
    if (n < 3) return;
    int min_y = pts[0].y, max_y = pts[0].y;
    for (int i = 1; i < n; i++) {
        if (pts[i].y < min_y) min_y = pts[i].y;
        if (pts[i].y > max_y) max_y = pts[i].y;
    }
    for (int y = min_y; y <= max_y; y++) {
        int inter_count = 0, inter_x[16];
        for (int i = 0; i < n; i++) {
            SDL_Point a = pts[i], b = pts[(i + 1) % n];
            if (a.y == b.y) continue;
            int y0 = a.y < b.y ? a.y : b.y;
            int y1 = a.y < b.y ? b.y : a.y;
            if (y < y0 || y >= y1) continue;
            float fx = a.x + (float)(y - a.y) * (b.x - a.x) / (float)(b.y - a.y);
            inter_x[inter_count++] = (int)(fx + 0.5f);
        }
        if (inter_count < 2) continue;
        for (int i = 0; i < inter_count - 1; i++)
            for (int j = i + 1; j < inter_count; j++)
                if (inter_x[j] < inter_x[i]) { int t = inter_x[i]; inter_x[i] = inter_x[j]; inter_x[j] = t; }
        for (int i = 0; i + 1 < inter_count; i += 2)
            SDL_RenderDrawLine(renderer, inter_x[i], y, inter_x[i + 1], y);
    }
}

int get_neighbors(int r, int c, int rows, int cols, int *out_r, int *out_c) {
    static const int even_d[6][2] = {{-1,-1},{-1,0},{-1,1},{0,1},{1,0},{0,-1}};
    static const int odd_d[6][2]  = {{0,-1},{-1,0},{0,1},{1,1},{1,0},{1,-1}};
    const int (*d)[2] = (c % 2 == 0) ? even_d : odd_d;
    int count = 0;
    for (int i = 0; i < 6; ++i) {
        int nr = r + d[i][0], nc = c + d[i][1];
        if (nr >= 0 && nr < rows && nc >= 0 && nc < cols)
            { out_r[count] = nr; out_c[count] = nc; count++; }
    }
    return count;
}

void terrain_color(Terrain t, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a) {
    switch (t) {
        case TERRAIN_PLAINS:   *r=170;*g=210;*b=120;*a=255; break;
        case TERRAIN_HILLS:    *r=190;*g=170;*b=110;*a=255; break;
        case TERRAIN_FOREST:   *r=60; *g=140;*b=60; *a=255; break;
        case TERRAIN_DESERT:   *r=240;*g=220;*b=120;*a=255; break;
        case TERRAIN_WATER:    *r=70; *g=130;*b=200;*a=255; break;
        case TERRAIN_MOUNTAIN: *r=120;*g=120;*b=120;*a=255; break;
        default:               *r=200;*g=200;*b=200;*a=255; break;
    }
}

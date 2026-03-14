/* =====================================================================================
 *       Filename: 2048civ.cpp
 *    Description:
 *        Created: 2024/03/28 20:12:42
 *         Author: archer
 * =====================================================================================*/

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "sprite.h"
/* Terrain type and path API */
#include "path.h"
/* Include runtime config and runtime-sized terrain buffer */
#include "config.h"
#include "perlin.h"
#include "job.h"
#include "hex_utils.h"

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
int g_map_rows = 0;
int g_map_cols = 0;
Terrain* g_terrain_map = NULL; /* flattened [row*cols + col] */
#define TERRAIN_AT(r,c) (g_terrain_map[(r)*g_map_cols + (c)])

// 当前选中的单元格
int selected_row = -1;
int selected_col = -1;
// Pathfinding state
int path_start_row = -1, path_start_col = -1;
int path_end_row = -1, path_end_col = -1;
int path_preview_row = -1, path_preview_col = -1;
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

// 玩家菜单系统
#define MENU_OPTION_COUNT 5
typedef enum {
    MENU_MOVE = 0,
    MENU_ATTACK,
    MENU_ITEM,
    MENU_EQUIP,
    MENU_WAIT
} MenuOption;

int show_player_menu = 0; // 是否显示玩家菜单
int menu_selected_option = 0; // 当前选中的菜单项
int menu_x = 0, menu_y = 0; // 菜单位置
int menu_width = 120, menu_height = 180; // 菜单尺寸
const char* menu_options[MENU_OPTION_COUNT] = {
    "Move",
    "Attack",
    "Items",
    "Equipment",
    "Wait"
};

int attack_mode = ATTACK_MODE_NONE; // 当前攻击模式
int attack_target_row = -1, attack_target_col = -1; // 攻击目标位置
int attack_range = 1; // 默认攻击范围


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

// 计算右侧信息面板的可用宽度
int get_info_panel_width() {
    return g_window_width - g_main_width - 20; // 减去边距
}

// 将文本按指定宽度自动换行
char* wrap_text(const char* text, int max_width) {
    if (!text || !g_font) return NULL;

    char* result = malloc(strlen(text) * 2 + 1); // 预留足够空间
    if (!result) return NULL;

    char* current_pos = result;
    const char* start = text;
    const char* end = text;

    while (*end) {
        // 找到下一个换行符或字符串结束
        const char* line_end = strchr(end, '\n');
        if (!line_end) line_end = text + strlen(text);

        // 处理这一行
        const char* line_start = end;
        const char* word_start = line_start;
        const char* word_end = line_start;

        while (word_start < line_end) {
            // 找到下一个单词边界
            while (*word_end && *word_end != ' ' && *word_end != '\t' && word_end < line_end) {
                word_end++;
            }

            // 计算当前行的宽度
            char temp_line[256];
            int line_len = word_end - line_start;
            if (line_len >= sizeof(temp_line)) line_len = sizeof(temp_line) - 1;
            strncpy(temp_line, line_start, line_len);
            temp_line[line_len] = '\0';

            int line_width = 0;
            TTF_SizeUTF8(g_font, temp_line, &line_width, NULL);

            // 如果超出宽度，换行
            if (line_width > max_width && word_start > line_start) {
                // 复制当前行（不包括最后一个单词）
                int copy_len = word_start - line_start - 1;
                if (copy_len > 0) {
                    strncpy(current_pos, line_start, copy_len);
                    current_pos += copy_len;
                }
                *current_pos++ = '\n';
                line_start = word_start;
            }

            // 移动到下一个单词
            word_start = word_end;
            while (*word_start && (*word_start == ' ' || *word_start == '\t') && word_start < line_end) {
                word_start++;
            }
            word_end = word_start;
        }

        // 复制剩余的行内容
        int copy_len = line_end - line_start;
        if (copy_len > 0) {
            strncpy(current_pos, line_start, copy_len);
            current_pos += copy_len;
        }

        // 处理换行符
        if (*line_end == '\n') {
            *current_pos++ = '\n';
            end = line_end + 1;
        } else {
            end = line_end;
        }
    }

    *current_pos = '\0';
    return result;
}

// Create texture from text with automatic word wrapping (frees previous texture if non-NULL)
int create_text_texture(SDL_Renderer* renderer, const char* text) {
    if (!g_font) return 0;
    if (g_info_tex) { SDL_DestroyTexture(g_info_tex); g_info_tex = NULL; }

    // 计算可用宽度
    int max_width = get_info_panel_width();

    // 自动换行
    char* wrapped_text = wrap_text(text, max_width);
    if (!wrapped_text) return 0;

    SDL_Color col = {255,255,255,255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended_Wrapped(g_font, wrapped_text, col, max_width);

    free(wrapped_text);

    if (!surf) return 0;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) { SDL_FreeSurface(surf); return 0; }
    g_info_tex = tex;
    g_info_w = surf->w;
    g_info_h = surf->h;

    // 确保宽度不超过面板宽度
    if (g_info_w > max_width) {
        g_info_w = max_width;
    }

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

/* Create and set a multi-line info texture from an array of lines. */
int set_info_lines(SDL_Renderer* renderer, const char** lines, int nlines) {
    if (!g_font || nlines <= 0) return 0;
    SDL_Surface** line_surfs = malloc(sizeof(SDL_Surface*) * nlines);
    if (!line_surfs) return 0;
    int max_w = 0; int total_h = 0; int spacing = 4;
    for (int i = 0; i < nlines; ++i) {
        SDL_Color col = {255,255,255,255};
        line_surfs[i] = TTF_RenderUTF8_Blended(g_font, lines[i] ? lines[i] : "", col);
        if (!line_surfs[i]) {
            for (int j = 0; j < i; ++j) SDL_FreeSurface(line_surfs[j]);
            free(line_surfs);
            return 0;
        }
        if (line_surfs[i]->w > max_w) max_w = line_surfs[i]->w;
        total_h += line_surfs[i]->h + (i > 0 ? spacing : 0);
    }

    SDL_Surface* dest = SDL_CreateRGBSurfaceWithFormat(0, max_w, total_h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!dest) {
        for (int i = 0; i < nlines; ++i) SDL_FreeSurface(line_surfs[i]);
        free(line_surfs);
        return 0;
    }
    SDL_FillRect(dest, NULL, SDL_MapRGBA(dest->format, 0,0,0,0));

    int y = 0;
    for (int i = 0; i < nlines; ++i) {
        SDL_Rect dst = {0, y, line_surfs[i]->w, line_surfs[i]->h};
        SDL_BlitSurface(line_surfs[i], NULL, dest, &dst);
        y += line_surfs[i]->h + spacing;
        SDL_FreeSurface(line_surfs[i]);
    }
    free(line_surfs);

    if (g_info_tex) { SDL_DestroyTexture(g_info_tex); g_info_tex = NULL; }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, dest);
    if (!tex) { SDL_FreeSurface(dest); return 0; }
    g_info_tex = tex;
    g_info_w = dest->w; g_info_h = dest->h;
    SDL_FreeSurface(dest);
    return 1;
}

// 渲染玩家菜单
void render_player_menu(SDL_Renderer* renderer) {
    if (!show_player_menu) return;

    // 获取当前鼠标位置
    int mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);

    // 绘制菜单背景
    SDL_Rect menu_rect = {menu_x, menu_y, menu_width, menu_height};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 220);
    SDL_RenderFillRect(renderer, &menu_rect);

    // 绘制菜单边框
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &menu_rect);

    // 绘制菜单选项
    int option_height = menu_height / MENU_OPTION_COUNT;
    int hovered_option = -1; // 鼠标悬停的选项

    for (int i = 0; i < MENU_OPTION_COUNT; i++) {
        SDL_Rect option_rect = {menu_x, menu_y + i * option_height, menu_width, option_height};

        // 检查鼠标是否悬停在此选项上
        int is_hovered = (mouse_x >= option_rect.x && mouse_x <= option_rect.x + option_rect.w &&
                         mouse_y >= option_rect.y && mouse_y <= option_rect.y + option_rect.h);

        if (is_hovered) {
            hovered_option = i;
        }

        // 高亮选中的选项或鼠标悬停的选项
        if (i == menu_selected_option || is_hovered) {
            SDL_SetRenderDrawColor(renderer, 80, 80, 200, 180);
            SDL_RenderFillRect(renderer, &option_rect);
        }

        // 绘制选项文本
        if (g_font) {
            SDL_Color text_color = {255, 255, 255, 255};
            SDL_Surface* text_surface = TTF_RenderUTF8_Blended(g_font, menu_options[i], text_color);
            if (text_surface) {
                SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
                if (text_texture) {
                    SDL_Rect text_rect = {
                        menu_x + 10,
                        menu_y + i * option_height + (option_height - text_surface->h) / 2,
                        text_surface->w,
                        text_surface->h
                    };
                    SDL_RenderCopy(renderer, text_texture, NULL, &text_rect);
                    SDL_DestroyTexture(text_texture);
                }
                SDL_FreeSurface(text_surface);
            }
        }

        // 绘制选项分隔线
        if (i < MENU_OPTION_COUNT - 1) {
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
            SDL_RenderDrawLine(renderer,
                menu_x, menu_y + (i+1) * option_height,
                menu_x + menu_width, menu_y + (i+1) * option_height);
        }
    }
}

/* Build and display sprite info lines in the right info panel. */
void show_sprite_info(SDL_Renderer* renderer, Sprite* s) {
    if (!s) return;
    char l0[128], l1[128], l2[128], l3[128], l4[128], l5[128], l6[128], l7[128], l8[128], l9[256] = {0};
    snprintf(l0, sizeof(l0), "Name: %s", s->name ? s->name : "");
    snprintf(l1, sizeof(l1), "Job: %s", s->job ? s->job : "");
    snprintf(l2, sizeof(l2), "Level: %d", s->level);
    snprintf(l3, sizeof(l3), "HP: %d / %d", s->hp, s->max_hp);
    snprintf(l4, sizeof(l4), "MP: %d / %d", s->mp, s->max_mp);
    snprintf(l5, sizeof(l5), "ATK: %d  DEF: %d", s->attack, s->defense);
    snprintf(l6, sizeof(l6), "Speed: %d  Jump: %d", s->speed, s->jump);
    snprintf(l7, sizeof(l7), "Move: %d", s->move);
    snprintf(l8, sizeof(l8), "Position: (%d,%d)", s->x, s->y);
    for (int i = 0; i < MAX_EQUIP_SLOTS; i++) {
        const char* eq_name = (s->equipments[i].name) ? s->equipments[i].name : "None";
        snprintf(l9 + strlen(l9), 40, "Equip%d: %s ", i+1, eq_name);
    }
    const char* lines[10] = { l0,l1,l2,l3,l4,l5,l6,l7,l8,l9 };
    set_info_lines(renderer, lines, 10);
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

    /* Demo: create a randomized player sprite and an enemy sprite placed on the map */
    srand((unsigned)time(NULL));

    /* Load atlas texture and parse atlas file for frames */
    SDL_Texture *atlas_tex = NULL;
    SDL_Rect player_src = {0,0,16,16}, enemy_src = {0,0,16,16};
    /* optional run animation frames for player loaded from atlas */
    int player_run_x = 0, player_run_y = 0, player_run_w = 0, player_run_h = 0;
    int player_run_frames = 0;
    if (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) {
        SDL_Surface *surf = IMG_Load("res/drawable/dungeon.png");
        if (surf) {
            atlas_tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
        } else {
            fprintf(stderr, "Failed to load atlas image: %s\n", IMG_GetError());
        }
    } else {
        fprintf(stderr, "SDL_image PNG support not initialized\n");
    }

    /* parse atlas description file for named entries */
    FILE *af = fopen("res/drawable/dungeon", "r");
    if (af) {
        char name[256];
        while (fscanf(af, "%255s", name) == 1) {
            int x, y, w, h; int frames = 1;
                if (fscanf(af, "%d %d %d %d", &x, &y, &w, &h) >= 4) {
                    /* optional frames field */
                    fscanf(af, "%d", &frames);
                    if (strcmp(name, "knight_f_idle_anim") == 0) {
                        player_src.x = x; player_src.y = y; player_src.w = w; player_src.h = h;
                    } else if (strcmp(name, "knight_f_run_anim") == 0) {
                        player_run_x = x; player_run_y = y; player_run_w = w; player_run_h = h; player_run_frames = frames > 0 ? frames : 1;
                    } else if (strcmp(name, "big_zombie_idle_anim") == 0) {
                        enemy_src.x = x; enemy_src.y = y; enemy_src.w = w; enemy_src.h = h;
                    }
            } else break;
        }
        fclose(af);
    } else {
        fprintf(stderr, "Failed to open atlas description file res/drawable/dungeon\n");
    }

    Sprite *player = sprite_create("Player", "Warrior", NULL, 1);
    Sprite *enemy = sprite_create("Enemy", "Goblin", NULL, 1);
    if (player) {
        player->level = 1 + rand() % 10;
        player->max_hp = 80 + rand() % (200 - 80 + 1);
        player->hp = player->max_hp/2 + rand() % (player->max_hp - player->max_hp/2 + 1);
        player->max_mp = 10 + rand() % (80 - 10 + 1);
        player->mp = rand() % (player->max_mp + 1);
        player->attack = 5 + rand() % (40 - 5 + 1);
        player->defense = rand() % 31;
        player->speed = 1 + rand() % 10;
        player->move = 1 + rand() % 3;
        player->jump = 1 + rand() % 3;
        int pr = rand() % g_map_rows; int pc = rand() % g_map_cols;
        sprite_set_position(player, pr, pc);
        printf("Player created at cell (%d,%d): lvl=%d HP=%d/%d MP=%d/%d ATK=%d DEF=%d\n",
               player->x, player->y, player->level, player->hp, player->max_hp,
               player->mp, player->max_mp, player->attack, player->defense);
    }
    if (enemy) {
        enemy->level = 1 + rand() % 8;
        enemy->max_hp = 40 + rand() % (160 - 40 + 1);
        enemy->hp = enemy->max_hp/2 + rand() % (enemy->max_hp - enemy->max_hp/2 + 1);
        enemy->max_mp = rand() % 41;
        enemy->mp = rand() % (enemy->max_mp + 1);
        enemy->attack = 4 + rand() % (30 - 4 + 1);
        enemy->defense = rand() % 21;
        enemy->speed = 1 + rand() % 8;
        enemy->move = 1 + rand() % 2;
        enemy->jump = 1 + rand() % 2;
        int er = rand() % g_map_rows; int ec = rand() % g_map_cols;
        sprite_set_position(enemy, er, ec);
        printf("Enemy created at cell (%d,%d): lvl=%d HP=%d/%d MP=%d/%d ATK=%d DEF=%d\n",
               enemy->x, enemy->y, enemy->level, enemy->hp, enemy->max_hp,
               enemy->mp, enemy->max_mp, enemy->attack, enemy->defense);
    }

    /* movement state: when a path (path_nodes) is computed and an endpoint selected,
        we will animate the player along the path at a configurable ms-per-tile speed. */
    int moving = 0;
    int move_index = 0; /* next index in path_nodes to move to (0 is start) */
    Uint32 last_move_tick = 0;
    int move_ms = config_get_move_ms();
    /* animation timing for run frames */
    Uint32 last_anim_tick = 0;
    int player_run_frame = 0;
    int anim_frame_ms = 120; /* ms per animation frame while running */
    /* interpolation state for smooth movement between hex cells */
    int move_from_r = -1, move_from_c = -1;
    int move_to_r = -1, move_to_c = -1;
    float move_progress = 0.0f; /* 0.0 .. 1.0 */

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
                            path_preview_row = path_preview_col = -1;
                        /* stop any ongoing movement when user clicks to change selection */
                        moving = 0;
                        /* reset interpolation state */
                        move_progress = 0.0f;
                        move_from_r = move_from_c = move_to_r = move_to_c = -1;
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
                            if (attack_mode != ATTACK_MODE_NONE) {
                                // 攻击模式：选择攻击目标
                                if (enemy && enemy->x == row && enemy->y == col) {
                                    // 计算攻击距离
                                    int distance = hex_distance_cells(player->x, player->y, row, col);

                                    if (distance <= attack_range) {
                                        // 执行攻击
                                        int damage = sprite_attack(player, enemy, attack_mode);

                                        // 显示攻击结果
                                        char attack_info[256];
                                        const char* attack_type = (attack_mode == ATTACK_MODE_PHYSICAL) ? "Physical" : "Magic";
                                        snprintf(attack_info, sizeof(attack_info),
                                                "%s vs %s\n%d %s damage\n%sHP: %d/%d\n",
                                                player->name, enemy->name, damage, attack_type,
                                                enemy->name, enemy->hp, enemy->max_hp);

                                        // 检查是否击败敌人
                                        if (enemy->hp <= 0) {
                                            strncat(attack_info, "\n敌人被击败！", sizeof(attack_info) - strlen(attack_info) - 1);
                                        }

                                        create_text_texture(renderer, attack_info);

                                        // 退出攻击模式
                                        attack_mode = ATTACK_MODE_NONE;
                                        found = 1;
                                        break;
                                    } else {
                                        snprintf(info, sizeof(info), "目标超出攻击范围！距离: %d, 范围: %d",
                                                distance, attack_range);
                                        create_text_texture(renderer, info);
                                        found = 1;
                                        break;
                                    }
                                } else {
                                    snprintf(info, sizeof(info), "请选择有效的攻击目标（敌人）");
                                    create_text_texture(renderer, info);
                                    found = 1;
                                    break;
                                }
                            }

                            if (path_start_row == -1) {
                                /* set start */
                                /* only allow start if the clicked cell contains the player */
                                if (!player || player->x != row || player->y != col) {
                                    snprintf(info, sizeof(info), "Start must be player cell");
                                    create_text_texture(renderer, info);
                                    found = 1; break;
                                }

                                // 点击玩家角色时显示菜单
                                if (player && player->x == row && player->y == col) {
                                    show_player_menu = 1;
                                    menu_selected_option = 0;
                                    // 设置菜单位置在玩家角色附近
                                    int cx, cy;
                                    hex_center(row, col, current_radius, &cx, &cy);
                                    menu_x = cx + current_radius;
                                    menu_y = cy;
                                    // 确保菜单不会超出窗口边界
                                    if (menu_x + menu_width > g_main_width) {
                                        menu_x = cx - menu_width - current_radius;
                                    }
                                    if (menu_y + menu_height > g_window_height) {
                                        menu_y = g_window_height - menu_height - 10;
                                    }
                                    snprintf(info, sizeof(info), "玩家菜单已打开");
                                    found = 1; break;
                                }

                                path_start_row = row; path_start_col = col;
                                /* mark selection for highlighting */
                                selected_row = row; selected_col = col;
                                /* clear any previous path */
                                if (in_path) memset(in_path, 0, g_map_rows * g_map_cols);
                                path_preview_row = path_preview_col = -1;
                                snprintf(info, sizeof(info), "Start: (%d,%d) Terrain: %s", row, col, names[t]);
                            } else if (path_start_row == row && path_start_col == col) {
                                /* clicked start again -> clear start */
                                path_start_row = path_start_col = -1;
                                if (in_path) memset(in_path, 0, g_map_rows * g_map_cols);
                                path_preview_row = path_preview_col = -1;
                                snprintf(info, sizeof(info), "Start cleared (%d,%d)", row, col);
                                /* clear selection highlight when clearing start */
                                selected_row = selected_col = -1;
                            } else if (path_end_row == -1) {
                                /* set end and compute path */
                                /* disallow choosing an end that is occupied by player or enemy */
                                if (player && player->x == row && player->y == col) {
                                    snprintf(info, sizeof(info), "End cannot be player's cell");
                                    create_text_texture(renderer, info);
                                    found = 1; break;
                                }
                                if (enemy && enemy->x == row && enemy->y == col) {
                                    snprintf(info, sizeof(info), "End cannot be enemy's cell");
                                    create_text_texture(renderer, info);
                                    found = 1; break;
                                }
                                path_end_row = row; path_end_col = col;
                                /* mark selection */
                                selected_row = row; selected_col = col;
                                snprintf(info, sizeof(info), "End: (%d,%d) Terrain: %s", row, col, names[t]);
                                compute_path(path_start_row, path_start_col, path_end_row, path_end_col);
                                /* end selected: disable hover preview (keep this computed path as final) */
                                path_preview_row = path_preview_col = -1;
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
                                /* if a path was found, begin moving the player along it */
                                if (path_len > 1) {
                                                /* start movement from index 1 (0 is current player cell) */
                                                moving = 1;
                                                move_index = 1;
                                                last_move_tick = SDL_GetTicks();
                                                last_anim_tick = last_move_tick;
                                                player_run_frame = 0;
                                                /* initialize interpolation from current cell to first target */
                                                move_progress = 0.0f;
                                                if (path_nodes) {
                                                    int idx0 = path_nodes[0];
                                                    int idx1 = path_nodes[1];
                                                    move_from_r = idx0 / g_map_cols; move_from_c = idx0 % g_map_cols;
                                                    move_to_r = idx1 / g_map_cols; move_to_c = idx1 % g_map_cols;
                                                }
                                            } else if (path_len == 1) {
                                                /* trivial path: already at destination */
                                                create_text_texture(renderer, "Path is current cell");
                                            }
                            } else {
                                /* both set: start a new start */
                                path_start_row = row; path_start_col = col;
                                path_end_row = path_end_col = -1;
                                if (in_path) memset(in_path, 0, g_map_rows * g_map_cols);
                                path_preview_row = path_preview_col = -1;
                                /* clear previous end highlight and highlight the new start */
                                selected_row = row; selected_col = col;
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
                        path_preview_row = path_preview_col = -1;
                        SDL_SetWindowTitle(window, "Hex Terrain Map");
                        if (g_info_tex) { SDL_DestroyTexture(g_info_tex); g_info_tex = NULL; }
                    }

                    // 检查是否点击了菜单
                    if (show_player_menu) {
                        char info[256];
                        int option_height = menu_height / MENU_OPTION_COUNT;
                        for (int i = 0; i < MENU_OPTION_COUNT; i++) {
                            SDL_Rect option_rect = {menu_x, menu_y + i * option_height, menu_width, option_height};
                            if (mx >= option_rect.x && mx <= option_rect.x + option_rect.w &&
                                my >= option_rect.y && my <= option_rect.y + option_rect.h) {
                                // 处理菜单选项点击
                                switch (i) {
                                    case MENU_MOVE:
                                        // 移动模式：设置路径起点
                                        path_start_row = player->x;
                                        path_start_col = player->y;
                                        selected_row = player->x;
                                        selected_col = player->y;
                                        snprintf(info, sizeof(info), "Move Mode: Select Target Position");
                                        break;
                                    case MENU_ATTACK:
                                        // 攻击模式：显示可攻击范围
                                        snprintf(info, sizeof(info), "Attack Mode: Select Target");
                                        attack_mode = get_attack_mode(player->job);
                                        snprintf(info, sizeof(info), "Attack Mode: Select Target (%s)",
                                            attack_mode == ATTACK_MODE_PHYSICAL ? "Physical" : "Magic");
                                    break;
                                        break;
                                    case MENU_ITEM:
                                        // 道具模式：显示道具列表
                                        snprintf(info, sizeof(info), "Item Mode: Select Item to Use");
                                        break;
                                    case MENU_EQUIP:
                                        // 装备模式：显示装备界面
                                        snprintf(info, sizeof(info), "Equipment Mode: Manage Equipment");
                                        break;
                                    case MENU_WAIT:
                                        // 待机：结束当前回合
                                        snprintf(info, sizeof(info), "Wait Mode: End Current Turn");
                                        break;
                                }
                                show_player_menu = 0; // 关闭菜单
                                create_text_texture(renderer, info);
                                found = 1;
                                break;
                            }
                        }
                    }

                    if (!found && selected_row >= 0 && selected_col >= 0) {
                        if (player && player->x == selected_row && player->y == selected_col)
                            show_sprite_info(renderer, player);
                        else if (enemy && enemy->x == selected_row && enemy->y == selected_col)
                            show_sprite_info(renderer, enemy);
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

                        // 首先检查鼠标是否在菜单上
                        if (show_player_menu) {
                            int option_height = menu_height / MENU_OPTION_COUNT;
                            for (int i = 0; i < MENU_OPTION_COUNT; i++) {
                                SDL_Rect option_rect = {menu_x, menu_y + i * option_height, menu_width, option_height};
                                if (mx >= option_rect.x && mx <= option_rect.x + option_rect.w &&
                                    my >= option_rect.y && my <= option_rect.y + option_rect.h) {
                                    // 鼠标在菜单上，更新选中项
                                    menu_selected_option = i;
                                    found = 1;
                                    break;
                                }
                            }
                        }

                        // 如果不在菜单上，检查地图单元格
                        if (!found) {
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
                        }
                    } else {
                        hover_row = hover_col = -1;
                    }
                    /* Path preview: only when a start is set AND end is NOT set, and cursor is over a cell */
                    if (path_start_row != -1 && path_end_row == -1 && hover_row >= 0 && hover_col >= 0) {
                        if (hover_row != path_preview_row || hover_col != path_preview_col) {
                            path_preview_row = hover_row; path_preview_col = hover_col;
                            if (in_path) memset(in_path, 0, g_map_rows * g_map_cols);
                            compute_path(path_start_row, path_start_col, hover_row, hover_col);
                        }
                    } else {
                        /* clear preview if cursor moved off cells or no start/end state for preview */
                        if (path_preview_row != -1 || path_preview_col != -1) {
                            path_preview_row = path_preview_col = -1;
                            if (path_len > 0) {
                                int total = g_map_rows * g_map_cols;
                                if (path_nodes) { free(path_nodes); path_nodes = NULL; }
                                path_len = 0;
                                if (in_path) memset(in_path, 0, total);
                            }
                        }
                    }
                }
            }
            else if (event.type == SDL_KEYDOWN) {
                if (show_player_menu) {
                    // 菜单导航
                    switch (event.key.keysym.sym) {
                        case SDLK_UP:
                            menu_selected_option = (menu_selected_option - 1 + MENU_OPTION_COUNT) % MENU_OPTION_COUNT;
                            break;
                        case SDLK_DOWN:
                            menu_selected_option = (menu_selected_option + 1) % MENU_OPTION_COUNT;
                            break;
                        case SDLK_RETURN:
                        case SDLK_SPACE:
                            // 执行选中的菜单项
                            char info[256];
                            switch (menu_selected_option) {
                                case MENU_MOVE:
                                    path_start_row = player->x;
                                    path_start_col = player->y;
                                    selected_row = player->x;
                                    selected_col = player->y;
                                    snprintf(info, sizeof(info), "移动模式：选择目标位置");
                                    break;
                                case MENU_ATTACK:
                                    snprintf(info, sizeof(info), "攻击模式：选择攻击目标");
                                    break;
                                case MENU_ITEM:
                                    snprintf(info, sizeof(info), "道具模式：选择要使用的道具");
                                    break;
                                case MENU_EQUIP:
                                    snprintf(info, sizeof(info), "装备模式：管理装备");
                                    break;
                                case MENU_WAIT:
                                    snprintf(info, sizeof(info), "待机：结束当前行动");
                                    break;
                            }
                            show_player_menu = 0;
                            create_text_texture(renderer, info);
                            break;
                        case SDLK_ESCAPE:
                            show_player_menu = 0; // 按ESC关闭菜单
                            break;
                    }
                } else {
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

                // 攻击模式下高亮显示攻击范围内的敌人
                if (attack_mode != ATTACK_MODE_NONE && player) {
                    int distance = hex_distance_cells(player->x, player->y, row, col);
                    if (enemy && enemy->x == row && enemy->y == col && distance <= attack_range) {
                        SDL_Point pts[6];
                        compute_hex_points(cx, cy, current_radius - 1, pts);
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer, 255, 50, 50, 120);
                        fill_polygon(renderer, pts, 6);
                        // 红色边框表示可攻击目标
                        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 200);
                        SDL_RenderDrawLines(renderer, pts, 6);
                        SDL_RenderDrawLine(renderer, pts[5].x, pts[5].y, pts[0].x, pts[0].y);
                    }
                }
            }
        }

        /* Advance movement along path with interpolation and update run animation frame */
        {
            Uint32 now = SDL_GetTicks();
            if (moving && path_len > 1 && path_nodes && move_index < path_len) {
                if (move_ms <= 0) move_ms = 200;
                Uint32 dt = now - last_move_tick;
                last_move_tick = now;
                move_progress += (float)dt / (float)move_ms;

                /* complete one or more steps if progress overflowed (supports large dt) */
                while (move_progress >= 1.0f && moving) {
                    /* snap to target cell */
                    if (player && move_to_r >= 0 && move_to_c >= 0) sprite_set_position(player, move_to_r, move_to_c);
                    move_progress -= 1.0f;
                    move_index++;
                    if (move_index < path_len) {
                        /* advance from/to */
                        move_from_r = move_to_r; move_from_c = move_to_c;
                        int idx = path_nodes[move_index];
                        move_to_r = idx / g_map_cols; move_to_c = idx % g_map_cols;
                    } else {
                        /* finished path */
                        moving = 0;
                        /* clear path data */
                        int total = g_map_rows * g_map_cols;
                        if (path_nodes) { free(path_nodes); path_nodes = NULL; }
                        path_len = 0;
                        if (in_path) memset(in_path, 0, total);
                        path_start_row = path_start_col = path_end_row = path_end_col = -1;
                        move_from_r = move_from_c = move_to_r = move_to_c = -1;
                        move_progress = 0.0f;
                        create_text_texture(renderer, "Movement complete");
                        break;
                    }
                }

                /* update animation frame */
                if (player_run_frames > 0 && now - last_anim_tick >= (Uint32)anim_frame_ms) {
                    player_run_frame = (player_run_frame + 1) % player_run_frames;
                    last_anim_tick = now;
                }
            } else {
                player_run_frame = 0;
                move_progress = 0.0f;
            }
        }

        /* Draw sprites (player, enemy) if atlas loaded */
        if (atlas_tex) {
            if (player) {
                /* compute rendered pixel center: interpolated when moving, otherwise snap to player's cell */
                int render_x, render_y;
                if (moving && move_from_r >= 0 && move_to_r >= 0) {
                    int fx, fy, tx, ty;
                    hex_center(move_from_r, move_from_c, current_radius, &fx, &fy);
                    hex_center(move_to_r, move_to_c, current_radius, &tx, &ty);
                    float t = move_progress;
                    if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
                    render_x = (int)(fx + (tx - fx) * t + 0.5f);
                    render_y = (int)(fy + (ty - fy) * t + 0.5f);
                } else {
                    int pr = player->x, pc = player->y;
                    hex_center(pr, pc, current_radius, &render_x, &render_y);
                }
                /* choose source rect: running frames when moving, otherwise idle */
                SDL_Rect cur_src = player_src;
                if (moving && player_run_frames > 0 && player_run_w > 0) {
                    cur_src.x = player_run_x + player_run_frame * player_run_w;
                    cur_src.y = player_run_y;
                    cur_src.w = player_run_w;
                    cur_src.h = player_run_h;
                }
                double hex_w = current_radius * 2.0;
                double hex_h = current_radius * sqrt(3.0);
                const double pad = 0.9; /* keep some padding inside the hex */
                double scale_w = (hex_w * pad) / (double)cur_src.w;
                double scale_h = (hex_h * pad) / (double)cur_src.h;
                double scale = scale_w < scale_h ? scale_w : scale_h;
                if (scale <= 0.0) scale = 1.0;
                int dw = (int)(cur_src.w * scale + 0.5);
                int dh = (int)(cur_src.h * scale + 0.5);
                SDL_Rect dst = { render_x - dw/2, render_y - dh/2, dw, dh };
                SDL_RenderCopy(renderer, atlas_tex, &cur_src, &dst);
            }
            if (enemy) {
                int er = enemy->x, ec = enemy->y;
                int ex, ey; hex_center(er, ec, current_radius, &ex, &ey);
                double hex_w = current_radius * 2.0;
                double hex_h = current_radius * sqrt(3.0);
                const double pad = 0.9;
                double scale_w = (hex_w * pad) / (double)enemy_src.w;
                double scale_h = (hex_h * pad) / (double)enemy_src.h;
                double scale = scale_w < scale_h ? scale_w : scale_h;
                if (scale <= 0.0) scale = 1.0;
                int dw = (int)(enemy_src.w * scale + 0.5);
                int dh = (int)(enemy_src.h * scale + 0.5);
                SDL_Rect dst = { ex - dw/2, ey - dh/2, dw, dh };
                SDL_RenderCopy(renderer, atlas_tex, &enemy_src, &dst);
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

        // 渲染玩家菜单（在信息面板之前渲染）
        render_player_menu(renderer);

        // 绘制信息纹理（信息面板，右侧）
        if (g_info_tex) {
            int info_x = g_main_width + 10;
            int panel_width = get_info_panel_width();

            // 确保文本宽度不超过面板宽度
            int display_width = g_info_w;
            if (display_width > panel_width) {
                display_width = panel_width;
            }

            // 确保文本高度不会超出窗口高度
            int display_height = g_info_h;
            int max_height = g_window_height - 20; // 留出上下边距
            if (display_height > max_height) {
                display_height = max_height;
            }

            SDL_Rect bg = { info_x, 10, panel_width + 10, display_height + 8 };
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
            SDL_RenderFillRect(renderer, &bg);
            // 绘制边框
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 200);
            SDL_RenderDrawRect(renderer, &bg);

            SDL_Rect dst = { info_x + 5, 14, display_width, display_height };
            SDL_RenderCopy(renderer, g_info_tex, NULL, &dst);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    /* cleanup atlas texture and SDL_image */
    if (atlas_tex) SDL_DestroyTexture(atlas_tex);
    IMG_Quit();
    SDL_DestroyRenderer(renderer);
    if (g_info_tex) SDL_DestroyTexture(g_info_tex);
    if (g_font) TTF_CloseFont(g_font);
    /* destroy demo sprites if present (created earlier in main) */
    if (player) sprite_destroy(player);
    if (enemy) sprite_destroy(enemy);
    path_cleanup();
    if (g_terrain_map) { free(g_terrain_map); g_terrain_map = NULL; }
    config_free();
    TTF_Quit();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
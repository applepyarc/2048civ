/* game_state.h */
#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "sprite.h"
#include "path.h"
#include "config.h"
#include "unit_manager.h"
#include "turn_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PHASE_FREE_ROAM,
    PHASE_PATH_START,
    PHASE_PATH_MOVING,
    PHASE_MENU,
} GamePhase;

#define MENU_ITEM_COUNT 5
typedef enum {
    MENU_MOVE=0, MENU_ATTACK=1, MENU_SKILL=2, MENU_ITEM=3, MENU_WAIT=4,
} MenuItemId;

typedef struct {
    int visible, anchor_x, anchor_y, item_w, item_h, hovered, unit_row, unit_col;
    int disabled[MENU_ITEM_COUNT];
} ActionMenu;

typedef struct {
    int x, y, dragging, drag_start_x, drag_start_y, cam_start_x, cam_start_y;
    int map_min_x, map_max_x, map_min_y, map_max_y;
} CameraState;

typedef struct {
    int start_row, start_col, end_row, end_col, preview_row, preview_col;
} PathSelState;

typedef struct {
    int moving, move_index, move_ms, run_frame, anim_frame_ms;
    int from_r, from_c, to_r, to_c, unit_idx;
    Uint32 last_move_tick, last_anim_tick;
    float progress;
} MoveState;

typedef struct {
    SDL_Texture *tex;
    SDL_Rect idle_src, run_src, role_src[4];
    int run_frames, run_w, run_h;
} AtlasInfo;

/* ---- Enemy AI sub-state ---- */
typedef enum {
    AI_STATE_IDLE    = 0,
    AI_STATE_PICK,
    AI_STATE_MOVING,
    AI_STATE_ATTACK_PAUSE,
    AI_STATE_INTER_PAUSE,   /* brief gap between two enemy units  ← BUG2 FIX */
    AI_STATE_DONE,
} EnemyAISubState;

typedef struct {
    EnemyAISubState sub_state;
    int             current_enemy_idx;
    int             attack_target_idx;
    Uint32          pause_until_tick;
} EnemyAIState;

typedef enum {
    RLAYER_TERRAIN=1<<0, RLAYER_PATH=1<<1, RLAYER_SELECTION=1<<2,
    RLAYER_NEIGHBORS=1<<3, RLAYER_PATH_LINE=1<<4, RLAYER_UNITS=1<<5,
    RLAYER_UI=1<<6, RLAYER_ALL=0xFF,
} RenderLayerFlags;

typedef struct {
    SDL_Window *window; SDL_Renderer *renderer; TTF_Font *font;
    int window_width, window_height, main_width;
    int map_rows, map_cols; Terrain *terrain_map;
    int current_radius;
    CameraState camera;
    int selected_row, selected_col, hover_row, hover_col;
    PathSelState path_sel;
    UnitManager units; AtlasInfo atlas; MoveState move; TurnManager turn;
    EnemyAIState ai;
    SDL_Texture *info_tex; int info_w, info_h;
    SDL_Texture **coord_textures; int coord_tex_w, coord_tex_h;
    int highlight_neighbors_enabled, show_cell_coords_enabled;
    RenderLayerFlags render_layers;
    ActionMenu menu;
    int running;
    GamePhase phase;
} GameState;

#define GS_TERRAIN(gs,r,c) ((gs)->terrain_map[(r)*(gs)->map_cols+(c)])

#ifdef __cplusplus
}
#endif
#endif

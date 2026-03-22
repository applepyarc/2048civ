/* event_handler.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "event_handler.h"
#include "hex_utils.h"
#include "render.h"
#include "combat.h"
#include "enemy_ai.h"

#define MIN_RADIUS            8
#define MAX_RADIUS          120
#define CLICK_DRAG_THRESHOLD  5

static void handle_map_click(GameState *gs, int mx, int my);
static void handle_mouse_motion(GameState *gs, int mx, int my);
static void handle_zoom(GameState *gs, int wheel_y, int mx, int my);
static void handle_keydown(GameState *gs, int sym);

void eh_compute_map_bounds(GameState *gs) {
    int rad = gs->current_radius;
    CameraState *cam = &gs->camera;
    int first = 1;
    for (int row = 0; row < gs->map_rows; row++) {
        for (int col = 0; col < gs->map_cols; col++) {
            int cx, cy;
            hex_center(row, col, rad, 0, 0, &cx, &cy);
            SDL_Point pts[6];
            hex_compute_points(cx, cy, rad, pts);
            for (int i = 0; i < 6; i++) {
                if (first) {
                    cam->map_min_x = cam->map_max_x = pts[i].x;
                    cam->map_min_y = cam->map_max_y = pts[i].y;
                    first = 0;
                } else {
                    if (pts[i].x < cam->map_min_x) cam->map_min_x = pts[i].x;
                    if (pts[i].x > cam->map_max_x) cam->map_max_x = pts[i].x;
                    if (pts[i].y < cam->map_min_y) cam->map_min_y = pts[i].y;
                    if (pts[i].y > cam->map_max_y) cam->map_max_y = pts[i].y;
                }
            }
        }
    }
}

void eh_clamp_camera(GameState *gs) {
    CameraState *cam = &gs->camera;
    int min_x = gs->main_width - cam->map_max_x;
    int max_x = -cam->map_min_x;
    if (min_x > max_x) cam->x = (min_x + max_x) / 2;
    else { if (cam->x < min_x) cam->x = min_x; if (cam->x > max_x) cam->x = max_x; }
    int min_y = gs->window_height - cam->map_max_y;
    int max_y = -cam->map_min_y;
    if (min_y > max_y) cam->y = (min_y + max_y) / 2;
    else { if (cam->y < min_y) cam->y = min_y; if (cam->y > max_y) cam->y = max_y; }
}

static void find_cell_at(GameState *gs, int mx, int my, int *out_r, int *out_c) {
    *out_r = *out_c = -1;
    for (int row = 0; row < gs->map_rows && *out_r < 0; row++) {
        for (int col = 0; col < gs->map_cols; col++) {
            int cx, cy;
            hex_center(row, col, gs->current_radius,
                       gs->camera.x, gs->camera.y, &cx, &cy);
            SDL_Point pts[6];
            hex_compute_points(cx, cy, gs->current_radius - 1, pts);
            if (point_in_polygon(pts, 6, mx, my)) {
                *out_r = row; *out_c = col; return;
            }
        }
    }
}

static void gs_clear_path(GameState *gs) {
    int total = gs->map_rows * gs->map_cols;
    if (path_nodes) { free(path_nodes); path_nodes = NULL; }
    path_len = 0;
    if (in_path) memset(in_path, 0, total);
    gs->path_sel.preview_row = gs->path_sel.preview_col = -1;
}

static void gs_compute_path(GameState *gs, int sr, int sc, int tr, int tc) {
    compute_path(sr, sc, tr, tc,
                 gs->map_rows, gs->map_cols,
                 gs->terrain_map,
                 get_neighbors);
}

static void menu_open(GameState *gs, int row, int col) {
    ActionMenu *m = &gs->menu;
    m->item_w = 100;
    m->item_h = 32;
    int cx, cy;
    hex_center(row, col, gs->current_radius,
               gs->camera.x, gs->camera.y, &cx, &cy);
    int mx = cx + gs->current_radius + 6;
    int my = cy - (MENU_ITEM_COUNT * m->item_h) / 2;
    if (mx + m->item_w > gs->main_width)
        mx = cx - gs->current_radius - m->item_w - 6;
    if (my < 4) my = 4;
    if (my + MENU_ITEM_COUNT * m->item_h > gs->window_height - 4)
        my = gs->window_height - 4 - MENU_ITEM_COUNT * m->item_h;
    m->anchor_x = mx;
    m->anchor_y = my;
    m->hovered  = -1;
    m->unit_row = row;
    m->unit_col = col;
    m->visible  = 1;
    int moved    = gs->turn.player_moved;
    int attacked = gs->turn.player_attacked;
    m->disabled[MENU_MOVE]   = moved;
    m->disabled[MENU_ATTACK] = attacked;
    m->disabled[MENU_SKILL]  = attacked;
    m->disabled[MENU_ITEM]   = attacked;
    m->disabled[MENU_WAIT]   = 0;
    gs->phase = PHASE_MENU;
}

static void menu_close(GameState *gs) {
    gs->menu.visible = 0;
    gs->menu.hovered = -1;
    if (gs->phase == PHASE_MENU)
        gs->phase = PHASE_FREE_ROAM;
}

static int menu_hit_test(const ActionMenu *m, int mx, int my) {
    if (!m->visible) return -1;
    if (mx < m->anchor_x || mx >= m->anchor_x + m->item_w) return -1;
    int rel = my - m->anchor_y;
    if (rel < 0 || rel >= MENU_ITEM_COUNT * m->item_h) return -1;
    int idx = rel / m->item_h;
    if (m->disabled[idx]) return -1;
    return idx;
}

static void menu_execute(GameState *gs, int item_idx) {
    int row = gs->menu.unit_row;
    int col = gs->menu.unit_col;
    menu_close(gs);
    switch ((MenuItemId)item_idx) {
    case MENU_MOVE:
        gs->path_sel.start_row = row;
        gs->path_sel.start_col = col;
        gs->selected_row = row;
        gs->selected_col = col;
        if (in_path) memset(in_path, 0, gs->map_rows * gs->map_cols);
        gs->path_sel.preview_row = gs->path_sel.preview_col = -1;
        gs->phase = PHASE_PATH_START;
        render_set_info_text(gs, "Move: select destination");
        break;
    case MENU_ATTACK:
        gs->selected_row = row;
        gs->selected_col = col;
        gs->phase = PHASE_PATH_START;
        render_set_info_text(gs, "Attack: select a target");
        break;
    case MENU_SKILL:
        gs->turn.player_attacked = 1;
        render_set_info_text(gs, "Skill: (not yet implemented)");
        gs->phase = PHASE_FREE_ROAM;
        break;
    case MENU_ITEM:
        gs->turn.player_attacked = 1;
        render_set_info_text(gs, "Item: (not yet implemented)");
        gs->phase = PHASE_FREE_ROAM;
        break;
    case MENU_WAIT:
        gs->turn.player_moved    = 1;
        gs->turn.player_attacked = 1;
        render_set_info_text(gs, "Wait: turn ended — press Enter to next turn");
        gs->phase = PHASE_FREE_ROAM;
        break;
    }
}

static void handle_map_click(GameState *gs, int mx, int my) {
    if (gs->phase == PHASE_MENU) {
        int idx = menu_hit_test(&gs->menu, mx, my);
        if (idx >= 0) menu_execute(gs, idx);
        else           menu_close(gs);
        return;
    }

    int row, col;
    find_cell_at(gs, mx, my, &row, &col);

    if (row < 0) {
        gs->selected_row = gs->selected_col = -1;
        gs->path_sel.start_row = gs->path_sel.start_col = -1;
        gs->path_sel.end_row   = gs->path_sel.end_col   = -1;
        if (in_path) memset(in_path, 0, gs->map_rows * gs->map_cols);
        gs->path_sel.preview_row = gs->path_sel.preview_col = -1;
        SDL_SetWindowTitle(gs->window, "2048civ");
        if (gs->info_tex) { SDL_DestroyTexture(gs->info_tex); gs->info_tex = NULL; }
        gs->phase = PHASE_FREE_ROAM;
        return;
    }

    static const char *terrain_names[TERRAIN_COUNT] = {
        "Plains", "Hills", "Forest", "Desert", "Water", "Mountain"
    };
    Terrain t    = GS_TERRAIN(gs, row, col);
    const char *tname = (t < TERRAIN_COUNT) ? terrain_names[t] : "?";
    char info[512];
    PathSelState *ps = &gs->path_sel;

    if (gs->phase == PHASE_PATH_MOVING) {
        gs->move.moving   = 0;
        gs->move.progress = 0.0f;
        if (gs->move.to_r >= 0 && gs->move.to_c >= 0) {
            Unit *mu = (gs->move.unit_idx >= 0 && gs->move.unit_idx < gs->units.count)
                       ? &gs->units.units[gs->move.unit_idx] : NULL;
            if (mu && mu->sprite)
                sprite_set_position(mu->sprite, gs->move.to_r, gs->move.to_c);
        }
        gs->move.from_r = gs->move.from_c = gs->move.to_r = gs->move.to_c = -1;
        gs_clear_path(gs);
        gs->path_sel.start_row = gs->path_sel.start_col = -1;
        gs->path_sel.end_row   = gs->path_sel.end_col   = -1;
        gs->phase = PHASE_FREE_ROAM;
        render_set_info_text(gs, "Movement cancelled");
        return;
    }

    Unit *player_unit  = um_player(&gs->units);
    Unit *clicked_unit = um_at(&gs->units, row, col);

    if (player_unit && player_unit->sprite &&
            player_unit->sprite->x == row && player_unit->sprite->y == col) {
        gs->path_sel.start_row = gs->path_sel.start_col = -1;
        gs->path_sel.end_row   = gs->path_sel.end_col   = -1;
        if (in_path) memset(in_path, 0, gs->map_rows * gs->map_cols);
        gs->path_sel.preview_row = gs->path_sel.preview_col = -1;
        gs->selected_row = row;
        gs->selected_col = col;
        render_show_sprite_info_ex(gs, player_unit->sprite,
                                   player_unit->role, player_unit->faction);
        if (gs->turn.player_moved && gs->turn.player_attacked) {
            render_set_info_text(gs, "All actions used — press Enter to end turn");
            return;
        }
        menu_open(gs, row, col);
        return;
    }

    if (gs->phase == PHASE_PATH_START && clicked_unit &&
            clicked_unit->faction == FACTION_ENEMY) {
        if (player_unit && player_unit->sprite) {
            int pr = player_unit->sprite->x, pc = player_unit->sprite->y;
            gs_compute_path(gs, pr, pc, row, col);
            int dist = path_len - 1;
            gs_clear_path(gs);
            if (combat_in_range(dist)) {
                int wpow = combat_weapon_power(player_unit->sprite);
                CombatResult cr = combat_attack(player_unit->sprite,
                                                clicked_unit->sprite, wpow);
                snprintf(info, sizeof(info), "%s", cr.log);
                render_set_info_text(gs, info);
                if (!combat_is_alive(clicked_unit->sprite)) {
                    int idx = um_idx_at(&gs->units, row, col);
                    if (idx >= 0) um_remove(&gs->units, idx);
                }
                gs->turn.player_attacked = 1;
                gs->selected_row = gs->selected_col = -1;
                ps->start_row = ps->start_col = -1;
                gs->phase = PHASE_FREE_ROAM;
                return;
            } else {
                snprintf(info, sizeof(info), "Enemy out of range (dist=%d)", dist);
                render_set_info_text(gs, info);
                return;
            }
        }
    }

    if (ps->start_row == -1) {
        if (clicked_unit && clicked_unit->sprite) {
            render_show_sprite_info_ex(gs, clicked_unit->sprite,
                                       clicked_unit->role, clicked_unit->faction);
        } else {
            render_set_info_text(gs, "Select your unit as start");
        }
        return;
    } else if (ps->start_row == row && ps->start_col == col) {
        ps->start_row = ps->start_col = -1;
        if (in_path) memset(in_path, 0, gs->map_rows * gs->map_cols);
        ps->preview_row = ps->preview_col = -1;
        gs->selected_row = gs->selected_col = -1;
        gs->phase = PHASE_FREE_ROAM;
        render_set_info_text(gs, "Selection cancelled");
        return;
    } else if (ps->end_row == -1) {
        Unit *dest_unit = um_at(&gs->units, row, col);
        if (dest_unit && dest_unit->faction == FACTION_PLAYER) {
            render_set_info_text(gs, "Cannot move to friendly unit's cell");
            return;
        }
        if (dest_unit && dest_unit->faction == FACTION_ENEMY) {
            render_set_info_text(gs, "Cannot move onto enemy cell");
            return;
        }
        ps->end_row = row; ps->end_col = col;
        gs->selected_row = row; gs->selected_col = col;
        snprintf(info, sizeof(info), "End: (%d,%d) %s", row, col, tname);
        gs_compute_path(gs, ps->start_row, ps->start_col, row, col);
        ps->preview_row = ps->preview_col = -1;
        if (path_len > 0) {
            char pbuf[256];
            int pos = snprintf(pbuf, sizeof(pbuf), "  Path: %d steps", path_len - 1);
            for (int pi = 0; pi < path_len && pos < (int)sizeof(pbuf) - 40; ++pi) {
                int pr2 = path_nodes[pi] / gs->map_cols;
                int pc2 = path_nodes[pi] % gs->map_cols;
                pos += snprintf(pbuf + pos, sizeof(pbuf) - pos, " (%d,%d)", pr2, pc2);
            }
            strncat(info, pbuf, sizeof(info) - strlen(info) - 1);
        } else {
            strncat(info, "  No path found", sizeof(info) - strlen(info) - 1);
        }
        if (path_len > 1) {
            MoveState *mv = &gs->move;
            mv->moving         = 1;
            mv->move_index     = 1;
            mv->last_move_tick = SDL_GetTicks();
            mv->last_anim_tick = mv->last_move_tick;
            mv->run_frame      = 0;
            mv->progress       = 0.0f;
            mv->from_r = path_nodes[0] / gs->map_cols;
            mv->from_c = path_nodes[0] % gs->map_cols;
            mv->to_r   = path_nodes[1] / gs->map_cols;
            mv->to_c   = path_nodes[1] % gs->map_cols;
            mv->unit_idx = -1;
            for (int i = 0; i < gs->units.count; i++) {
                if (gs->units.units[i].alive &&
                        gs->units.units[i].role == UNIT_ROLE_PLAYER) {
                    mv->unit_idx = i;
                    break;
                }
            }
            gs->phase = PHASE_PATH_MOVING;
        } else if (path_len == 1) {
            render_set_info_text(gs, "Already at destination");
            gs->phase = PHASE_FREE_ROAM;
        }
    } else {
        ps->start_row = row; ps->start_col = col;
        ps->end_row   = ps->end_col = -1;
        if (in_path) memset(in_path, 0, gs->map_rows * gs->map_cols);
        ps->preview_row = ps->preview_col = -1;
        gs->selected_row = row; gs->selected_col = col;
        snprintf(info, sizeof(info), "Start: (%d,%d) %s", row, col, tname);
        gs->phase = PHASE_PATH_START;
    }

    render_set_info_text(gs, info);
    if (gs->selected_row >= 0 && gs->selected_col >= 0) {
        Unit *u = um_at(&gs->units, gs->selected_row, gs->selected_col);
        if (u && u->sprite)
            render_show_sprite_info_ex(gs, u->sprite, u->role, u->faction);
    }
    if (clicked_unit && clicked_unit->sprite && gs->selected_row < 0) {
        render_show_sprite_info_ex(gs, clicked_unit->sprite,
                                   clicked_unit->role, clicked_unit->faction);
    }
}

static void handle_mouse_motion(GameState *gs, int mx, int my) {
    if (gs->menu.visible) {
        ActionMenu *m = &gs->menu;
        int hov = -1;
        if (mx >= m->anchor_x && mx < m->anchor_x + m->item_w) {
            int rel = my - m->anchor_y;
            if (rel >= 0 && rel < MENU_ITEM_COUNT * m->item_h) {
                int idx = rel / m->item_h;
                if (!m->disabled[idx]) hov = idx;
            }
        }
        m->hovered = hov;
        return;
    }
    CameraState *cam = &gs->camera;
    if (cam->dragging) {
        cam->x = cam->cam_start_x + (mx - cam->drag_start_x);
        cam->y = cam->cam_start_y + (my - cam->drag_start_y);
        eh_clamp_camera(gs);
        gs->hover_row = gs->hover_col = -1;
        return;
    }
    if (mx >= gs->main_width) { gs->hover_row = gs->hover_col = -1; return; }
    find_cell_at(gs, mx, my, &gs->hover_row, &gs->hover_col);
    PathSelState *ps = &gs->path_sel;
    if (ps->start_row != -1 && ps->end_row == -1
            && gs->hover_row >= 0 && gs->hover_col >= 0) {
        if (gs->hover_row != ps->preview_row || gs->hover_col != ps->preview_col) {
            ps->preview_row = gs->hover_row;
            ps->preview_col = gs->hover_col;
            if (in_path) memset(in_path, 0, gs->map_rows * gs->map_cols);
            gs_compute_path(gs, ps->start_row, ps->start_col,
                            gs->hover_row, gs->hover_col);
        }
    } else if (ps->preview_row != -1 || ps->preview_col != -1) {
        ps->preview_row = ps->preview_col = -1;
        if (path_len > 0) {
            if (path_nodes) { free(path_nodes); path_nodes = NULL; }
            path_len = 0;
            if (in_path) memset(in_path, 0, gs->map_rows * gs->map_cols);
        }
    }
}

static void handle_zoom(GameState *gs, int wheel_y, int mx, int my) {
    if (mx >= gs->main_width) return;
    int old_r = gs->current_radius;
    float factor = (wheel_y > 0) ? 1.1f : 0.9f;
    int new_r = (int)(old_r * factor + 0.5f);
    if (new_r < MIN_RADIUS) new_r = MIN_RADIUS;
    if (new_r > MAX_RADIUS) new_r = MAX_RADIUS;
    if (new_r == old_r) return;
    float scale = (float)new_r / (float)old_r;
    gs->camera.x = mx - (int)((mx - gs->camera.x) * scale);
    gs->camera.y = my - (int)((my - gs->camera.y) * scale);
    gs->current_radius = new_r;
    eh_compute_map_bounds(gs);
    eh_clamp_camera(gs);
}

static void handle_keydown(GameState *gs, int sym) {
    char buf[128];
    switch (sym) {
    case SDLK_n:
        gs->highlight_neighbors_enabled ^= 1;
        snprintf(buf, sizeof(buf), "Highlight neighbors: %s",
                 gs->highlight_neighbors_enabled ? "ON" : "OFF");
        render_set_info_text(gs, buf);
        break;
    case SDLK_c:
        gs->show_cell_coords_enabled ^= 1;
        snprintf(buf, sizeof(buf), "Show cell coords: %s",
                 gs->show_cell_coords_enabled ? "ON" : "OFF");
        render_set_info_text(gs, buf);
        break;
    case SDLK_ESCAPE:
        if (gs->menu.visible) {
            menu_close(gs);
        } else if (gs->phase != PHASE_FREE_ROAM && !gs->move.moving) {
            gs->path_sel.start_row = gs->path_sel.start_col = -1;
            gs->path_sel.end_row   = gs->path_sel.end_col   = -1;
            gs_clear_path(gs);
            gs->selected_row = gs->selected_col = -1;
            gs->phase = PHASE_FREE_ROAM;
            render_set_info_text(gs, "Selection cancelled");
        }
        break;
    case SDLK_RETURN:
        if (gs->phase == PHASE_FREE_ROAM || gs->phase == PHASE_PATH_START) {
            TurnPhase new_phase = tm_end_faction_turn(&gs->turn, &gs->units);
            render_set_info_text(gs, tm_phase_name(gs->turn.phase));
            if (new_phase == TURN_PHASE_ENEMY_AI) {
                enemy_ai_begin(gs);
            }
        }
        break;
    }
}

void eh_process_events(GameState *gs) {
    SDL_Event event;
    CameraState *cam = &gs->camera;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            gs->running = 0;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (gs->menu.visible &&
                        menu_hit_test(&gs->menu, event.button.x, event.button.y) < 0) {
                    menu_close(gs);
                    break;
                }
                cam->dragging     = 1;
                cam->drag_start_x = event.button.x;
                cam->drag_start_y = event.button.y;
                cam->cam_start_x  = cam->x;
                cam->cam_start_y  = cam->y;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x, my = event.button.y;
                int dx = mx - cam->drag_start_x; if (dx < 0) dx = -dx;
                int dy = my - cam->drag_start_y; if (dy < 0) dy = -dy;
                if (dx <= CLICK_DRAG_THRESHOLD && dy <= CLICK_DRAG_THRESHOLD
                        && mx < gs->main_width) {
                    handle_map_click(gs, mx, my);
                }
                cam->dragging = 0;
            }
            break;
        case SDL_MOUSEWHEEL: {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            handle_zoom(gs, event.wheel.y, mx, my);
            break;
        }
        case SDL_MOUSEMOTION:
            handle_mouse_motion(gs, event.motion.x, event.motion.y);
            break;
        case SDL_KEYDOWN:
            handle_keydown(gs, event.key.keysym.sym);
            break;
        }
    }
}

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "unit_manager.h"

void um_init(UnitManager *um, int capacity) {
    if (capacity < 4) capacity = 4;
    um->units = calloc(capacity, sizeof(Unit));
    um->count = 0; um->capacity = um->units ? capacity : 0;
}

void um_free(UnitManager *um) {
    if (!um) return;
    for (int i = 0; i < um->count; i++) { if (um->units[i].sprite) { sprite_destroy(um->units[i].sprite); um->units[i].sprite = NULL; } }
    free(um->units); um->units = NULL; um->count = 0; um->capacity = 0;
}

Unit *um_add(UnitManager *um, const char *name, const char *job, int level, UnitRole role, Faction faction, int row, int col) {
    if (!um) return NULL;
    if (um->count >= um->capacity) {
        int nc = um->capacity * 2 + 4;
        Unit *nb = realloc(um->units, sizeof(Unit) * nc);
        if (!nb) { fprintf(stderr, "um_add: realloc failed\n"); return NULL; }
        um->units = nb; um->capacity = nc;
    }
    Sprite *s = sprite_create(name, job, NULL, level);
    if (!s) return NULL;
    sprite_set_position(s, row, col);
    Unit *u = &um->units[um->count++];
    memset(u, 0, sizeof(*u));
    u->sprite = s;
    u->role = role;
    u->faction = faction;
    u->alive = 1;
    u->atlas_idle_idx = -1;
    u->atlas_run_idx = -1;
    return u;
}

void um_remove(UnitManager *um, int idx) {
    if (!um || idx < 0 || idx >= um->count) return;
    Unit *u = &um->units[idx];
    if (u->sprite) { sprite_destroy(u->sprite); u->sprite = NULL; }
    u->alive = 0;
    if (idx != um->count - 1) { um->units[idx] = um->units[um->count - 1]; memset(&um->units[um->count - 1], 0, sizeof(Unit)); }
    um->count--;
}

Unit *um_at(UnitManager *um, int row, int col) {
    if (!um) return NULL;
    for (int i = 0; i < um->count; i++) { Unit *u = &um->units[i]; if (u->alive && u->sprite && u->sprite->x == row && u->sprite->y == col) return u; }
    return NULL;
}

int um_idx_at(UnitManager *um, int row, int col) {
    if (!um) return -1;
    for (int i = 0; i < um->count; i++) { Unit *u = &um->units[i]; if (u->alive && u->sprite && u->sprite->x == row && u->sprite->y == col) return i; }
    return -1;
}

void um_foreach(UnitManager *um, UnitCallback cb, void *user_data) {
    if (!um || !cb) return;
    for (int i = 0; i < um->count; i++) { if (um->units[i].alive) cb(&um->units[i], user_data); }
}

void um_reset_acted(UnitManager *um, Faction faction) {
    if (!um) return;
    for (int i = 0; i < um->count; i++) { if (um->units[i].faction == faction) um->units[i].acted = 0; }
}

Unit *um_player(UnitManager *um) {
    if (!um) return NULL;
    for (int i = 0; i < um->count; i++) { if (um->units[i].alive && um->units[i].role == UNIT_ROLE_PLAYER) return &um->units[i]; }
    return NULL;
}

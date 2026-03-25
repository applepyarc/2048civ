#ifndef UNIT_MANAGER_H
#define UNIT_MANAGER_H
#include "sprite.h"
#include "path.h"

typedef enum { UNIT_ROLE_PLAYER, UNIT_ROLE_ENEMY, UNIT_ROLE_ALLY, UNIT_ROLE_NEUTRAL } UnitRole;
typedef enum { FACTION_PLAYER=0, FACTION_ENEMY=1, FACTION_COUNT } Faction;

typedef struct {
    Sprite *sprite;
    UnitRole role;
    Faction faction;
    int        atlas_idle_idx;  /* index into AtlasInfo.sprites[] for idle anim */
    int        atlas_run_idx;   /* index into AtlasInfo.sprites[] for run anim */
    int        acted;           /* 1 = has already acted this turn */
    int        alive;           /* 0 = dead / removed */
 } Unit;

typedef struct { Unit *units; int count, capacity; } UnitManager;

void  um_init(UnitManager *um, int capacity);
void  um_free(UnitManager *um);
Unit *um_add(UnitManager *um, const char *name, const char *job, int level, UnitRole role, Faction faction, int row, int col);
void  um_remove(UnitManager *um, int idx);
Unit *um_at(UnitManager *um, int row, int col);
int   um_idx_at(UnitManager *um, int row, int col);
typedef void (*UnitCallback)(Unit *u, void *user_data);
void  um_foreach(UnitManager *um, UnitCallback cb, void *user_data);
void  um_reset_acted(UnitManager *um, Faction faction);
Unit *um_player(UnitManager *um);
#endif

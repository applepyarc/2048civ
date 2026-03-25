/* =====================================================================================
 *       Filename: roster.h
 *    Description: Unit roster — loads character definitions from res/roster.cfg.
 * =====================================================================================*/
#ifndef ROSTER_H
#define ROSTER_H

#include "unit_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ROSTER_MAX_CHARS   32
#define ROSTER_NAME_LEN    64
#define ROSTER_SPRITE_LEN  128

typedef struct {
    char     name[ROSTER_NAME_LEN];
    char     job[ROSTER_NAME_LEN];
    Faction  faction;
    char     idle_sprite[ROSTER_SPRITE_LEN];
    char     run_sprite[ROSTER_SPRITE_LEN];
    int      run_frames;
    int hp_base,  hp_rand;
    int atk_base, atk_rand;
    int def_base, def_rand;
    int move;
} RosterChar;

typedef struct {
    RosterChar  all[ROSTER_MAX_CHARS];
    int         count;
    RosterChar *player_chars[ROSTER_MAX_CHARS];
    int         player_count;
    RosterChar *enemy_chars[ROSTER_MAX_CHARS];
    int         enemy_count;
} Roster;

int         roster_load(Roster *r, const char *path);
void        roster_free(Roster *r);
RosterChar *roster_random_player(Roster *r);
int         roster_random_enemies(Roster *r, RosterChar **out, int count);

#ifdef __cplusplus
}
#endif
#endif /* ROSTER_H */
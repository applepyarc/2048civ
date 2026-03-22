#ifndef TURN_MANAGER_H
#define TURN_MANAGER_H
#include "unit_manager.h"

typedef enum {
    TURN_PHASE_PLAYER_ACTION, TURN_PHASE_PLAYER_MOVE, TURN_PHASE_PLAYER_ATTACK,
    TURN_PHASE_ENEMY_AI, TURN_PHASE_TURN_END, TURN_PHASE_VICTORY, TURN_PHASE_DEFEAT
} TurnPhase;

typedef enum { ACTION_NONE=0,ACTION_MOVE,ACTION_ATTACK,ACTION_SKILL,ACTION_ITEM,ACTION_WAIT } ActionType;

typedef struct { ActionType type; int unit_idx,target_row,target_col,target_unit_idx,skill_idx; const char *item_id; } PendingAction;
typedef struct { TurnPhase phase; int turn_number; Faction active_faction; int active_unit_idx; PendingAction pending; int player_moved,player_attacked; } TurnManager;

void       tm_init(TurnManager *tm);
void       tm_begin_turn(TurnManager *tm, UnitManager *um, Faction faction);
int        tm_queue_action(TurnManager *tm, PendingAction action);
int        tm_can_move(const TurnManager *tm);
int        tm_can_attack(const TurnManager *tm);
TurnPhase  tm_end_faction_turn(TurnManager *tm, UnitManager *um);
int        tm_check_end_conditions(TurnManager *tm, UnitManager *um);
const char *tm_phase_name(TurnPhase phase);
#endif

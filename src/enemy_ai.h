/* =====================================================================================
 *       Filename: enemy_ai.h
 *    Description: Enemy AI subsystem.
 *
 *  IMPORTANT: This header must be included AFTER game_state.h in every .c file,
 *  because it uses the GameState typedef defined there.
 *
 *  Algorithm (see enemy_ai.c for full detail):
 *    Each enemy unit: BFS flood-fill reachable cells → score by proximity to player
 *    (+bonus for attack position) → move to best cell → attack if adjacent.
 *  EnemyAIState (stored in GameState.ai) drives a per-enemy state machine:
 *    IDLE → PICK → MOVING → ATTACK_PAUSE → DONE → next enemy → … → end turn
 * =====================================================================================*/
#ifndef ENEMY_AI_H
#define ENEMY_AI_H

#ifdef __cplusplus
extern "C" {
#endif

/* GameState typedef is provided by game_state.h — include that first. */

/*
 * Call once when TURN_PHASE_ENEMY_AI begins.
 * Initialises the AI state machine and finds the first active enemy.
 */
void enemy_ai_begin(GameState *gs);

/*
 * Call every game tick while TURN_PHASE_ENEMY_AI is active.
 * Drives movement animations, attacks, and advances to next enemy.
 * Calls tm_end_faction_turn() automatically when all enemies have acted.
 */
void enemy_ai_update(GameState *gs);

#ifdef __cplusplus
}
#endif
#endif /* ENEMY_AI_H */

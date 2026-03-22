#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H
#include "game_state.h"

void eh_process_events(GameState *gs);
void eh_compute_map_bounds(GameState *gs);
void eh_clamp_camera(GameState *gs);
#endif

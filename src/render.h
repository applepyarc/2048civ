#ifndef RENDER_H
#define RENDER_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "game_state.h"

#ifdef __cplusplus
extern "C" {
#endif

void render_prerender_coord_textures(GameState *gs);
void render_free_coord_textures(GameState *gs);
int  render_set_info_lines(GameState *gs, const char **lines, int nlines);
int  render_set_info_text(GameState *gs, const char *text);
void render_show_sprite_info(GameState *gs, Sprite *s);
void render_show_sprite_info_ex(GameState *gs, Sprite *s, UnitRole role, Faction faction);
void render_hex_terrain(SDL_Renderer *renderer, int cx, int cy, int radius, Terrain t);
void game_render(GameState *gs);
void render_action_menu(GameState *gs);

#ifdef __cplusplus
}
#endif
#endif

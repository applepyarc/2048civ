#ifndef TITLE_SCREEN_H
#define TITLE_SCREEN_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TITLE_RESULT_NONE     = 0,
    TITLE_RESULT_START,       /* player chose START  */
    TITLE_RESULT_CONTINUE,    /* player chose CONTINUE */
    TITLE_RESULT_OPTION,      /* player chose OPTION */
    TITLE_RESULT_CREDITS,     /* player chose 制作组 */
    TITLE_RESULT_QUIT,        /* window closed / Escape */
} TitleResult;

/*
 * Run the title screen event loop.
 * Blocks until the player makes a selection or closes the window.
 */
TitleResult title_screen_run(SDL_Renderer *renderer, TTF_Font *font,
                              int win_w, int win_h);

#ifdef __cplusplus
}
#endif
#endif /* TITLE_SCREEN_H */
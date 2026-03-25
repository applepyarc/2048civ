#include <string.h>
#include <stdio.h>
#include "title_screen.h"

/* ---- Layout ---- */
#define LOGO_COLOR_R    80
#define LOGO_COLOR_G   200
#define LOGO_COLOR_B   120
#define MENU_NORMAL_R  220
#define MENU_NORMAL_G  220
#define MENU_NORMAL_B  220
#define MENU_HOVER_R   255
#define MENU_HOVER_G   220
#define MENU_HOVER_B    60
#define MENU_SPACING    56
#define LOGO_MARGIN_TOP  60
#define MENU_TOP_OFFSET 320

/* ---- ASCII art logo ---- */
static const char *LOGO_LINES[] = {
    " ____   ___  _  _    ___       _",
    "|___ \\ / _ \\| || |  ( _ )  ___(_)_   __",
    "  __) | | | | || |_ / _ \\ / __| \\ \\ / /",
    " / __/| |_| |__   _| (_) | (__| |\\ V /",
    "|_____|\\___/   |_|  \\___/ \\___|_| \\_/",
};
#define LOGO_LINES_COUNT 5

/* ---- Menu items ---- */
typedef struct { const char *label; TitleResult result; } MenuItem;
static const MenuItem MENU_ITEMS[] = {
    { "START",    TITLE_RESULT_START    },
    { "CONTINUE", TITLE_RESULT_CONTINUE },
    { "OPTION",   TITLE_RESULT_OPTION   },
    { "CREDITS",  TITLE_RESULT_CREDITS  },
};
#define MENU_ITEM_COUNT 4

/* ---- Render helpers ---- */
static int render_text_centred(SDL_Renderer *rdr, TTF_Font *font,
                                const char *text, SDL_Color color, int cx, int y) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return 0;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(rdr, surf);
    if (!tex) { SDL_FreeSurface(surf); return 0; }
    int w=surf->w, h=surf->h; SDL_FreeSurface(surf);
    SDL_Rect dst={cx-w/2, y-h/2, w, h};
    SDL_RenderCopy(rdr, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    return w;
}
static int render_text_left(SDL_Renderer *rdr, TTF_Font *font,
                             const char *text, SDL_Color color, int x, int y) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return 0;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(rdr, surf);
    if (!tex) { SDL_FreeSurface(surf); return 0; }
    int h=surf->h;
    SDL_Rect dst={x, y, surf->w, h}; SDL_FreeSurface(surf);
    SDL_RenderCopy(rdr, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    return h;
}

/* ---- Credits sub-screen ---- */
static void show_credits(SDL_Renderer *rdr, TTF_Font *font, int win_w, int win_h) {
    static const char *credits[] = {
        "2048civ",
        "",
        "Game Design & Programming",
        "  2048civ Development Team",
        "",
        "Art Assets",
        "  Dungeon Tileset II by 0x72",
        "  (opengameart.org)",
        "",
        "Built with SDL2 + SDL_ttf + Claude",
        "",
        "Press any key to return",
    };
    int nlines=(int)(sizeof(credits)/sizeof(credits[0]));
    SDL_Color white={220,220,220,255}, accent={80,200,120,255}, dim={140,140,140,255};
    int running=1; SDL_Event ev;
    while (running) {
        while (SDL_PollEvent(&ev))
            if (ev.type==SDL_QUIT||ev.type==SDL_KEYDOWN||ev.type==SDL_MOUSEBUTTONDOWN)
                running=0;
        SDL_SetRenderDrawColor(rdr,15,15,25,255); SDL_RenderClear(rdr);
        int line_h=win_h/(nlines+2); if(line_h>40)line_h=40;
        int y=(win_h-nlines*line_h)/2;
        for(int i=0;i<nlines;i++){
            SDL_Color col=(i==0)?accent:(credits[i][0]==' '?dim:white);
            if(credits[i][0]) render_text_centred(rdr,font,credits[i],col,win_w/2,y);
            y+=line_h;
        }
        SDL_RenderPresent(rdr); SDL_Delay(16);
    }
}

/* ---- Main title loop ---- */
TitleResult title_screen_run(SDL_Renderer *renderer, TTF_Font *font,
                              int win_w, int win_h) {
    int hovered=0;
    TitleResult result=TITLE_RESULT_QUIT;
    SDL_Color logo_color  ={LOGO_COLOR_R,  LOGO_COLOR_G,  LOGO_COLOR_B,  255};
    SDL_Color normal_color={MENU_NORMAL_R, MENU_NORMAL_G, MENU_NORMAL_B, 255};
    SDL_Color hover_color ={MENU_HOVER_R,  MENU_HOVER_G,  MENU_HOVER_B,  255};
    SDL_Color dim_color   ={100,100,100,255};
    SDL_Color hint_color  ={80,80,80,255};

    int menu_y[MENU_ITEM_COUNT];
    for(int i=0;i<MENU_ITEM_COUNT;i++) menu_y[i]=MENU_TOP_OFFSET+i*MENU_SPACING;
    int font_h=TTF_FontHeight(font);

    int running=1; SDL_Event ev;
    while (running) {
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                running=0; result=TITLE_RESULT_QUIT; break;
            case SDL_KEYDOWN:
                switch(ev.key.keysym.sym){
                case SDLK_ESCAPE: running=0; result=TITLE_RESULT_QUIT; break;
                case SDLK_UP:   hovered=(hovered-1+MENU_ITEM_COUNT)%MENU_ITEM_COUNT; break;
                case SDLK_DOWN: hovered=(hovered+1)%MENU_ITEM_COUNT; break;
                case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE:
                    result=MENU_ITEMS[hovered].result;
                    if(result==TITLE_RESULT_CREDITS){show_credits(renderer,font,win_w,win_h);result=TITLE_RESULT_NONE;}
                    else running=0;
                    break;
                }
                break;
            case SDL_MOUSEMOTION:{
                int mx=ev.motion.x, my=ev.motion.y;
                for(int i=0;i<MENU_ITEM_COUNT;i++){
                    if(my>=menu_y[i]-font_h/2-4&&my<=menu_y[i]+font_h/2+4&&
                       mx>=win_w/4&&mx<=win_w*3/4){hovered=i;break;}
                }break;}
            case SDL_MOUSEBUTTONDOWN:
                if(ev.button.button==SDL_BUTTON_LEFT){
                    int mx=ev.button.x, my=ev.button.y;
                    for(int i=0;i<MENU_ITEM_COUNT;i++){
                        if(my>=menu_y[i]-font_h/2-4&&my<=menu_y[i]+font_h/2+4&&
                           mx>=win_w/4&&mx<=win_w*3/4){
                            result=MENU_ITEMS[i].result;
                            if(result==TITLE_RESULT_CREDITS){show_credits(renderer,font,win_w,win_h);result=TITLE_RESULT_NONE;}
                            else running=0;
                            break;
                        }
                    }
                }break;
            }
        }

        /* ---- Draw ---- */
        SDL_SetRenderDrawColor(renderer,15,15,25,255);
        SDL_RenderClear(renderer);

        /* Logo */
        {
            int lh=TTF_FontHeight(font);
            int max_w=0;
            for(int i=0;i<LOGO_LINES_COUNT;i++){
                int w=0; TTF_SizeUTF8(font,LOGO_LINES[i],&w,NULL);
                if(w>max_w)max_w=w;
            }
            int logo_x=(win_w-max_w)/2; if(logo_x<8)logo_x=8;
            int y=LOGO_MARGIN_TOP;
            for(int i=0;i<LOGO_LINES_COUNT;i++){
                render_text_left(renderer,font,LOGO_LINES[i],logo_color,logo_x,y);
                y+=lh+4;
            }
        }

        /* Separator */
        SDL_SetRenderDrawColor(renderer,60,80,60,255);
        SDL_RenderDrawLine(renderer,win_w/6,MENU_TOP_OFFSET-MENU_SPACING/2-8,
                                    win_w*5/6,MENU_TOP_OFFSET-MENU_SPACING/2-8);

        /* Menu items */
        for(int i=0;i<MENU_ITEM_COUNT;i++){
            SDL_Color col;
            if(i==hovered){
                col=hover_color;
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer,255,220,60,30);
                SDL_Rect bar={win_w/4, menu_y[i]-font_h/2-6, win_w/2, font_h+12};
                SDL_RenderFillRect(renderer,&bar);
                SDL_SetRenderDrawColor(renderer,MENU_HOVER_R,MENU_HOVER_G,MENU_HOVER_B,200);
                SDL_RenderDrawLine(renderer,win_w/4-16,menu_y[i],win_w/4-4,menu_y[i]);
                SDL_RenderDrawLine(renderer,win_w*3/4+4,menu_y[i],win_w*3/4+16,menu_y[i]);
            } else {
                col=(i==1||i==2)?dim_color:normal_color;
            }
            render_text_centred(renderer,font,MENU_ITEMS[i].label,col,win_w/2,menu_y[i]);
        }

        /* Hint */
        render_text_centred(renderer,font,
                            "Arrow keys / mouse to navigate   Enter / click to select",
                            hint_color,win_w/2,win_h-32);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    return result;
}
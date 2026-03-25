/* =====================================================================================
 *       Filename: 2048civ.c
 *    Description:
 *        Created: 2024/03/28 20:12:42
 *         Author: archer
 * =====================================================================================*/
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "game_state.h"
#include "hex_utils.h"
#include "render.h"
#include "event_handler.h"
#include "unit_manager.h"
#include "turn_manager.h"
#include "combat.h"
#include "path.h"
#include "config.h"
#include "perlin.h"
#include "enemy_ai.h"
#include "roster.h"

#define DEFAULT_HEX_RADIUS 40

static void generate_terrain(GameState *gs) {
    unsigned int seed = (unsigned)time(NULL);
    PerlinParams pp;
    config_get_perlin_params(&pp);
    if (pp.seed == 0) pp.seed = seed;
    perlin_init_with_params(&pp);
    for (int r = 0; r < gs->map_rows; r++) {
        for (int c = 0; c < gs->map_cols; c++) {
            float elev = perlin_elevation((float)c, (float)r);
            float mois = perlin_moisture((float)c, (float)r);
            Terrain t;
            if      (elev < 0.35f) t = TERRAIN_WATER;
            else if (elev > 0.85f) t = TERRAIN_MOUNTAIN;
            else if (elev > 0.60f) t = (mois > 0.55f) ? TERRAIN_FOREST : TERRAIN_HILLS;
            else if (mois < 0.28f) t = TERRAIN_DESERT;
            else if (mois > 0.65f) t = TERRAIN_FOREST;
            else                   t = TERRAIN_PLAINS;
            GS_TERRAIN(gs, r, c) = t;
        }
    }
}

static void load_atlas(GameState *gs) {
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "SDL_image PNG unavailable: %s\n", IMG_GetError()); return;
    }
    const char *atlas_img  = config_get_atlas_image_path();
    const char *atlas_desc = config_get_atlas_desc_path();
    SDL_Surface *surf = IMG_Load(atlas_img);
    if (!surf) { fprintf(stderr,"Atlas load failed: %s\n",IMG_GetError()); return; }
    gs->atlas.tex = SDL_CreateTextureFromSurface(gs->renderer, surf);
    SDL_FreeSurface(surf);
    if (!gs->atlas.tex) return;

    gs->atlas.idle_src   = (SDL_Rect){0,0,16,16};
    gs->atlas.run_src    = (SDL_Rect){0,0,16,16};
    gs->atlas.run_frames = 0;
    gs->atlas.run_w = gs->atlas.run_h = 16;
    for (int i=0; i<4; i++) gs->atlas.role_src[i] = (SDL_Rect){0,0,16,16};
    gs->atlas.enemy_run_src    = (SDL_Rect){0,0,16,16};
    gs->atlas.enemy_run_frames = 0;
    gs->atlas.enemy_run_w = gs->atlas.enemy_run_h = 16;
    gs->atlas.sprite_count = 0;

    FILE *af = fopen(atlas_desc, "r");
    if (!af) { fprintf(stderr,"Atlas desc missing, using defaults\n"); return; }
    char name[256];
    while (fscanf(af, "%255s", name) == 1) {
        int x,y,w,h,frames=1;
        if (fscanf(af,"%d %d %d %d",&x,&y,&w,&h) < 4) break;
        fscanf(af,"%d",&frames);
        /* store in full sprite table */
        if (gs->atlas.sprite_count < ATLAS_MAX_SPRITES) {
            AtlasSprite *sp = &gs->atlas.sprites[gs->atlas.sprite_count++];
            strncpy(sp->name, name, ATLAS_SPRITE_NAME_LEN-1);
            sp->src    = (SDL_Rect){x,y,w,h};
            sp->frames = (frames>0) ? frames : 1;
        }
        /* legacy quick-access */
        if (strcmp(name,"knight_f_idle_anim")==0) {
            gs->atlas.idle_src=(SDL_Rect){x,y,w,h};
            gs->atlas.role_src[UNIT_ROLE_PLAYER]=gs->atlas.idle_src;
        } else if (strcmp(name,"knight_f_run_anim")==0) {
            gs->atlas.run_src=(SDL_Rect){x,y,w,h};
            gs->atlas.run_frames=frames>0?frames:1;
            gs->atlas.run_w=w; gs->atlas.run_h=h;
        } else if (strcmp(name,"big_zombie_idle_anim")==0) {
            gs->atlas.role_src[UNIT_ROLE_ENEMY]=(SDL_Rect){x,y,w,h};
        } else if (strcmp(name,"big_zombie_run_anim")==0) {
            gs->atlas.enemy_run_src=(SDL_Rect){x,y,w,h};
            gs->atlas.enemy_run_frames=4;
            gs->atlas.enemy_run_w=w; gs->atlas.enemy_run_h=h;
        }
    }
    fclose(af);
    printf("[atlas] Loaded %d sprites from '%s'\n", gs->atlas.sprite_count, atlas_desc);
}

static int atlas_find_sprite(const AtlasInfo *a, const char *name) {
    for (int i=0; i<a->sprite_count; i++)
        if (strcmp(a->sprites[i].name, name)==0) return i;
    return -1;
}

static void apply_roster_char(GameState *gs, Unit *u, const RosterChar *rc, int level) {
    if (!u || !u->sprite || !rc) return;
    Sprite *s = u->sprite;
    strncpy(s->name, rc->name, sizeof(s->name)-1);
    strncpy(s->job,  rc->job,  sizeof(s->job) -1);
    s->level   = level;
    s->max_hp  = rc->hp_base  + (rc->hp_rand  > 0 ? rand()%rc->hp_rand  : 0);
    s->hp      = s->max_hp;
    s->max_mp  = 20; s->mp = s->max_mp;
    s->attack  = rc->atk_base + (rc->atk_rand > 0 ? rand()%rc->atk_rand : 0);
    s->defense = rc->def_base + (rc->def_rand > 0 ? rand()%rc->def_rand : 0);
    s->speed   = 3 + rand()%5;
    s->move    = rc->move > 0 ? rc->move : 2;
    u->atlas_idle_idx = atlas_find_sprite(&gs->atlas, rc->idle_sprite);
    u->atlas_run_idx  = atlas_find_sprite(&gs->atlas, rc->run_sprite);
    /* update legacy quick-access */
    if (u->role==UNIT_ROLE_PLAYER && u->atlas_idle_idx>=0) {
        gs->atlas.idle_src=gs->atlas.sprites[u->atlas_idle_idx].src;
        gs->atlas.role_src[UNIT_ROLE_PLAYER]=gs->atlas.idle_src;
        if (u->atlas_run_idx>=0) {
            const AtlasSprite *rs=&gs->atlas.sprites[u->atlas_run_idx];
            gs->atlas.run_src=rs->src; gs->atlas.run_frames=rs->frames;
            gs->atlas.run_w=rs->src.w; gs->atlas.run_h=rs->src.h;
        }
    }
    if (u->role==UNIT_ROLE_ENEMY && u->atlas_idle_idx>=0) {
        gs->atlas.role_src[UNIT_ROLE_ENEMY]=gs->atlas.sprites[u->atlas_idle_idx].src;
        if (u->atlas_run_idx>=0) {
            const AtlasSprite *rs=&gs->atlas.sprites[u->atlas_run_idx];
            gs->atlas.enemy_run_src=rs->src; gs->atlas.enemy_run_frames=rs->frames;
            gs->atlas.enemy_run_w=rs->src.w; gs->atlas.enemy_run_h=rs->src.h;
        }
    }
    printf("[roster] Spawned %s (%s) lv%d HP=%d ATK=%d idle=%s(%d) run=%s(%d)\n",
           s->name,s->job,level,s->max_hp,s->attack,
           rc->idle_sprite,u->atlas_idle_idx,rc->run_sprite,u->atlas_run_idx);
}

static void populate_units(GameState *gs) {
    Roster roster;
    int ok = roster_load(&roster, config_get_roster_path());
    if (!ok || roster.count==0) {
        fprintf(stderr,"[roster] Falling back to hardcoded units\n");
        Unit *player=um_add(&gs->units,"Player","Warrior",1,UNIT_ROLE_PLAYER,FACTION_PLAYER,
                            rand()%gs->map_rows,rand()%gs->map_cols);
        if (player && player->sprite) {
            Sprite *p=player->sprite;
            p->level=1+rand()%10; p->max_hp=80+rand()%121; p->hp=p->max_hp;
            p->max_mp=10+rand()%71; p->mp=p->max_mp; p->attack=5+rand()%36;
            p->defense=rand()%31; p->speed=1+rand()%10; p->move=1+rand()%3;
        }
        for (int i=0;i<2;i++){
            char ename[32]; snprintf(ename,sizeof(ename),"Enemy%d",i+1);
            Unit *e=um_add(&gs->units,ename,"Goblin",1+rand()%5,
                           UNIT_ROLE_ENEMY,FACTION_ENEMY,
                           rand()%gs->map_rows,rand()%gs->map_cols);
            if(e&&e->sprite){
                e->sprite->max_hp=40+rand()%81; e->sprite->hp=e->sprite->max_hp;
                e->sprite->attack=4+rand()%20; e->sprite->defense=rand()%15;
                e->sprite->speed=1+rand()%6; e->sprite->move=1+rand()%2;
            }
        }
        roster_free(&roster); return;
    }
    RosterChar *pc = roster_random_player(&roster);
    if (pc) {
        Unit *player=um_add(&gs->units,pc->name,pc->job,1,
                            UNIT_ROLE_PLAYER,FACTION_PLAYER,
                            rand()%gs->map_rows,rand()%gs->map_cols);
        if (player) apply_roster_char(gs,player,pc,1+rand()%5);
    }
    RosterChar *ec[2];
    int ne=roster_random_enemies(&roster,ec,2);
    for (int i=0;i<ne;i++){
        char ename[64]; snprintf(ename,sizeof(ename),"%s%d",ec[i]->name,i+1);
        Unit *enemy=um_add(&gs->units,ename,ec[i]->job,1,
                           UNIT_ROLE_ENEMY,FACTION_ENEMY,
                           rand()%gs->map_rows,rand()%gs->map_cols);
        if (enemy) apply_roster_char(gs,enemy,ec[i],1+rand()%5);
    }
    roster_free(&roster);
}

static int game_init(GameState *gs) {
    memset(gs,0,sizeof(*gs));
    if (SDL_Init(SDL_INIT_VIDEO)!=0){fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return 0;}
    if (TTF_Init()==-1){fprintf(stderr,"TTF_Init: %s\n",TTF_GetError());SDL_Quit();return 0;}
    config_init();
    gs->window_width =config_get_window_width();
    gs->window_height=config_get_window_height();
    float split=config_get_split_ratio();
    if(split<0.05f)split=0.05f; if(split>0.95f)split=0.95f;
    gs->main_width=(int)(gs->window_width*split);
    gs->window=SDL_CreateWindow("2048civ",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                                gs->window_width,gs->window_height,0);
    if(!gs->window){fprintf(stderr,"CreateWindow: %s\n",SDL_GetError());return 0;}
    gs->renderer=SDL_CreateRenderer(gs->window,-1,SDL_RENDERER_ACCELERATED);
    if(!gs->renderer){fprintf(stderr,"CreateRenderer: %s\n",SDL_GetError());return 0;}
    gs->font=TTF_OpenFont(config_get_font_path(),config_get_font_size());
    if(!gs->font)fprintf(stderr,"Font '%s': %s\n",config_get_font_path(),TTF_GetError());
    gs->map_rows=config_get_map_rows(); gs->map_cols=config_get_map_cols();
    gs->terrain_map=malloc(sizeof(Terrain)*gs->map_rows*gs->map_cols);
    if(!gs->terrain_map){fprintf(stderr,"OOM terrain\n");return 0;}
    srand((unsigned)time(NULL));
    generate_terrain(gs);
    gs->current_radius=DEFAULT_HEX_RADIUS;
    eh_compute_map_bounds(gs); eh_clamp_camera(gs);
    render_prerender_coord_textures(gs);
    um_init(&gs->units,16);
    populate_units(gs);
    load_atlas(gs);
    /* NOTE: load_atlas MUST come after populate_units so apply_roster_char
       can look up sprite indices in the freshly populated table. */
    gs->move.move_ms=config_get_move_ms(); gs->move.anim_frame_ms=120;
    gs->move.unit_idx=-1; gs->move.from_r=gs->move.from_c=-1; gs->move.to_r=gs->move.to_c=-1;
    tm_init(&gs->turn); tm_begin_turn(&gs->turn,&gs->units,FACTION_PLAYER);
    gs->highlight_neighbors_enabled=1; gs->show_cell_coords_enabled=1;
    gs->render_layers=RLAYER_ALL;
    gs->path_sel.start_row=gs->path_sel.start_col=-1;
    gs->path_sel.end_row=gs->path_sel.end_col=-1;
    gs->path_sel.preview_row=gs->path_sel.preview_col=-1;
    gs->selected_row=gs->selected_col=-1; gs->hover_row=gs->hover_col=-1;
    gs->phase=PHASE_FREE_ROAM; gs->running=1;
    return 1;
}

static void game_update(GameState *gs) {
    if (gs->turn.phase==TURN_PHASE_ENEMY_AI) {
        MoveState *mv=&gs->move;
        if (mv->moving&&path_len>1&&path_nodes&&mv->move_index<path_len) {
            Uint32 now=SDL_GetTicks();
            if(mv->move_ms<=0)mv->move_ms=200;
            Uint32 dt=now-mv->last_move_tick; mv->last_move_tick=now;
            mv->progress+=(float)dt/(float)mv->move_ms;
            while(mv->progress>=1.0f&&mv->moving){
                Unit *u=(mv->unit_idx>=0&&mv->unit_idx<gs->units.count)?&gs->units.units[mv->unit_idx]:NULL;
                if(u&&u->sprite&&mv->to_r>=0&&mv->to_c>=0) sprite_set_position(u->sprite,mv->to_r,mv->to_c);
                mv->progress-=1.0f; mv->move_index++;
                if(mv->move_index<path_len){
                    mv->from_r=mv->to_r; mv->from_c=mv->to_c;
                    int idx=path_nodes[mv->move_index]; mv->to_r=idx/gs->map_cols; mv->to_c=idx%gs->map_cols;
                } else {
                    mv->moving=0; mv->progress=0.0f; mv->from_r=mv->from_c=mv->to_r=mv->to_c=-1;
                    if(path_nodes){free(path_nodes);path_nodes=NULL;} path_len=0;
                    if(in_path)memset(in_path,0,gs->map_rows*gs->map_cols); break;
                }
            }
            int anim_frames=(mv->unit_idx>=0&&mv->unit_idx<gs->units.count&&
                             gs->units.units[mv->unit_idx].faction==FACTION_ENEMY)
                            ?gs->atlas.enemy_run_frames:gs->atlas.run_frames;
            if(anim_frames>0&&now-mv->last_anim_tick>=(Uint32)mv->anim_frame_ms){
                mv->run_frame=(mv->run_frame+1)%anim_frames; mv->last_anim_tick=now;
            }
        }
        enemy_ai_update(gs); return;
    }
    MoveState *mv=&gs->move;
    if(!mv->moving||path_len<=1||!path_nodes)return;
    if(mv->move_index>=path_len)return;
    Uint32 now=SDL_GetTicks();
    if(mv->move_ms<=0)mv->move_ms=200;
    Uint32 dt=now-mv->last_move_tick; mv->last_move_tick=now;
    mv->progress+=(float)dt/(float)mv->move_ms;
    while(mv->progress>=1.0f&&mv->moving){
        Unit *u=(mv->unit_idx>=0&&mv->unit_idx<gs->units.count)?&gs->units.units[mv->unit_idx]:NULL;
        if(u&&u->sprite&&mv->to_r>=0&&mv->to_c>=0) sprite_set_position(u->sprite,mv->to_r,mv->to_c);
        mv->progress-=1.0f; mv->move_index++;
        if(mv->move_index<path_len){
            mv->from_r=mv->to_r; mv->from_c=mv->to_c;
            int idx=path_nodes[mv->move_index]; mv->to_r=idx/gs->map_cols; mv->to_c=idx%gs->map_cols;
        } else {
            mv->moving=0; mv->progress=0.0f; mv->from_r=mv->from_c=mv->to_r=mv->to_c=-1;
            int total=gs->map_rows*gs->map_cols;
            if(path_nodes){free(path_nodes);path_nodes=NULL;} path_len=0;
            if(in_path)memset(in_path,0,total);
            gs->path_sel.start_row=gs->path_sel.start_col=-1;
            gs->path_sel.end_row=gs->path_sel.end_col=-1;
            gs->phase=PHASE_FREE_ROAM; gs->turn.player_moved=1;
            tm_check_end_conditions(&gs->turn,&gs->units);
            render_set_info_text(gs,"Move complete — press Enter to end turn"); break;
        }
    }
    if(gs->atlas.run_frames>0&&now-mv->last_anim_tick>=(Uint32)mv->anim_frame_ms){
        mv->run_frame=(mv->run_frame+1)%gs->atlas.run_frames; mv->last_anim_tick=now;
    }
}

static void game_cleanup(GameState *gs) {
    render_free_coord_textures(gs);
    if(gs->atlas.tex)SDL_DestroyTexture(gs->atlas.tex);
    if(gs->info_tex) SDL_DestroyTexture(gs->info_tex);
    um_free(&gs->units); path_cleanup();
    if(gs->terrain_map)free(gs->terrain_map);
    if(gs->font)      TTF_CloseFont(gs->font);
    if(gs->renderer)  SDL_DestroyRenderer(gs->renderer);
    if(gs->window)    SDL_DestroyWindow(gs->window);
    IMG_Quit(); config_free(); TTF_Quit(); SDL_Quit();
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    GameState gs;
    if(!game_init(&gs)){fprintf(stderr,"Initialization failed.\n");return 1;}
    while(gs.running){eh_process_events(&gs);game_update(&gs);game_render(&gs);}
    game_cleanup(&gs);
    return 0;
}
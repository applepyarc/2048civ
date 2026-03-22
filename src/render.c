#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "render.h"
#include "unit_manager.h"
#include "hex_utils.h"

#define COORDS_SHOW_MIN_RADIUS 16

void render_prerender_coord_textures(GameState *gs) {
    if (!gs->font) return;
    int n = gs->map_rows * gs->map_cols;
    gs->coord_textures = calloc(n, sizeof(SDL_Texture *));
    if (!gs->coord_textures) return;
    SDL_Color white = {255,255,255,255};
    for (int r = 0; r < gs->map_rows; r++) {
        for (int c = 0; c < gs->map_cols; c++) {
            char buf[32]; snprintf(buf,sizeof(buf),"%d,%d",r,c);
            SDL_Surface *surf = TTF_RenderUTF8_Blended(gs->font,buf,white);
            if (!surf) continue;
            gs->coord_textures[r*gs->map_cols+c] = SDL_CreateTextureFromSurface(gs->renderer,surf);
            gs->coord_tex_w=surf->w; gs->coord_tex_h=surf->h;
            SDL_FreeSurface(surf);
        }
    }
}

void render_free_coord_textures(GameState *gs) {
    if (!gs->coord_textures) return;
    int n = gs->map_rows * gs->map_cols;
    for (int i=0;i<n;i++) if (gs->coord_textures[i]) SDL_DestroyTexture(gs->coord_textures[i]);
    free(gs->coord_textures); gs->coord_textures=NULL;
}

int render_set_info_lines(GameState *gs, const char **lines, int nlines) {
    if (!gs->font || nlines<=0) return 0;
    SDL_Surface **surfs = malloc(sizeof(SDL_Surface*)*nlines);
    if (!surfs) return 0;
    SDL_Color white={255,255,255,255};
    int max_w=0,total_h=0; const int spacing=4;
    for (int i=0;i<nlines;++i) {
        surfs[i]=TTF_RenderUTF8_Blended(gs->font,lines[i]?lines[i]:"",white);
        if (!surfs[i]){for(int j=0;j<i;++j)SDL_FreeSurface(surfs[j]);free(surfs);return 0;}
        if (surfs[i]->w>max_w) max_w=surfs[i]->w;
        total_h+=surfs[i]->h+(i>0?spacing:0);
    }
    SDL_Surface *dest=SDL_CreateRGBSurfaceWithFormat(0,max_w,total_h,32,SDL_PIXELFORMAT_RGBA32);
    if (!dest){for(int i=0;i<nlines;++i)SDL_FreeSurface(surfs[i]);free(surfs);return 0;}
    SDL_FillRect(dest,NULL,SDL_MapRGBA(dest->format,0,0,0,0));
    int y=0;
    for (int i=0;i<nlines;++i){
        SDL_Rect dst={0,y,surfs[i]->w,surfs[i]->h};
        SDL_BlitSurface(surfs[i],NULL,dest,&dst);
        y+=surfs[i]->h+spacing; SDL_FreeSurface(surfs[i]);
    }
    free(surfs);
    if (gs->info_tex){SDL_DestroyTexture(gs->info_tex);gs->info_tex=NULL;}
    gs->info_tex=SDL_CreateTextureFromSurface(gs->renderer,dest);
    gs->info_w=dest->w; gs->info_h=dest->h;
    SDL_FreeSurface(dest);
    return gs->info_tex?1:0;
}

int render_set_info_text(GameState *gs,const char *text){
    const char *lines[1]={text}; return render_set_info_lines(gs,lines,1);
}

static void make_bar(char *buf,size_t bufsz,int cur,int max,int width){
    if(max<=0){snprintf(buf,bufsz,"[----------] %d/%d",cur,max);return;}
    int filled=(int)((float)cur/max*width+0.5f);
    if(filled>width)filled=width; if(filled<0)filled=0;
    int pos=0; buf[pos++]='[';
    for(int i=0;i<width;i++) buf[pos++]=(i<filled)?'#':'-';
    buf[pos++]=']'; buf[pos]='\0';
    char num[32]; snprintf(num,sizeof(num)," %d/%d",cur,max);
    strncat(buf,num,bufsz-strlen(buf)-1);
}

void render_show_sprite_info(GameState *gs,Sprite *s){
    render_show_sprite_info_ex(gs,s,UNIT_ROLE_NEUTRAL,FACTION_PLAYER);
}

void render_show_sprite_info_ex(GameState *gs,Sprite *s,UnitRole role,Faction faction){
    if(!s) return;
    static const char *role_tag[]={"[Player]","[Enemy]","[Ally]","[Neutral]"};
    static const char *faction_tag[]={"Player Faction","Enemy Faction"};
    char l_header[128],l_sep[64],l_job[128],l_level[64],l_faction[64],l_pos[64];
    char hp_bar[64],mp_bar[64],l_hp[96],l_mp[96],l_status[64];
    char l_atk[64],l_def[64],l_spd[64],l_mov[64];
    snprintf(l_header,sizeof(l_header),"%s  %s",s->name?s->name:"???",(role<4)?role_tag[role]:"");
    snprintf(l_sep,sizeof(l_sep),"--------------------");
    snprintf(l_job,sizeof(l_job),"Job    : %s",s->job?s->job:"Unknown");
    snprintf(l_level,sizeof(l_level),"Level  : %d",s->level);
    snprintf(l_faction,sizeof(l_faction),"Side   : %s",(faction<2)?faction_tag[faction]:"?");
    snprintf(l_pos,sizeof(l_pos),"Pos    : (%d, %d)",s->x,s->y);
    make_bar(hp_bar,sizeof(hp_bar),s->hp,s->max_hp,12);
    make_bar(mp_bar,sizeof(mp_bar),s->mp,s->max_mp,12);
    snprintf(l_hp,sizeof(l_hp),"HP  %s",hp_bar);
    snprintf(l_mp,sizeof(l_mp),"MP  %s",mp_bar);
    if(s->hp<=0) snprintf(l_status,sizeof(l_status),"Status : DEAD");
    else if(s->hp<s->max_hp/4) snprintf(l_status,sizeof(l_status),"Status : Critical!");
    else if(s->hp<s->max_hp/2) snprintf(l_status,sizeof(l_status),"Status : Wounded");
    else snprintf(l_status,sizeof(l_status),"Status : OK");
    snprintf(l_atk,sizeof(l_atk),"ATK    : %d",s->attack);
    snprintf(l_def,sizeof(l_def),"DEF    : %d",s->defense);
    snprintf(l_spd,sizeof(l_spd),"SPD    : %d   JMP: %d",s->speed,s->jump);
    snprintf(l_mov,sizeof(l_mov),"MOV    : %d",s->move);
    static const char *slot_name[]={"Weapon","Shield","Helmet","Armor ","Acces."};
    char l_eq[MAX_EQUIP_SLOTS][96];
    for(int i=0;i<MAX_EQUIP_SLOTS;i++){
        const Equipment *eq=&s->equipments[i];
        if(eq->name) snprintf(l_eq[i],sizeof(l_eq[i]),"%s : %s (atk+%d def+%d)",slot_name[i],eq->name,eq->atk,eq->def);
        else snprintf(l_eq[i],sizeof(l_eq[i]),"%s : (none)",slot_name[i]);
    }
    const char *lines[]={l_header,l_sep,l_job,l_level,l_faction,l_pos,l_sep,l_hp,l_mp,l_status,l_sep,l_atk,l_def,l_spd,l_mov,l_sep,l_eq[0],l_eq[1],l_eq[2],l_eq[3],l_eq[4]};
    render_set_info_lines(gs,lines,21);
}

void render_hex_terrain(SDL_Renderer *renderer,int cx,int cy,int radius,Terrain t){
    SDL_Point pts[6]; hex_compute_points(cx,cy,radius,pts);
    Uint8 r,g,b,a; terrain_color(t,&r,&g,&b,&a);
    SDL_SetRenderDrawColor(renderer,r,g,b,a); fill_polygon(renderer,pts,6);
    SDL_SetRenderDrawColor(renderer,r>40?r-40:0,g>40?g-40:0,b>40?b-40:0,a);
    SDL_RenderDrawLines(renderer,pts,6);
    SDL_RenderDrawLine(renderer,pts[5].x,pts[5].y,pts[0].x,pts[0].y);
}

static void render_unit_sprite(GameState *gs,SDL_Rect src,int cx,int cy){
    double hex_w=gs->current_radius*2.0, hex_h=gs->current_radius*sqrt(3.0);
    double scale=(hex_w*0.9/src.w<hex_h*0.9/src.h)?(hex_w*0.9/src.w):(hex_h*0.9/src.h);
    if(scale<=0.0)scale=1.0;
    int dw=(int)(src.w*scale+0.5),dh=(int)(src.h*scale+0.5);
    SDL_Rect dst={cx-dw/2,cy-dh/2,dw,dh};
    SDL_RenderCopy(gs->renderer,gs->atlas.tex,&src,&dst);
}

static void pass_terrain(GameState *gs){
    SDL_Renderer *rdr=gs->renderer; int rad=gs->current_radius;
    for(int row=0;row<gs->map_rows;row++){
        for(int col=0;col<gs->map_cols;col++){
            int cx,cy; hex_center(row,col,rad,gs->camera.x,gs->camera.y,&cx,&cy);
            render_hex_terrain(rdr,cx,cy,rad-1,GS_TERRAIN(gs,row,col));
            if(gs->show_cell_coords_enabled&&rad>=COORDS_SHOW_MIN_RADIUS&&gs->coord_textures){
                SDL_Texture *ttx=gs->coord_textures[row*gs->map_cols+col];
                if(ttx){int tw,th;SDL_QueryTexture(ttx,NULL,NULL,&tw,&th);SDL_SetTextureBlendMode(ttx,SDL_BLENDMODE_BLEND);SDL_Rect td={cx-tw/2,cy-th/2,tw,th};SDL_RenderCopy(rdr,ttx,NULL,&td);}
            }
        }
    }
}
static void pass_path_overlay(GameState *gs){
    if(!in_path||path_len==0)return;
    SDL_Renderer *rdr=gs->renderer; int rad=gs->current_radius;
    SDL_SetRenderDrawBlendMode(rdr,SDL_BLENDMODE_BLEND);
    for(int row=0;row<gs->map_rows;row++)for(int col=0;col<gs->map_cols;col++){
        if(!in_path[row*gs->map_cols+col])continue;
        int cx,cy; hex_center(row,col,rad,gs->camera.x,gs->camera.y,&cx,&cy);
        SDL_Point pts[6]; hex_compute_points(cx,cy,rad-1,pts);
        SDL_SetRenderDrawColor(rdr,0,200,200,140); fill_polygon(rdr,pts,6);
    }
}
static void pass_selection(GameState *gs){
    if(gs->selected_row<0||gs->selected_col<0)return;
    int cx,cy; hex_center(gs->selected_row,gs->selected_col,gs->current_radius,gs->camera.x,gs->camera.y,&cx,&cy);
    SDL_Point pts[6]; hex_compute_points(cx,cy,gs->current_radius-1,pts);
    SDL_SetRenderDrawBlendMode(gs->renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(gs->renderer,255,0,0,200);
    SDL_RenderDrawLines(gs->renderer,pts,6);
    SDL_RenderDrawLine(gs->renderer,pts[5].x,pts[5].y,pts[0].x,pts[0].y);
}
static void pass_neighbors(GameState *gs){
    if(!gs->highlight_neighbors_enabled||gs->hover_row<0)return;
    SDL_Renderer *rdr=gs->renderer; int rad=gs->current_radius;
    int nbr_r[6],nbr_c[6];
    int nc=get_neighbors(gs->hover_row,gs->hover_col,gs->map_rows,gs->map_cols,nbr_r,nbr_c);
    SDL_Point pts[6]; SDL_SetRenderDrawBlendMode(rdr,SDL_BLENDMODE_BLEND);
    for(int i=0;i<nc;++i){
        int cx,cy; hex_center(nbr_r[i],nbr_c[i],rad,gs->camera.x,gs->camera.y,&cx,&cy);
        hex_compute_points(cx,cy,rad-1,pts);
        SDL_SetRenderDrawColor(rdr,255,220,0,120);fill_polygon(rdr,pts,6);
        SDL_SetRenderDrawColor(rdr,255,220,0,200);SDL_RenderDrawLines(rdr,pts,6);
        SDL_RenderDrawLine(rdr,pts[5].x,pts[5].y,pts[0].x,pts[0].y);
    }
    int hcx,hcy; hex_center(gs->hover_row,gs->hover_col,rad,gs->camera.x,gs->camera.y,&hcx,&hcy);
    hex_compute_points(hcx,hcy,rad-1,pts);
    SDL_SetRenderDrawColor(rdr,255,255,255,160);SDL_RenderDrawLines(rdr,pts,6);
    SDL_RenderDrawLine(rdr,pts[5].x,pts[5].y,pts[0].x,pts[0].y);
}
static void pass_path_line(GameState *gs){
    if(path_len<2||!path_nodes)return;
    SDL_Point *pline=malloc(sizeof(SDL_Point)*path_len); if(!pline)return;
    for(int i=0;i<path_len;++i){int pr=path_nodes[i]/gs->map_cols,pc=path_nodes[i]%gs->map_cols;hex_center(pr,pc,gs->current_radius,gs->camera.x,gs->camera.y,&pline[i].x,&pline[i].y);}
    SDL_SetRenderDrawBlendMode(gs->renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(gs->renderer,255,80,80,220);SDL_RenderDrawLines(gs->renderer,pline,path_len);
    SDL_SetRenderDrawColor(gs->renderer,255,255,255,255);
    SDL_Rect rs={pline[0].x-4,pline[0].y-4,8,8},re={pline[path_len-1].x-4,pline[path_len-1].y-4,8,8};
    SDL_RenderFillRect(gs->renderer,&rs);SDL_RenderFillRect(gs->renderer,&re);
    free(pline);
}
static void pass_units(GameState *gs){
    if(!gs->atlas.tex)return;
    MoveState *mv=&gs->move; int rad=gs->current_radius;
    for(int i=0;i<gs->units.count;i++){
        Unit *u=&gs->units.units[i]; if(!u->alive||!u->sprite)continue;
        int cx,cy;
        if(mv->moving&&mv->unit_idx==i&&mv->from_r>=0&&mv->to_r>=0){
            int fx,fy,tx,ty;
            hex_center(mv->from_r,mv->from_c,rad,gs->camera.x,gs->camera.y,&fx,&fy);
            hex_center(mv->to_r,mv->to_c,rad,gs->camera.x,gs->camera.y,&tx,&ty);
            float t=mv->progress; if(t<0.0f)t=0.0f; if(t>1.0f)t=1.0f;
            cx=(int)(fx+(tx-fx)*t+0.5f); cy=(int)(fy+(ty-fy)*t+0.5f);
        } else {
            hex_center(u->sprite->x,u->sprite->y,rad,gs->camera.x,gs->camera.y,&cx,&cy);
        }
        SDL_Rect src;
        if(u->role==UNIT_ROLE_PLAYER){
            if(mv->moving&&mv->unit_idx==i&&gs->atlas.run_frames>0&&gs->atlas.run_w>0){
                src.x=gs->atlas.run_src.x+mv->run_frame*gs->atlas.run_w; src.y=gs->atlas.run_src.y;
                src.w=gs->atlas.run_w; src.h=gs->atlas.run_h;
            } else src=gs->atlas.idle_src;
        } else src=(gs->atlas.role_src[u->role].w>0)?gs->atlas.role_src[u->role]:gs->atlas.idle_src;
        if(!u->sprite->hp) SDL_SetTextureColorMod(gs->atlas.tex,200,60,60);
        else if(u->faction==FACTION_ENEMY) SDL_SetTextureColorMod(gs->atlas.tex,255,180,180);
        else SDL_SetTextureColorMod(gs->atlas.tex,255,255,255);
        render_unit_sprite(gs,src,cx,cy);
    }
    SDL_SetTextureColorMod(gs->atlas.tex,255,255,255);
}
static void pass_ui(GameState *gs){
    if(!gs->info_tex)return;
    int info_x=gs->main_width+10;
    SDL_Rect bg={info_x,10,gs->info_w+10,gs->info_h+8};
    SDL_SetRenderDrawBlendMode(gs->renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(gs->renderer,0,0,0,160);SDL_RenderFillRect(gs->renderer,&bg);
    SDL_Rect dst={info_x+5,14,gs->info_w,gs->info_h};
    SDL_RenderCopy(gs->renderer,gs->info_tex,NULL,&dst);
}

void render_action_menu(GameState *gs){
    ActionMenu *m=&gs->menu;
    if(!m->visible||!gs->font)return;
    static const char *labels[MENU_ITEM_COUNT]={"  移动  ","  攻击  ","  技能  ","  道具  ","  待机  "};
    SDL_Renderer *rdr=gs->renderer;
    SDL_SetRenderDrawBlendMode(rdr,SDL_BLENDMODE_BLEND);
    for(int i=0;i<MENU_ITEM_COUNT;i++){
        SDL_Rect item={m->anchor_x,m->anchor_y+i*m->item_h,m->item_w,m->item_h};
        if(i==m->hovered) SDL_SetRenderDrawColor(rdr,255,220,60,230);
        else SDL_SetRenderDrawColor(rdr,30,30,50,210);
        SDL_RenderFillRect(rdr,&item);
        SDL_SetRenderDrawColor(rdr,160,160,200,255);SDL_RenderDrawRect(rdr,&item);
        SDL_Color col=(i==m->hovered)?(SDL_Color){20,20,20,255}:(SDL_Color){220,220,220,255};
        SDL_Surface *surf=TTF_RenderUTF8_Blended(gs->font,labels[i],col);
        if(surf){
            SDL_Texture *tex=SDL_CreateTextureFromSurface(rdr,surf);
            if(tex){SDL_Rect dst={item.x+(item.w-surf->w)/2,item.y+(item.h-surf->h)/2,surf->w,surf->h};SDL_RenderCopy(rdr,tex,NULL,&dst);SDL_DestroyTexture(tex);}
            SDL_FreeSurface(surf);
        }
    }
}

void game_render(GameState *gs){
    SDL_SetRenderDrawColor(gs->renderer,30,30,30,255);
    SDL_RenderClear(gs->renderer);
    SDL_Rect map_view={0,0,gs->main_width,gs->window_height};
    SDL_RenderSetViewport(gs->renderer,&map_view);
    RenderLayerFlags layers=gs->render_layers;
    if(layers&RLAYER_TERRAIN)   pass_terrain(gs);
    if(layers&RLAYER_PATH)      pass_path_overlay(gs);
    if(layers&RLAYER_SELECTION) pass_selection(gs);
    if(layers&RLAYER_NEIGHBORS) pass_neighbors(gs);
    if(layers&RLAYER_PATH_LINE) pass_path_line(gs);
    if(layers&RLAYER_UNITS)     pass_units(gs);
    SDL_RenderSetViewport(gs->renderer,NULL);
    if(layers&RLAYER_UI) pass_ui(gs);
    render_action_menu(gs);
    SDL_RenderPresent(gs->renderer);
    SDL_Delay(16);
}

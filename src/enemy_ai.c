#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "game_state.h"
#include "enemy_ai.h"
#include "hex_utils.h"
#include "combat.h"
#include "render.h"
#include "path.h"

static void offset_to_cube(int row, int col, int *cx, int *cy, int *cz) {
    *cx = col - (row - (row & 1)) / 2;
    *cz = row;
    *cy = -(*cx) - (*cz);
}

static int hex_distance(int r1, int c1, int r2, int c2) {
    int ax, ay, az, bx, by, bz;
    offset_to_cube(r1, c1, &ax, &ay, &az);
    offset_to_cube(r2, c2, &bx, &by, &bz);
    int dx = ax-bx; if(dx<0)dx=-dx;
    int dy = ay-by; if(dy<0)dy=-dy;
    int dz = az-bz; if(dz<0)dz=-dz;
    return (dx+dy+dz)/2;
}

static int terrain_move_cost(Terrain t) {
    switch(t) {
        case TERRAIN_WATER:    return 99;
        case TERRAIN_MOUNTAIN: return 3;
        case TERRAIN_FOREST:   return 2;
        case TERRAIN_HILLS:    return 2;
        default:               return 1;
    }
}

typedef struct { int row, col, cost; } BFSCell;

static BFSCell *bfs_reachable(GameState *gs, int start_r, int start_c,
                               int move_range, int self_unit_idx, int *out_count) {
    int rows=gs->map_rows, cols=gs->map_cols, total=rows*cols;
    int *dist = malloc(sizeof(int)*total);
    if(!dist){*out_count=0;return NULL;}
    for(int i=0;i<total;i++)dist[i]=-1;
    BFSCell *queue  = malloc(sizeof(BFSCell)*total);
    BFSCell *result = malloc(sizeof(BFSCell)*total);
    if(!queue||!result){free(dist);free(queue);free(result);*out_count=0;return NULL;}
    int qhead=0,qtail=0,rcount=0;
    dist[start_r*cols+start_c]=0;
    queue[qtail++]=(BFSCell){start_r,start_c,0};
    while(qhead<qtail){
        BFSCell cur=queue[qhead++];
        result[rcount++]=cur;
        int nbr_r[6],nbr_c[6];
        int nc=get_neighbors(cur.row,cur.col,rows,cols,nbr_r,nbr_c);
        for(int i=0;i<nc;i++){
            int nr=nbr_r[i],nc2=nbr_c[i],idx=nr*cols+nc2;
            Terrain t=GS_TERRAIN(gs,nr,nc2);
            int cost=cur.cost+terrain_move_cost(t);
            if(terrain_move_cost(t)>=99)continue;
            if(cost>move_range)continue;
            if(dist[idx]>=0)continue;
            int occupied=0;
            for(int u=0;u<gs->units.count;u++){
                if(u==self_unit_idx)continue;
                Unit *uu=&gs->units.units[u];
                if(uu->alive&&uu->sprite&&uu->sprite->x==nr&&uu->sprite->y==nc2){occupied=1;break;}
            }
            if(occupied)continue;
            dist[idx]=cost;
            queue[qtail++]=(BFSCell){nr,nc2,cost};
        }
    }
    free(dist);free(queue);
    *out_count=rcount;
    return result;
}

static int player_adjacent(GameState *gs, int row, int col) {
    Unit *player=um_player(&gs->units);
    if(!player||!player->sprite)return 0;
    int pr=player->sprite->x, pc=player->sprite->y;
    int nbr_r[6],nbr_c[6];
    int nc=get_neighbors(row,col,gs->map_rows,gs->map_cols,nbr_r,nbr_c);
    for(int i=0;i<nc;i++)if(nbr_r[i]==pr&&nbr_c[i]==pc)return 1;
    return 0;
}

static void ai_pick_destination(GameState *gs, int enemy_idx, int *dest_r, int *dest_c) {
    Unit *enemy=&gs->units.units[enemy_idx];
    if(!enemy->alive||!enemy->sprite){*dest_r=-1;*dest_c=-1;return;}
    int er=enemy->sprite->x, ec=enemy->sprite->y;
    int move_range=enemy->sprite->move>0?enemy->sprite->move:1;
    Unit *player=um_player(&gs->units);
    if(!player||!player->sprite){*dest_r=er;*dest_c=ec;return;}
    int pr=player->sprite->x, pc=player->sprite->y;
    int count=0;
    BFSCell *cells=bfs_reachable(gs,er,ec,move_range,enemy_idx,&count);
    if(!cells||count==0){free(cells);*dest_r=er;*dest_c=ec;return;}
    int best_score=INT_MIN, best_r=er, best_c=ec;
    for(int i=0;i<count;i++){
        int cr=cells[i].row, cc=cells[i].col;
        if(cr==pr&&cc==pc)continue;
        int dist=hex_distance(cr,cc,pr,pc);
        int score=-dist;
        if(player_adjacent(gs,cr,cc))score+=1000;
        if(score>best_score){best_score=score;best_r=cr;best_c=cc;}
    }
    free(cells);
    *dest_r=best_r; *dest_c=best_c;
}

static int ai_start_move(GameState *gs, int enemy_idx, int dest_r, int dest_c) {
    Unit *enemy=&gs->units.units[enemy_idx];
    int er=enemy->sprite->x, ec=enemy->sprite->y;
    if(er==dest_r&&ec==dest_c)return 0;
    compute_path(er,ec,dest_r,dest_c,gs->map_rows,gs->map_cols,gs->terrain_map,get_neighbors);
    if(path_len<2){sprite_set_position(enemy->sprite,dest_r,dest_c);return 0;}
    MoveState *mv=&gs->move;
    mv->moving=1; mv->move_index=1;
    mv->last_move_tick=SDL_GetTicks(); mv->last_anim_tick=mv->last_move_tick;
    mv->run_frame=0; mv->progress=0.0f;
    mv->from_r=path_nodes[0]/gs->map_cols; mv->from_c=path_nodes[0]%gs->map_cols;
    mv->to_r=path_nodes[1]/gs->map_cols;   mv->to_c=path_nodes[1]%gs->map_cols;
    mv->unit_idx=enemy_idx;
    gs->phase=PHASE_PATH_MOVING;
    return 1;
}

static int ai_try_attack(GameState *gs, int enemy_idx) {
    Unit *enemy=&gs->units.units[enemy_idx];
    Unit *player=um_player(&gs->units);
    if(!enemy->alive||!enemy->sprite)return 0;
    if(!player||!player->sprite)return 0;
    int dist=hex_distance(enemy->sprite->x,enemy->sprite->y,
                          player->sprite->x,player->sprite->y);
    if(!combat_in_range(dist))return 0;
    CombatResult cr=combat_attack(enemy->sprite,player->sprite,combat_weapon_power(enemy->sprite));
    char msg[320]; snprintf(msg,sizeof(msg),"[Enemy] %s",cr.log);
    render_set_info_text(gs,msg);
    printf("[AI] Enemy%d attacks player: %s\n",enemy_idx,cr.log);
    if(!combat_is_alive(player->sprite)){
        int pidx=-1;
        for(int i=0;i<gs->units.count;i++)
            if(gs->units.units[i].alive&&gs->units.units[i].role==UNIT_ROLE_PLAYER){pidx=i;break;}
        if(pidx>=0)um_remove(&gs->units,pidx);
        tm_check_end_conditions(&gs->turn,&gs->units);
    }
    return 1;
}

static int ai_next_enemy(GameState *gs, int after_idx) {
    for(int i=after_idx+1;i<gs->units.count;i++){
        Unit *u=&gs->units.units[i];
        if(u->alive&&u->faction==FACTION_ENEMY&&u->sprite)return i;
    }
    return -1;
}

static int ai_first_enemy(GameState *gs){return ai_next_enemy(gs,-1);}

void enemy_ai_begin(GameState *gs) {
    EnemyAIState *ai=&gs->ai;
    memset(ai,0,sizeof(*ai));
    ai->attack_target_idx=-1;
    int first=ai_first_enemy(gs);
    if(first<0){
        ai->sub_state=AI_STATE_IDLE;
        tm_end_faction_turn(&gs->turn,&gs->units);
        render_set_info_text(gs,"No enemies remain — Player turn");
        return;
    }
    ai->current_enemy_idx=first;
    ai->sub_state=AI_STATE_PICK;
    printf("[AI] Enemy turn begins. First enemy: %d\n",first);
}

void enemy_ai_update(GameState *gs) {
    if(gs->turn.phase!=TURN_PHASE_ENEMY_AI)return;
    if(gs->ai.sub_state==AI_STATE_IDLE)return;

    EnemyAIState *ai=&gs->ai;
    MoveState *mv=&gs->move;

    switch(ai->sub_state){

    case AI_STATE_PICK: {
        int eidx=ai->current_enemy_idx;
        if(eidx<0||eidx>=gs->units.count||!gs->units.units[eidx].alive){
            ai->sub_state=AI_STATE_DONE; break;
        }
        Unit *enemy=&gs->units.units[eidx];
        printf("[AI] Picking action for enemy %d at (%d,%d)\n",
               eidx,enemy->sprite->x,enemy->sprite->y);
        int dest_r,dest_c;
        ai_pick_destination(gs,eidx,&dest_r,&dest_c);
        ai->attack_target_idx=-1;
        int moving=ai_start_move(gs,eidx,dest_r,dest_c);
        if(moving){
            ai->sub_state=AI_STATE_MOVING;
        } else {
            ai_try_attack(gs,eidx);
            ai->pause_until_tick=SDL_GetTicks()+600;
            ai->sub_state=AI_STATE_ATTACK_PAUSE;
        }
        break;
    }

    case AI_STATE_MOVING:
        /* BUG1 FIX: animation is now driven by game_update() each tick.
         * We just wait here until mv->moving clears. */
        if(!mv->moving){
            ai_try_attack(gs,ai->current_enemy_idx);
            ai->pause_until_tick=SDL_GetTicks()+600;
            ai->sub_state=AI_STATE_ATTACK_PAUSE;
            gs->phase=PHASE_FREE_ROAM;
        }
        break;

    case AI_STATE_ATTACK_PAUSE:
        if(SDL_GetTicks()>=ai->pause_until_tick)
            ai->sub_state=AI_STATE_DONE;
        break;

    /* BUG2 FIX: separate state for inter-unit pause so PICK is never overwritten */
    case AI_STATE_INTER_PAUSE:
        if(SDL_GetTicks()>=ai->pause_until_tick)
            ai->sub_state=AI_STATE_PICK;
        break;

    case AI_STATE_DONE: {
        int next=ai_next_enemy(gs,ai->current_enemy_idx);
        if(next<0){
            printf("[AI] All enemies acted. Returning to player turn.\n");
            ai->sub_state=AI_STATE_IDLE;
            gs->phase=PHASE_FREE_ROAM;
            tm_end_faction_turn(&gs->turn,&gs->units);
            render_set_info_text(gs,"Enemy turn over — your turn");
        } else {
            /* BUG2 FIX: use INTER_PAUSE, never override with a second assignment */
            ai->current_enemy_idx=next;
            ai->attack_target_idx=-1;
            ai->pause_until_tick=SDL_GetTicks()+200;
            ai->sub_state=AI_STATE_INTER_PAUSE;
        }
        break;
    }

    default: break;
    }
}

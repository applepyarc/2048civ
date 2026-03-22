#include <stdio.h>
#include <string.h>
#include "turn_manager.h"

void tm_init(TurnManager *tm){memset(tm,0,sizeof(*tm));tm->phase=TURN_PHASE_PLAYER_ACTION;tm->turn_number=1;tm->active_faction=FACTION_PLAYER;tm->pending.type=ACTION_NONE;}

void tm_begin_turn(TurnManager *tm,UnitManager *um,Faction faction){
    tm->active_faction=faction;tm->player_moved=0;tm->player_attacked=0;um_reset_acted(um,faction);
    if(faction==FACTION_PLAYER){tm->phase=TURN_PHASE_PLAYER_ACTION;printf("[Turn %d] Player\n",tm->turn_number);}
    else{tm->phase=TURN_PHASE_ENEMY_AI;printf("[Turn %d] Enemy\n",tm->turn_number);}
}

int tm_queue_action(TurnManager *tm,PendingAction action){
    if(tm->phase!=TURN_PHASE_PLAYER_ACTION)return 0;
    if(action.type==ACTION_MOVE&&!tm_can_move(tm))return 0;
    if((action.type==ACTION_ATTACK||action.type==ACTION_SKILL)&&!tm_can_attack(tm))return 0;
    tm->pending=action;return 1;
}
int tm_can_move(const TurnManager *tm){return tm->phase==TURN_PHASE_PLAYER_ACTION&&!tm->player_moved;}
int tm_can_attack(const TurnManager *tm){return tm->phase==TURN_PHASE_PLAYER_ACTION&&!tm->player_attacked;}

TurnPhase tm_end_faction_turn(TurnManager *tm,UnitManager *um){
    if(tm->active_faction==FACTION_PLAYER)tm_begin_turn(tm,um,FACTION_ENEMY);
    else{tm->turn_number++;tm_begin_turn(tm,um,FACTION_PLAYER);}
    return tm->phase;
}

int tm_check_end_conditions(TurnManager *tm,UnitManager *um){
    int pa=0,ea=0;
    for(int i=0;i<um->count;i++){Unit *u=&um->units[i];if(!u->alive)continue;if(u->faction==FACTION_PLAYER&&u->sprite&&u->sprite->hp>0)pa++;if(u->faction==FACTION_ENEMY&&u->sprite&&u->sprite->hp>0)ea++;}
    if(!pa){tm->phase=TURN_PHASE_DEFEAT;return 1;}if(!ea){tm->phase=TURN_PHASE_VICTORY;return 1;}return 0;
}

const char *tm_phase_name(TurnPhase p){
    switch(p){case TURN_PHASE_PLAYER_ACTION:return "Player Action";case TURN_PHASE_PLAYER_MOVE:return "Player Moving";case TURN_PHASE_PLAYER_ATTACK:return "Player Attack";case TURN_PHASE_ENEMY_AI:return "Enemy Turn";case TURN_PHASE_TURN_END:return "Turn End";case TURN_PHASE_VICTORY:return "VICTORY";case TURN_PHASE_DEFEAT:return "DEFEAT";default:return "Unknown";}
}

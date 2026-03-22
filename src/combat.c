#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "combat.h"

static int clamp_int(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
static int roll_critical(const Sprite *atk){return (rand()%100)<(atk->speed/2);}

static int phys_damage(const Sprite *atk,const Sprite *def,int wpow,int crit){
    int d=atk->attack*2+wpow-def->defense/2;
    if(crit)d=(int)(d*1.5f); return d<1?1:d;
}
static int magic_damage(const Sprite *atk,const Sprite *def,const SkillDef *sp){
    int d=atk->attack*2+sp->power-def->defense/4; return d<1?1:d;
}
static void apply_hp_delta(Sprite *t,int delta){t->hp=clamp_int(t->hp-delta,0,t->max_hp);}

CombatResult combat_attack(Sprite *atk,Sprite *def,int wpow){
    CombatResult r; memset(&r,0,sizeof(r));
    if(!atk||!def||!combat_is_alive(atk)||!combat_is_alive(def)){r.is_miss=1;snprintf(r.log,sizeof(r.log),"Invalid attack");return r;}
    r.is_critical=roll_critical(atk);
    r.damage=phys_damage(atk,def,wpow,r.is_critical);
    if(r.damage>def->hp)r.damage=def->hp;
    apply_hp_delta(def,r.damage);
    snprintf(r.log,sizeof(r.log),"%s%s attacks %s: %d dmg [HP %d/%d]",r.is_critical?"CRITICAL! ":"",atk->name,def->name,r.damage,def->hp,def->max_hp);
    return r;
}

CombatResult combat_use_skill(Sprite *caster,Sprite *target,const SkillDef *skill){
    CombatResult r; memset(&r,0,sizeof(r));
    if(!caster||!target||!skill){r.is_miss=1;snprintf(r.log,sizeof(r.log),"Invalid skill");return r;}
    if(caster->mp<skill->mp_cost){r.is_miss=1;snprintf(r.log,sizeof(r.log),"Not enough MP");return r;}
    caster->mp-=skill->mp_cost; r.attacker_mp_spent=skill->mp_cost;
    if(skill->is_magic){r.damage=magic_damage(caster,target,skill);}
    else{r.is_critical=roll_critical(caster);r.damage=phys_damage(caster,target,skill->power,r.is_critical);}
    if(r.damage>target->hp)r.damage=target->hp;
    apply_hp_delta(target,r.damage);
    snprintf(r.log,sizeof(r.log),"%s uses %s: %d dmg [HP %d/%d]%s",caster->name,skill->name,r.damage,target->hp,target->max_hp,r.is_critical?" (CRIT)":"");
    return r;
}

int combat_use_item(Sprite *user,Sprite *target,const char *item_id){
    (void)user; if(!target||!item_id)return 0;
    if(!strcmp(item_id,"potion")){if(target->hp>=target->max_hp)return 0;target->hp=clamp_int(target->hp+50,0,target->max_hp);return 1;}
    if(!strcmp(item_id,"hi-potion")){if(target->hp>=target->max_hp)return 0;target->hp=clamp_int(target->hp+150,0,target->max_hp);return 1;}
    if(!strcmp(item_id,"ether")){if(target->mp>=target->max_mp)return 0;target->mp=clamp_int(target->mp+30,0,target->max_mp);return 1;}
    if(!strcmp(item_id,"elixir")){target->hp=target->max_hp;target->mp=target->max_mp;return 1;}
    return 0;
}
int combat_cast_time_ms(const Sprite *c,const SkillDef *s){if(!c||!s)return 0;float r=c->speed*0.005f;if(r>0.8f)r=0.8f;int t=(int)(s->base_cast_ms*(1.0f-r));return t<100?100:t;}
int combat_weapon_power(const Sprite *s){if(!s)return 0;const Equipment *e=&s->equipments[0];return e->name?e->atk:0;}
int combat_is_alive(const Sprite *s){return s&&s->hp>0;}
int combat_in_range(int d){return d<=1;}

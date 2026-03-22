#ifndef COMBAT_H
#define COMBAT_H

#include "sprite.h"
#include "unit_manager.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum { ELEM_NONE=0,ELEM_FIRE,ELEM_ICE,ELEM_WIND,ELEM_EARTH,ELEM_LIGHT,ELEM_DARK } Element;

typedef struct { const char *name; int power,base_cast_ms,is_magic,mp_cost; Element elem; } SkillDef;
typedef struct { int damage,is_critical,is_miss,attacker_mp_spent; char log[256]; } CombatResult;

CombatResult combat_attack(Sprite *attacker, Sprite *defender, int weapon_power);
CombatResult combat_use_skill(Sprite *caster, Sprite *target, const SkillDef *skill);
int          combat_use_item(Sprite *user, Sprite *target, const char *item_id);
int          combat_cast_time_ms(const Sprite *caster, const SkillDef *skill);
int          combat_weapon_power(const Sprite *s);
int          combat_is_alive(const Sprite *s);
int          combat_in_range(int hex_distance);

#ifdef __cplusplus
}
#endif

#endif

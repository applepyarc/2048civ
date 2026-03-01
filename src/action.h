#ifndef SRC_ACTION_H
#define SRC_ACTION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations of project types */
typedef struct Sprite Sprite;
typedef struct Skill Skill;
typedef struct Item Item;

/* Element enum reference (if present elsewhere); keep as int if unknown */
typedef int Element;
#ifndef ELEM_FIRE
#define ELEM_FIRE 1
#endif

/* Action interfaces adapted to Sprite */
int calc_physical_damage(const Sprite *atk, const Sprite *def, int weapon_power, int is_critical);
int calc_magic_damage(const Sprite *atk, const Sprite *def, const Skill *spell);
int get_physical_defense(const Sprite *c);
int get_magic_defense(const Sprite *c);
int calc_cast_time_ms(const Sprite *caster, const Skill *spell, float skill_cast_modifier);
int can_move_distance(const Sprite *c, int distance);
int get_move_range(const Sprite *c);
int use_item(Sprite *user, const Item *item, Sprite *target);
int view_equipment(const Sprite *c, char *out_buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* SRC_ACTION_H */

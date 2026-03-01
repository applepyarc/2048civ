#ifndef SPRITE_H
#define SPRITE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_EQUIP_SLOTS 5

typedef enum {
    EQUIP_WEAPON,
    EQUIP_SHIELD,
    EQUIP_HELMET,
    EQUIP_ARMOR,
    EQUIP_ACCESSORY
} EquipType;

typedef struct Equipment {
    char *name;
    int type;
    int hp;
    int mp;
    int atk;
    int def;
    int spd;
    int jmp;
    int mov;
    int spec; /* bitfield for special properties (e.g. elemental affinity) */
} Equipment;

typedef struct Sprite {
    char *name;
    char *job;
    char *image;
    int level;
    int max_hp;
    int hp;
    int max_mp;
    int mp;
    int move;
    int jump;
    int speed;
    int attack;
    int defense;
    Equipment equipments[MAX_EQUIP_SLOTS];
    int x;
    int y;
} Sprite;

/* Creation / destruction */
Sprite *sprite_create(const char *name, const char *job, const char *image, int level);
void sprite_destroy(Sprite *s);

/* Basic getters */
const char *sprite_get_name(const Sprite *s);
const char *sprite_get_job(const Sprite *s);
const char *sprite_get_image(const Sprite *s);
int sprite_get_level(const Sprite *s);
int sprite_get_hp(const Sprite *s);
int sprite_get_mp(const Sprite *s);
int sprite_get_x(const Sprite *s);
int sprite_get_y(const Sprite *s);

/* Basic setters */
void sprite_set_name(Sprite *s, const char *name);
void sprite_set_job(Sprite *s, const char *job);
void sprite_set_image(Sprite *s, const char *image);
void sprite_set_position(Sprite *s, int x, int y);
void sprite_set_hp(Sprite *s, int hp);
void sprite_set_mp(Sprite *s, int mp);

/* Actions */
int sprite_move(Sprite *s, int dx, int dy);
int sprite_attack(Sprite *attacker, Sprite *defender);
int sprite_use_item(Sprite *s, const char *item_id);

#ifdef __cplusplus
}
#endif

#endif /* SPRITE_H */

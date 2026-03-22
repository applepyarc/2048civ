#include <stdlib.h>
#include <string.h>
#include "sprite.h"

Sprite *sprite_create(const char *name, const char *job, const char *image, int level) {
    Sprite *s = calloc(1, sizeof(Sprite));
    if (!s) return NULL;
    s->name  = name  ? strdup(name)  : NULL;
    s->job   = job   ? strdup(job)   : NULL;
    s->image = image ? strdup(image) : NULL;
    s->level = level > 0 ? level : 1;
    s->x = 0; s->y = 0;
    return s;
}

void sprite_destroy(Sprite *s) {
    if (!s) return;
    free(s->name); free(s->job); free(s->image);
    for (int i = 0; i < MAX_EQUIP_SLOTS; i++) free(s->equipments[i].name);
    free(s);
}

const char *sprite_get_name(const Sprite *s)  { return s ? s->name : NULL; }
const char *sprite_get_job(const Sprite *s)   { return s ? s->job  : NULL; }
int  sprite_get_level(const Sprite *s)        { return s ? s->level : 0; }
int  sprite_get_hp(const Sprite *s)           { return s ? s->hp    : 0; }
int  sprite_get_mp(const Sprite *s)           { return s ? s->mp    : 0; }
int  sprite_get_x(const Sprite *s)            { return s ? s->x     : 0; }
int  sprite_get_y(const Sprite *s)            { return s ? s->y     : 0; }

void sprite_set_name(Sprite *s, const char *name) {
    if (!s) return; free(s->name);
    s->name = name ? strdup(name) : NULL;
}
void sprite_set_job(Sprite *s, const char *job) {
    if (!s) return; free(s->job);
    s->job = job ? strdup(job) : NULL;
}

/* ★ KEY FIX: 原来是空stub，现在真正赋值 */
void sprite_set_position(Sprite *s, int x, int y) {
    if (!s) return;
    s->x = x;
    s->y = y;
}

void sprite_set_hp(Sprite *s, int hp) {
    if (!s) return;
    if (hp < 0) hp = 0; if (hp > s->max_hp) hp = s->max_hp;
    s->hp = hp;
}
void sprite_set_mp(Sprite *s, int mp) {
    if (!s) return;
    if (mp < 0) mp = 0; if (mp > s->max_mp) mp = s->max_mp;
    s->mp = mp;
}
int sprite_move(Sprite *s, int dx, int dy) {
    if (!s) return 0; s->x += dx; s->y += dy; return 1;
}
int sprite_attack(Sprite *attacker, Sprite *defender) {
    if (!attacker || !defender) return 0;
    int dmg = attacker->attack - defender->defense / 2;
    if (dmg < 1) dmg = 1;
    defender->hp -= dmg; if (defender->hp < 0) defender->hp = 0;
    return dmg;
}
int sprite_use_item(Sprite *s, const char *item_id) {
    if (!s || !item_id) return 0;
    if (strcmp(item_id, "potion") == 0) { s->hp += 50; if (s->hp > s->max_hp) s->hp = s->max_hp; return 1; }
    if (strcmp(item_id, "ether")  == 0) { s->mp += 30; if (s->mp > s->max_mp) s->mp = s->max_mp; return 1; }
    return 0;
}

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sprite.h"

static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

Sprite *sprite_create(const char *name, const char *job, const char *image, int level) {
    Sprite *s = (Sprite *)malloc(sizeof(Sprite));
    if (!s) return NULL;
    s->name = safe_strdup(name ? name : "");
    s->job = safe_strdup(job ? job : "");
    s->image = safe_strdup(image ? image : "");
    s->level = level > 0 ? level : 1;
    /* simple default stats based on level */
    s->max_hp = 100 + (s->level - 1) * 10;
    s->hp = s->max_hp;
    s->max_mp = 30 + (s->level - 1) * 5;
    s->mp = s->max_mp;
    s->move = 2;
    s->jump = 1;
    s->speed = 5;
    s->attack = 10 + (s->level - 1) * 2;
    s->defense = 5 + (s->level - 1) * 1;
    s->equipment = NULL;
    s->x = 0;
    s->y = 0;
    return s;
}

void sprite_destroy(Sprite *s) {
    if (!s) return;
    free(s->name);
    free(s->job);
    free(s->image);
    if (s->equipment) {
        free(s->equipment->name);
        free(s->equipment);
    }
    free(s);
}

const char *sprite_get_name(const Sprite *s) { return s ? s->name : NULL; }
const char *sprite_get_job(const Sprite *s) { return s ? s->job : NULL; }
const char *sprite_get_image(const Sprite *s) { return s ? s->image : NULL; }
int sprite_get_level(const Sprite *s) { return s ? s->level : 0; }
int sprite_get_hp(const Sprite *s) { return s ? s->hp : 0; }
int sprite_get_mp(const Sprite *s) { return s ? s->mp : 0; }
int sprite_get_x(const Sprite *s) { return s ? s->x : 0; }
int sprite_get_y(const Sprite *s) { return s ? s->y : 0; }

void sprite_set_name(Sprite *s, const char *name) {
    if (!s) return;
    char *n = safe_strdup(name ? name : "");
    if (!n) return;
    free(s->name);
    s->name = n;
}

void sprite_set_job(Sprite *s, const char *job) {
    if (!s) return;
    char *n = safe_strdup(job ? job : "");
    if (!n) return;
    free(s->job);
    s->job = n;
}

void sprite_set_image(Sprite *s, const char *image) {
    if (!s) return;
    char *n = safe_strdup(image ? image : "");
    if (!n) return;
    free(s->image);
    s->image = n;
}

void sprite_set_position(Sprite *s, int x, int y) {
    if (!s) return;
    s->x = x;
    s->y = y;
}

void sprite_set_hp(Sprite *s, int hp) {
    if (!s) return;
    if (hp < 0) hp = 0;
    if (hp > s->max_hp) hp = s->max_hp;
    s->hp = hp;
}

void sprite_set_mp(Sprite *s, int mp) {
    if (!s) return;
    if (mp < 0) mp = 0;
    if (mp > s->max_mp) mp = s->max_mp;
    s->mp = mp;
}

int sprite_move(Sprite *s, int dx, int dy) {
    if (!s) return -1;
    s->x += dx;
    s->y += dy;
    return 0;
}

int sprite_attack(Sprite *attacker, Sprite *defender) {
    if (!attacker || !defender) return 0;
    int base = attacker->attack - defender->defense;
    int dmg = base > 0 ? base : 1;
    if (dmg > defender->hp) dmg = defender->hp;
    defender->hp -= dmg;
    if (defender->hp < 0) defender->hp = 0;
    return dmg;
}

int sprite_use_item(Sprite *s, const char *item_id) {
    if (!s || !item_id) return 0;
    /* Simple built-in items:
       - "potion" restores 50 HP
       - "ether" restores 30 MP
    */
    if (strcmp(item_id, "potion") == 0) {
        int heal = 50;
        s->hp += heal;
        if (s->hp > s->max_hp) s->hp = s->max_hp;
        return 1;
    }
    if (strcmp(item_id, "ether") == 0) {
        int restore = 30;
        s->mp += restore;
        if (s->mp > s->max_mp) s->mp = s->max_mp;
        return 1;
    }
    return 0; /* unknown item */
}

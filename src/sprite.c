#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "job.h"
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
    for (int i = 0; i < MAX_EQUIP_SLOTS; ++i) {
        s->equipments[i].name = NULL;
        s->equipments[i].type = 0;
        s->equipments[i].hp = 0;
        s->equipments[i].mp = 0;
        s->equipments[i].atk = 0;
        s->equipments[i].def = 0;
        s->equipments[i].spd = 0;
        s->equipments[i].jmp = 0;
        s->equipments[i].mov = 0;
        s->equipments[i].spec = 0;
    }
    s->x = 0;
    s->y = 0;
    return s;
}

void sprite_destroy(Sprite *s) {
    if (!s) return;
    free(s->name);
    free(s->job);
    free(s->image);
    for (int i = 0; i < 3; ++i) {
        if (s->equipments[i].name) {
            free(s->equipments[i].name);
        }
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

// 计算物理攻击伤害
int calculate_physical_damage(Sprite *attacker, Sprite *defender) {
    if (!attacker || !defender) return 0;

    int base_damage = attacker->attack;
    int defense = defender->defense;

    // 基础伤害计算：攻击力 - 防御力
    int damage = base_damage - defense;

    // 职业加成
    JobType job_type = get_job_type(attacker->job);
    switch (job_type) {
        case JOB_WARRIOR:
            damage += base_damage * 0.2; // 战士额外20%伤害
            break;
        case JOB_ROGUE:
            // 盗贼有暴击几率
            if (rand() % 100 < 30) { // 30%暴击几率
                damage *= 2;
            }
            break;
        case JOB_ARCHER:
            damage += 5; // 弓箭手固定加成
            break;
        default:
            break;
    }

    // 等级加成
    damage += attacker->level * 2;

    // 随机波动 (±10%)
    int variation = damage * 0.1;
    damage += (rand() % (2 * variation + 1)) - variation;

    // 确保最小伤害为1
    if (damage < 1) damage = 1;

    return damage;
}

// 计算魔法攻击伤害
int calculate_magic_damage(Sprite *attacker, Sprite *defender) {
    if (!attacker || !defender) return 0;

    int base_damage = attacker->attack;

    // 魔法伤害基于攻击力和魔法值
    int damage = base_damage + attacker->mp * 0.5;

    // 职业加成
    JobType job_type = get_job_type(attacker->job);
    switch (job_type) {
        case JOB_MAGE:
            damage += base_damage * 0.3; // 法师额外30%魔法伤害
            break;
        case JOB_CLERIC:
            // 牧师对不死系有额外伤害（这里简化处理）
            if (defender->hp < defender->max_hp * 0.3) {
                damage += 10; // 对低血量目标额外伤害
            }
            break;
        default:
            break;
    }

    // 等级加成
    damage += attacker->level * 3;

    // 随机波动 (±15%)
    int variation = damage * 0.15;
    damage += (rand() % (2 * variation + 1)) - variation;

    // 确保最小伤害为1
    if (damage < 1) damage = 1;

    // 消耗魔法值
    int mp_cost = damage / 5 + 5;
    if (mp_cost > attacker->mp) {
        mp_cost = attacker->mp; // 最多消耗所有魔法值
    }
    attacker->mp -= mp_cost;

    return damage;
}

int sprite_attack(Sprite *attacker, Sprite *defender, int attack_mode) {
    if (!attacker || !defender) return 0;

    int damage = 0;

    // 根据攻击模式选择伤害计算方式
    if (attack_mode == ATTACK_MODE_PHYSICAL) {
        damage = calculate_physical_damage(attacker, defender);
    } else if (attack_mode == ATTACK_MODE_MAGIC) {
        damage = calculate_magic_damage(attacker, defender);
    } else {
        // 默认使用物理攻击
        damage = calculate_physical_damage(attacker, defender);
    }

    // 应用伤害
    if (damage > defender->hp) damage = defender->hp;
    defender->hp -= damage;
    if (defender->hp < 0) defender->hp = 0;

    return damage;
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
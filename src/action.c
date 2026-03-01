#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sprite.h"
#include "action.h"

typedef struct Skill {
    char *name;
    int power;
    int base_cast_ms;
    Element elem;
} Skill;

typedef struct Item {
    char *name;
    int heal_hp;
    int heal_mp;
} Item;

static int clamp_int(int v, int lo, int hi) { if (v < lo) return lo; if (v > hi) return hi; return v; }

int calc_physical_damage(const Sprite *atk, const Sprite *def, int weapon_power, int is_critical) {
    // 基础公式：atk.str * 2 + weapon_power - def.phy_def/2
    if (!atk || !def) return 0;
    int base = atk->attack * 2 + weapon_power;
    int mitig = def->defense / 2;
    int dmg = base - mitig;
    if (is_critical) {
        // 暴击乘以 1.5
        dmg = (int)(dmg * 1.5f);
    }
    if (dmg < 1) dmg = 1;
    return dmg;
}

int calc_magic_damage(const Sprite *atk, const Sprite *def, const Skill *spell) {
    if (!atk || !def || !spell) return 0;
    int base = atk->attack * 2 + spell->power;
    int mitig = def->defense / 2;
    int dmg = base - mitig;
    // 元素相性示例：火对冰有加成，冰对火有减成（可扩展）
    if (spell->elem == ELEM_FIRE) {
        // 若被攻击者装备或状态影响会在更高层处理；此处为示例占位
    }
    if (dmg < 1) dmg = 1;
    return dmg;
}

int get_physical_defense(const Sprite *c) {
    if (!c) return 0;
    return c->defense;
}

int get_magic_defense(const Sprite *c) {
    if (!c) return 0;
    return c->defense;
}

int calc_cast_time_ms(const Sprite *caster, const Skill *spell, float skill_cast_modifier) {
    if (!caster || !spell) return 0;
    // 基础：spell->base_cast_ms，受智力与额外加成影响
    float base_ms = (float)spell->base_cast_ms;
    // 智力每点降低 0.5% 吟唱（可调），cast_speed_bonus 为额外百分比（0.1 表示 10%）
    float intl_reduction = caster->speed * 0.005f; /* 0.5% per INT */
    float total_multiplier = 1.0f - intl_reduction; // - caster->cast_speed_bonus;
    if (total_multiplier < 0.2f) total_multiplier = 0.2f; /* 不低于20% */
    total_multiplier *= skill_cast_modifier; /* 技能固有倍率（如咏唱缩短） */
    if (total_multiplier < 0.1f) total_multiplier = 0.1f;
    int result = (int)(base_ms * total_multiplier);
    if (result < 100) result = 100; /* 最低阈值100ms */
    return result;
}

int can_move_distance(const Sprite *c, int distance) {
    if (!c) return 0;
    if (distance < 0) return 0;
    return distance <= c->move;
}

int get_move_range(const Sprite *c) {
    if (!c) return 0;
    return c->move;
}

int use_item(Sprite *user, const Item *item, Sprite *target) {
    if (!item || !target) return 0;
    /* 简单实现：治疗/回魔/直接数值增益 */
    if (item->heal_hp != 0) {
        if (target->hp >= target->max_hp) return 0;
        target->hp += item->heal_hp;
        if (target->hp > target->max_hp) target->hp = target->max_hp;
    }
    if (item->heal_mp != 0) {
        if (target->mp >= target->max_mp) return 0;
        target->mp += item->heal_mp;
        if (target->mp > target->max_mp) target->mp = target->max_mp;
    }
    /* 可扩展：增益、复活等效果由更高层处理 */
    return 1;
}

int view_equipment(const Sprite *c, char *out_buf, size_t buf_len) {
    if (!c || !out_buf || buf_len == 0) return 0;
    size_t used = 0;
    for (int i = 0; i < 8; ++i) {
        const char *name = c->equipment[i].name;
        if (!name) continue;
        int res = snprintf(out_buf + used, (used < buf_len) ? (buf_len - used) : 0, "%s%s", name, (i < 7 ? ", " : ""));
        if (res < 0) break;
        used += (size_t)res;
        if (used >= buf_len) { used = buf_len - 1; break; }
    }
    if (used == 0) {
        strncpy(out_buf, "(无装备)", buf_len);
        out_buf[buf_len ? buf_len - 1 : 0] = '\0';
        return (int)strlen(out_buf);
    }
    /* 确保以终止符结尾 */
    out_buf[buf_len ? buf_len - 1 : 0] = '\0';
    return (int)used;
}

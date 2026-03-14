#ifndef JOB_H
#define JOB_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
    JOB_WARRIOR = 0,    // Warrior: Strong physical attacks
    JOB_MAGE,           // Mage: Strong magic attacks
    JOB_ROGUE,          // Rogue: High critical hit chance
    JOB_CLERIC,         // Cleric: Healing and support
    JOB_ARCHER          // Archer: Ranged attacks
} JobType;

typedef enum {
    ATTACK_MODE_NONE = 0,
    ATTACK_MODE_PHYSICAL,
    ATTACK_MODE_MAGIC
} AttackMode;

JobType get_job_type(const char* job_name);
int get_attack_mode(const char* job_name);

#if defined(__cplusplus)
}
#endif

#endif /* JOB_H */

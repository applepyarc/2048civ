#include <string.h>

#include "job.h"

JobType get_job_type(const char* job_name) {
    if (!job_name) return JOB_WARRIOR;
    
    if (strstr(job_name, "Warrior") || strstr(job_name, "战士")) return JOB_WARRIOR;
    if (strstr(job_name, "Mage") || strstr(job_name, "法师")) return JOB_MAGE;
    if (strstr(job_name, "Rogue") || strstr(job_name, "盗贼")) return JOB_ROGUE;
    if (strstr(job_name, "Cleric") || strstr(job_name, "牧师")) return JOB_CLERIC;
    if (strstr(job_name, "Archer") || strstr(job_name, "弓箭手")) return JOB_ARCHER;
    
    return JOB_WARRIOR;
}

int get_attack_mode(const char* job_name) {
    JobType job_type = get_job_type(job_name);
    switch (job_type) {
        case JOB_WARRIOR:
        case JOB_ROGUE:
        case JOB_ARCHER:
            return ATTACK_MODE_PHYSICAL;
        case JOB_MAGE:
        case JOB_CLERIC:
            return ATTACK_MODE_MAGIC;
        default:
            return ATTACK_MODE_PHYSICAL;
    }
}

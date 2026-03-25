#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "roster.h"

static void str_trim(char *s) {
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n-1])) s[--n] = '\0';
}
static int str_eq(const char *a, const char *b) { return strcmp(a,b)==0; }

int roster_load(Roster *r, const char *path) {
    memset(r, 0, sizeof(*r));
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr,"[roster] Cannot open '%s'\n",path); return 0; }
    char line[512];
    RosterChar *cur = NULL;
    int in_entry = 0;
    while (fgets(line, sizeof(line), f)) {
        char *hash = strchr(line,'#'); if (hash) *hash='\0';
        str_trim(line); if (!line[0]) continue;
        if (line[0] == '[') {
            char tag[64];
            if (sscanf(line,"[%63[^]]",tag)==1) {
                str_trim(tag);
                if (str_eq(tag,"character")) {
                    if (r->count >= ROSTER_MAX_CHARS) { break; }
                    cur = &r->all[r->count++];
                    memset(cur, 0, sizeof(*cur));
                    cur->run_frames = 4;
                    in_entry = 1;
                }
            }
            continue;
        }
        if (!in_entry || !cur) continue;
        char key[128], val[384];
        char *eq = strchr(line,'='); if (!eq) continue;
        size_t klen = (size_t)(eq-line);
        if (klen >= sizeof(key)) continue;
        strncpy(key, line, klen); key[klen]='\0'; str_trim(key);
        strncpy(val, eq+1, sizeof(val)-1); val[sizeof(val)-1]='\0'; str_trim(val);
        if (!val[0]) continue;
        if      (str_eq(key,"name"))        strncpy(cur->name,        val,ROSTER_NAME_LEN-1);
        else if (str_eq(key,"job"))         strncpy(cur->job,         val,ROSTER_NAME_LEN-1);
        else if (str_eq(key,"idle_sprite")) strncpy(cur->idle_sprite, val,ROSTER_SPRITE_LEN-1);
        else if (str_eq(key,"run_sprite"))  strncpy(cur->run_sprite,  val,ROSTER_SPRITE_LEN-1);
        else if (str_eq(key,"faction")) {
            if      (str_eq(val,"player")) cur->faction=FACTION_PLAYER;
            else if (str_eq(val,"enemy"))  cur->faction=FACTION_ENEMY;
        }
        else if (str_eq(key,"run_frames")) cur->run_frames=atoi(val);
        else if (str_eq(key,"hp_base"))    cur->hp_base  =atoi(val);
        else if (str_eq(key,"hp_rand"))    cur->hp_rand  =atoi(val);
        else if (str_eq(key,"atk_base"))   cur->atk_base =atoi(val);
        else if (str_eq(key,"atk_rand"))   cur->atk_rand =atoi(val);
        else if (str_eq(key,"def_base"))   cur->def_base =atoi(val);
        else if (str_eq(key,"def_rand"))   cur->def_rand =atoi(val);
        else if (str_eq(key,"move"))       cur->move     =atoi(val);
    }
    fclose(f);
    for (int i=0; i<r->count; i++) {
        RosterChar *c=&r->all[i];
        if (c->faction==FACTION_PLAYER && r->player_count<ROSTER_MAX_CHARS)
            r->player_chars[r->player_count++]=c;
        else if (c->faction==FACTION_ENEMY && r->enemy_count<ROSTER_MAX_CHARS)
            r->enemy_chars[r->enemy_count++]=c;
    }
    printf("[roster] Loaded %d characters (%d player, %d enemy) from '%s'\n",
           r->count, r->player_count, r->enemy_count, path);
    return 1;
}

void roster_free(Roster *r) { (void)r; }

RosterChar *roster_random_player(Roster *r) {
    if (r->player_count==0) return NULL;
    return r->player_chars[rand() % r->player_count];
}

int roster_random_enemies(Roster *r, RosterChar **out, int count) {
    if (r->enemy_count==0 || count<=0) return 0;
    int indices[ROSTER_MAX_CHARS];
    int n = r->enemy_count;
    for (int i=0; i<n; i++) indices[i]=i;
    for (int i=n-1; i>0; i--) {
        int j=rand()%(i+1);
        int tmp=indices[i]; indices[i]=indices[j]; indices[j]=tmp;
    }
    int written=0;
    for (int i=0; i<count; i++)
        out[written++]=r->enemy_chars[indices[i%n]];
    return written;
}
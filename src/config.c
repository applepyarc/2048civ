/* config.c - runtime configuration for 2048civ (phase 2+3) */
#include <stdlib.h>
#include <string.h>
#include "config.h"

#define DEFAULT_MAP_ROWS        50
#define DEFAULT_MAP_COLS        50
#define DEFAULT_FONT_PATH       "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#define DEFAULT_FONT_SIZE       16
#define DEFAULT_WINDOW_WIDTH    1024
#define DEFAULT_WINDOW_HEIGHT   1000
#define DEFAULT_SPLIT_RATIO     0.8f
#define DEFAULT_MOVE_MS         200
#define DEFAULT_ATLAS_IMAGE     "res/drawable/dungeon.png"
#define DEFAULT_ATLAS_DESC      "res/drawable/dungeon"
#define DEFAULT_ROSTER_PATH     "res/roster.cfg"

static int   s_map_rows=DEFAULT_MAP_ROWS, s_map_cols=DEFAULT_MAP_COLS;
static char  s_font_path[512]={0}, s_atlas_image[512]={0}, s_atlas_desc[512]={0};
static char  s_roster_path[512] = {0};
static int   s_font_size=DEFAULT_FONT_SIZE, s_window_width=DEFAULT_WINDOW_WIDTH;
static int   s_window_height=DEFAULT_WINDOW_HEIGHT, s_move_ms=DEFAULT_MOVE_MS;
static float s_split_ratio=DEFAULT_SPLIT_RATIO;
static PerlinParams s_perlin={.scale=0.03f,.octaves=5,.persistence=0.5f,.moisture_scale=0.06f,.moisture_octaves=4,.seed=0};
static int s_initialized=0;

static void str_env(const char *var,char *dst,size_t dsz){const char *e=getenv(var);if(e&&e[0]){strncpy(dst,e,dsz-1);dst[dsz-1]='\0';}}
static void int_env(const char *var,int *dst){const char *e=getenv(var);if(e){int v=atoi(e);if(v>0)*dst=v;}}
static void flt_env(const char *var,float *dst){const char *e=getenv(var);if(e){float f=(float)atof(e);if(f>0.0f)*dst=f;}}

static void apply_env_overrides(void){
    int_env("2048CIV_MAP_ROWS",&s_map_rows); int_env("2048CIV_MAP_COLS",&s_map_cols);
    str_env("2048CIV_FONT",s_font_path,sizeof(s_font_path)); int_env("2048CIV_FONT_SIZE",&s_font_size);
    int_env("2048CIV_WINDOW_WIDTH",&s_window_width); int_env("2048CIV_WINDOW_HEIGHT",&s_window_height);
    {const char *e=getenv("2048CIV_SPLIT_RATIO");if(e){float f=(float)atof(e);if(f>0.0f&&f<1.0f)s_split_ratio=f;}}
    int_env("2048CIV_MOVE_MS",&s_move_ms);
    str_env("2048CIV_ATLAS_IMAGE",s_atlas_image,sizeof(s_atlas_image));
    str_env("2048CIV_ATLAS_DESC",s_atlas_desc,sizeof(s_atlas_desc));
    str_env("2048CIV_ROSTER_PATH", s_roster_path, sizeof(s_roster_path));
    flt_env("2048CIV_PERLIN_SCALE",&s_perlin.scale); int_env("2048CIV_PERLIN_OCTAVES",&s_perlin.octaves);
    flt_env("2048CIV_PERLIN_PERSISTENCE",&s_perlin.persistence);
    flt_env("2048CIV_PERLIN_MOISTURE_SCALE",&s_perlin.moisture_scale);
    int_env("2048CIV_PERLIN_MOISTURE_OCTAVES",&s_perlin.moisture_octaves);
    {const char *e=getenv("2048CIV_PERLIN_SEED");if(e){unsigned int v=(unsigned int)atoi(e);if(v)s_perlin.seed=v;}}
}

int config_init(void){
    if(s_initialized)return 1;
    strncpy(s_font_path,DEFAULT_FONT_PATH,sizeof(s_font_path)-1);
    strncpy(s_atlas_image,DEFAULT_ATLAS_IMAGE,sizeof(s_atlas_image)-1);
    strncpy(s_atlas_desc,DEFAULT_ATLAS_DESC,sizeof(s_atlas_desc)-1);
    strncpy(s_roster_path, DEFAULT_ROSTER_PATH, sizeof(s_roster_path)-1);
    apply_env_overrides(); s_initialized=1; return 1;
}

int          config_get_map_rows(void)          {if(!s_initialized)config_init();return s_map_rows;}
int          config_get_map_cols(void)          {if(!s_initialized)config_init();return s_map_cols;}
const char  *config_get_font_path(void)         {if(!s_initialized)config_init();return s_font_path;}
int          config_get_font_size(void)         {if(!s_initialized)config_init();return s_font_size;}
int          config_get_window_width(void)      {if(!s_initialized)config_init();return s_window_width;}
int          config_get_window_height(void)     {if(!s_initialized)config_init();return s_window_height;}
float        config_get_split_ratio(void)       {if(!s_initialized)config_init();return s_split_ratio;}
int          config_get_move_ms(void)           {if(!s_initialized)config_init();return s_move_ms;}
const char  *config_get_atlas_image_path(void)  {if(!s_initialized)config_init();return s_atlas_image;}
const char  *config_get_atlas_desc_path(void)   {if(!s_initialized)config_init();return s_atlas_desc;}
const char  *config_get_roster_path(void)       {if(!s_initialized)config_init(); return s_roster_path; }
void         config_get_perlin_params(PerlinParams *out){if(!s_initialized)config_init();if(out)*out=s_perlin;}
void         config_free(void)                  {s_initialized=0;}

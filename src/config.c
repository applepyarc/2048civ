/* config.c - runtime configuration for 2048civ
 * Supports simple environment-variable overrides.
 */
#include "config.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_MAP_ROWS 100
#define DEFAULT_MAP_COLS 100
#define DEFAULT_FONT_PATH "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#define DEFAULT_FONT_SIZE 16

static int s_map_rows = DEFAULT_MAP_ROWS;
static int s_map_cols = DEFAULT_MAP_COLS;
static char s_font_path[512] = {0};
static int s_font_size = DEFAULT_FONT_SIZE;
static int s_initialized = 0;

static void apply_env_overrides(void) {
    const char* e;
    e = getenv("2048CIV_MAP_ROWS");
    if (e) {
        int v = atoi(e);
        if (v > 0) s_map_rows = v;
    }
    e = getenv("2048CIV_MAP_COLS");
    if (e) {
        int v = atoi(e);
        if (v > 0) s_map_cols = v;
    }
    e = getenv("2048CIV_FONT");
    if (e && e[0]) {
        strncpy(s_font_path, e, sizeof(s_font_path)-1);
        s_font_path[sizeof(s_font_path)-1] = '\0';
    }
    e = getenv("2048CIV_FONT_SIZE");
    if (e) {
        int v = atoi(e);
        if (v > 0) s_font_size = v;
    }
}

int config_init(void) {
    if (s_initialized) return 1;
    /* set defaults */
    strncpy(s_font_path, DEFAULT_FONT_PATH, sizeof(s_font_path)-1);
    s_font_path[sizeof(s_font_path)-1] = '\0';
    /* override from environment */
    apply_env_overrides();
    s_initialized = 1;
    return 1;
}

int config_get_map_rows(void) {
    if (!s_initialized) config_init();
    return s_map_rows;
}

int config_get_map_cols(void) {
    if (!s_initialized) config_init();
    return s_map_cols;
}

const char* config_get_font_path(void) {
    if (!s_initialized) config_init();
    return s_font_path;
}

int config_get_font_size(void) {
    if (!s_initialized) config_init();
    return s_font_size;
}

void config_free(void) {
    /* nothing to free now, placeholder for future resources */
    s_initialized = 0;
}

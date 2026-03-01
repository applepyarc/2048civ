/* config.c - runtime configuration for 2048civ
 * Supports simple environment-variable overrides.
 */
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define DEFAULT_MAP_ROWS 50
#define DEFAULT_MAP_COLS 50
#define DEFAULT_FONT_PATH "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#define DEFAULT_FONT_SIZE 16
/* Window / layout defaults */
#define DEFAULT_WINDOW_WIDTH 1024
#define DEFAULT_WINDOW_HEIGHT 1000
#define DEFAULT_SPLIT_RATIO 0.8f /* main area fraction (e.g. 0.8 == 4/5) */
/* movement defaults (milliseconds per tile) */
#define DEFAULT_MOVE_MS 200

static int s_map_rows = DEFAULT_MAP_ROWS;
static int s_map_cols = DEFAULT_MAP_COLS;
static char s_font_path[512] = {0};
static int s_font_size = DEFAULT_FONT_SIZE;
static int s_window_width = DEFAULT_WINDOW_WIDTH;
static int s_window_height = DEFAULT_WINDOW_HEIGHT;
static float s_split_ratio = DEFAULT_SPLIT_RATIO;
static int s_move_ms = DEFAULT_MOVE_MS;
/* perlin defaults */
static PerlinParams s_perlin_params = {
    .scale = 0.03f,
    .octaves = 5,
    .persistence = 0.5f,
    .moisture_scale = 0.06f,
    .moisture_octaves = 4,
    .seed = 0
};
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
    e = getenv("2048CIV_WINDOW_WIDTH");
    if (e) {
        int v = atoi(e);
        if (v > 0) s_window_width = v;
    }
    e = getenv("2048CIV_WINDOW_HEIGHT");
    if (e) {
        int v = atoi(e);
        if (v > 0) s_window_height = v;
    }
    e = getenv("2048CIV_SPLIT_RATIO");
    if (e) {
        float f = (float)atof(e);
        if (f > 0.0f && f < 1.0f) s_split_ratio = f;
    }
    e = getenv("2048CIV_MOVE_MS");
    if (e) {
        int v = atoi(e);
        if (v > 0) s_move_ms = v;
    }
    /* perlin overrides */
    e = getenv("2048CIV_PERLIN_SCALE");
    if (e) {
        float f = (float)atof(e);
        if (f > 0.0f) s_perlin_params.scale = f;
    }
    e = getenv("2048CIV_PERLIN_OCTAVES");
    if (e) {
        int v = atoi(e);
        if (v > 0) s_perlin_params.octaves = v;
    }
    e = getenv("2048CIV_PERLIN_PERSISTENCE");
    if (e) {
        float f = (float)atof(e);
        if (f > 0.0f && f <= 1.0f) s_perlin_params.persistence = f;
    }
    e = getenv("2048CIV_PERLIN_MOISTURE_SCALE");
    if (e) {
        float f = (float)atof(e);
        if (f > 0.0f) s_perlin_params.moisture_scale = f;
    }
    e = getenv("2048CIV_PERLIN_MOISTURE_OCTAVES");
    if (e) {
        int v = atoi(e);
        if (v > 0) s_perlin_params.moisture_octaves = v;
    }
    e = getenv("2048CIV_PERLIN_SEED");
    if (e) {
        unsigned int v = (unsigned int)atoi(e);
        if (v != 0) s_perlin_params.seed = v;
    }
}

int config_init(void) {
    if (s_initialized) return 1;
    /* set defaults */
    strncpy(s_font_path, DEFAULT_FONT_PATH, sizeof(s_font_path)-1);
    s_font_path[sizeof(s_font_path)-1] = '\0';
    s_window_width = DEFAULT_WINDOW_WIDTH;
    s_window_height = DEFAULT_WINDOW_HEIGHT;
    s_split_ratio = DEFAULT_SPLIT_RATIO;
    s_move_ms = DEFAULT_MOVE_MS;
    /* reset perlin defaults */
    s_perlin_params.scale = 0.03f;
    s_perlin_params.octaves = 5;
    s_perlin_params.persistence = 0.5f;
    s_perlin_params.moisture_scale = 0.06f;
    s_perlin_params.moisture_octaves = 4;
    s_perlin_params.seed = 0;
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

int config_get_window_width(void) {
    if (!s_initialized) config_init();
    return s_window_width;
}

int config_get_window_height(void) {
    if (!s_initialized) config_init();
    return s_window_height;
}

float config_get_split_ratio(void) {
    if (!s_initialized) config_init();
    return s_split_ratio;
}

void config_get_perlin_params(PerlinParams* out) {
    if (!s_initialized) config_init();
    if (!out) return;
    *out = s_perlin_params;
}

int config_get_move_ms(void) {
    if (!s_initialized) config_init();
    return s_move_ms;
}

void config_free(void) {
    /* nothing to free now, placeholder for future resources */
    s_initialized = 0;
}

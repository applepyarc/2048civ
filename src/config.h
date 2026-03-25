/* config.h - runtime configuration for 2048civ (phase 2+3) */
#ifndef CONFIG_H
#define CONFIG_H

#include "perlin.h"

#ifdef __cplusplus
extern "C" {
#endif

int          config_init(void);
int          config_get_map_rows(void);
int          config_get_map_cols(void);
const char  *config_get_font_path(void);
int          config_get_font_size(void);
int          config_get_window_width(void);
int          config_get_window_height(void);
float        config_get_split_ratio(void);
int          config_get_move_ms(void);
const char  *config_get_atlas_image_path(void);
const char  *config_get_atlas_desc_path(void);
const char  *config_get_roster_path(void);
void         config_get_perlin_params(PerlinParams *out);
void         config_free(void);

#ifdef __cplusplus
}
#endif
#endif /* CONFIG_H */

/* config.h - runtime configuration for 2048civ */
#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

int config_init(void); /* initialize config (reads env vars) */
int config_get_map_rows(void);
int config_get_map_cols(void);
const char* config_get_font_path(void);
int config_get_font_size(void);
void config_free(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */

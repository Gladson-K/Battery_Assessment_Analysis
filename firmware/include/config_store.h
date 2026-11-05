#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include "battery_types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool config_store_load(ba_config_t *config, bat_profile_t *profile);
bool config_store_save(const ba_config_t *config, bat_profile_t profile);
bool config_store_erase(void);

#ifdef __cplusplus
}
#endif

#endif

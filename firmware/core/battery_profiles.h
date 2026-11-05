#ifndef BATTERY_PROFILES_H
#define BATTERY_PROFILES_H

#include "battery_types.h"

#ifdef __cplusplus
extern "C" {
#endif

bool ba_config_from_profile(ba_config_t *config, bat_profile_t profile, uint8_t num_temp);
const char *ba_profile_name(bat_profile_t profile);

#ifdef __cplusplus
}
#endif

#endif

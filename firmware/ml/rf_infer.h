#ifndef BATTERY_RF_INFER_H
#define BATTERY_RF_INFER_H

#include "battery_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void rf_infer(const float features[BAT_ML_FEATURE_COUNT], rf_result_t *result);
const char *rf_model_version(void);
uint32_t rf_model_checksum(void);
bool rf_model_self_test(void);

#ifdef __cplusplus
}
#endif

#endif

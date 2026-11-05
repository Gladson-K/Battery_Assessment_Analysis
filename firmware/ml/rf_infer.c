#include "rf_infer.h"

#include "filters.h"
#include "rf_model.h"

#include <math.h>

#if RF_FEATURE_COUNT != BAT_ML_FEATURE_COUNT
#error "Generated model feature count does not match BAT_ML_FEATURE_COUNT"
#endif

void rf_infer(const float features[BAT_ML_FEATURE_COUNT], rf_result_t *result)
{
    if (result == 0) {
        return;
    }
    result->p_unsafe = 0.5f;
    result->label = BAT_LABEL_SAFE;
    result->valid = 0;
    result->low_confidence = 1;

    if (features == 0) {
        return;
    }

    for (uint8_t i = 0; i < RF_FEATURE_COUNT; i++) {
        if (!bat_is_finite(features[i])) {
            return;
        }
        if (features[i] < RF_DOMAIN_MIN[i] || features[i] > RF_DOMAIN_MAX[i]) {
            return;
        }
    }

    float sum = 0.0f;
    for (uint16_t tree = 0; tree < RF_TREE_COUNT; tree++) {
        uint16_t root = (uint16_t)RF_TREE_ROOTS[tree];
        uint16_t tree_size = RF_TREE_SIZES[tree];
        uint16_t index = 0;
        uint16_t guard = 0;
        while (guard++ < tree_size) {
            uint16_t node_index = (uint16_t)(root + index);
            if (node_index >= RF_NODE_COUNT) {
                return;
            }
            const rf_node_t *node = &RF_NODES[node_index];
            if ((node->flags & RF_NODE_FLAG_LEAF) != 0u) {
                sum += node->leaf_p;
                break;
            }
            if (node->feature >= RF_FEATURE_COUNT) {
                return;
            }
            float value = features[node->feature];
            int16_t next = value <= node->threshold ? node->left : node->right;
            if (next < 0 || (uint16_t)next >= tree_size) {
                return;
            }
            index = (uint16_t)next;
        }
        if (guard >= tree_size) {
            return;
        }
    }

    float p = sum / (float)RF_TREE_COUNT;
    result->p_unsafe = bat_clampf(p, 0.0f, 1.0f);
    result->label = result->p_unsafe >= 0.5f ? BAT_LABEL_UNSAFE : BAT_LABEL_SAFE;
    result->valid = 1;
    float confidence = result->p_unsafe > 0.5f ? result->p_unsafe : (1.0f - result->p_unsafe);
    result->low_confidence = confidence < 0.65f ? 1u : 0u;
}

const char *rf_model_version(void)
{
    return RF_MODEL_VERSION;
}

uint32_t rf_model_checksum(void)
{
    return RF_MODEL_CHECKSUM;
}

bool rf_model_self_test(void)
{
    if (RF_TREE_COUNT == 0u || RF_NODE_COUNT == 0u) {
        return false;
    }
    for (uint16_t tree = 0; tree < RF_TREE_COUNT; tree++) {
        uint16_t root = (uint16_t)RF_TREE_ROOTS[tree];
        uint16_t tree_size = RF_TREE_SIZES[tree];
        if (tree_size == 0u || (uint32_t)root + tree_size > RF_NODE_COUNT) {
            return false;
        }
        for (uint16_t i = 0; i < tree_size; i++) {
            const rf_node_t *node = &RF_NODES[root + i];
            if ((node->flags & RF_NODE_FLAG_LEAF) == 0u) {
                if (node->feature >= RF_FEATURE_COUNT) {
                    return false;
                }
                if (node->left < 0 || node->right < 0) {
                    return false;
                }
                if ((uint16_t)node->left >= tree_size || (uint16_t)node->right >= tree_size) {
                    return false;
                }
                if (node->left == (int16_t)i || node->right == (int16_t)i) {
                    return false;
                }
            }
        }
    }
    return true;
}

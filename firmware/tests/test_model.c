#include "model_test_vectors.h"
#include "rf_infer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                                     \
    do {                                                                                     \
        if (!(condition)) {                                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);            \
            failures++;                                                                      \
        }                                                                                    \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                                             \
    do {                                                                                     \
        float a_ = (float)(actual);                                                          \
        float e_ = (float)(expected);                                                        \
        if (fabsf(a_ - e_) > (float)(tolerance)) {                                           \
            fprintf(stderr, "FAIL %s:%d: %f != %f (+/- %f)\n", __FILE__, __LINE__, a_, e_,   \
                    (float)(tolerance));                                                     \
            failures++;                                                                      \
        }                                                                                    \
    } while (0)

int main(void)
{
    CHECK(rf_model_self_test());
    CHECK(rf_model_version() != NULL);
    CHECK(strlen(rf_model_version()) > 0u);
    CHECK(rf_model_checksum() != 0u);

    for (int i = 0; i < MODEL_TEST_VECTOR_COUNT; i++) {
        rf_result_t result;
        rf_infer(MODEL_TEST_VECTORS[i].x, &result);
        CHECK(result.valid == 1u);
        CHECK_NEAR(result.p_unsafe, MODEL_TEST_VECTORS[i].expected_p_unsafe, 1e-3f);
        CHECK(result.label == MODEL_TEST_VECTORS[i].expected_label);
    }

    float valid[BAT_ML_FEATURE_COUNT] = {45, 45, 45, 45, 45, 45, 45, 45,
                                        6.2f, 0.3f, 500.0f, 1.0f};
    rf_result_t result;
    rf_infer(valid, &result);
    CHECK(result.valid == 1u);
    CHECK(result.p_unsafe >= 0.0f && result.p_unsafe <= 1.0f);

    float out_of_domain[BAT_ML_FEATURE_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                0.0f, 0.0f, 0.0f, 0.0f};
    rf_infer(out_of_domain, &result);
    CHECK(result.valid == 0u);

    float not_finite[BAT_ML_FEATURE_COUNT] = {45, 45, 45, 45, 45, 45, 45, 45,
                                             6.2f, 0.3f, 500.0f, 1.0f};
    not_finite[0] = NAN;
    rf_infer(not_finite, &result);
    CHECK(result.valid == 0u);

    if (failures != 0) {
        fprintf(stderr, "%d model test(s) failed\n", failures);
        return 1;
    }
    printf("test_model: %d parity vectors passed\n", MODEL_TEST_VECTOR_COUNT);
    return 0;
}

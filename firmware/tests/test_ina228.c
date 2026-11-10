#include "ina228_math.h"

#include <math.h>
#include <stdio.h>

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

static void test_calibration(void)
{
    float lsb = 0.0f;
    uint16_t cal = 0;

    CHECK(ina228_calibrate(300.0f, 0.0001f, &lsb, &cal));
    CHECK_NEAR(lsb, 300.0f / 524288.0f, 1e-9f);
    CHECK(cal == 750u);

    CHECK(ina228_calibrate(50.0f, 0.001f, &lsb, &cal));
    CHECK_NEAR(lsb, 50.0f / 524288.0f, 1e-9f);
    CHECK(cal == 1250u);

    CHECK(!ina228_calibrate(0.0f, 0.001f, &lsb, &cal));
    CHECK(!ina228_calibrate(50.0f, 0.0f, &lsb, &cal));
    CHECK(!ina228_calibrate(50.0f, 100.0f, &lsb, &cal));
}

static void test_voltage(void)
{
    CHECK_NEAR(ina228_vbus_voltage(0x3C0000u), 48.0f, 0.001f);
    CHECK_NEAR(ina228_vbus_voltage(0x000000u), 0.0f, 0.0001f);
    CHECK_NEAR(ina228_vbus_voltage(0x3FFFF0u), 51.19921875f, 0.001f);
}

static void test_current(void)
{
    float lsb = 300.0f / 524288.0f;

    CHECK_NEAR(ina228_current_a(0x000010u, lsb), lsb, 1e-9f);
    CHECK_NEAR(ina228_current_a(0xFFFFF0u, lsb), -lsb, 1e-9f);
    CHECK_NEAR(ina228_current_a(0x7FFFF0u, lsb), 299.9994278f, 0.001f);
    CHECK_NEAR(ina228_current_a(0x800000u, lsb), -300.0f, 0.001f);
}

static void test_power_and_temperature(void)
{
    float lsb = 300.0f / 524288.0f;

    CHECK_NEAR(ina228_power_w(0x100000u, lsb), 1920.0f, 0.001f);
    CHECK_NEAR(ina228_die_temp_c(0x0C80u), 25.0f, 0.001f);
    CHECK_NEAR(ina228_die_temp_c(0xFB00u), -10.0f, 0.001f);
}

int main(void)
{
    test_calibration();
    test_voltage();
    test_current();
    test_power_and_temperature();
    if (failures != 0) {
        fprintf(stderr, "test_ina228: %d failures\n", failures);
        return 1;
    }
    printf("test_ina228: all tests passed\n");
    return 0;
}

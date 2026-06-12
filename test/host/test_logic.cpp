#include <stdint.h>
#include <stdio.h>

#include "src/fn_logic.h"

static int failures = 0;

#define CHECK(expr) do { \
    if (!(expr)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        failures++; \
    } \
} while (0)

static void test_ms2us() {
    CHECK(ms2us(0)    == 0UL);
    CHECK(ms2us(1)    == 1000UL);
    CHECK(ms2us(1000) == 1000000UL);
    CHECK(ms2us(3000) == 3000000UL);
}

static void test_us2ms() {
    CHECK(us2ms(0)       == 0UL);
    CHECK(us2ms(1000)    == 1UL);
    CHECK(us2ms(1500)    == 1UL);   // truncates toward zero
    CHECK(us2ms(3000000) == 3000UL);
}

static void test_clamp_us() {
    CHECK(clamp_us(0)          == 1UL);     // 0 → 1 (minimum)
    CHECK(clamp_us(1)          == 1UL);
    CHECK(clamp_us(1000)       == 1000UL);
    CHECK(clamp_us(MAX_US)     == MAX_US);
    CHECK(clamp_us(MAX_US + 1) == MAX_US);  // clamped at ceiling
}

static void test_nearest_ms_index() {
    // STEP_MS_LIST: {1,5,10,50,100,250,500,1000,2000,3000}
    CHECK(nearest_ms_index(ms2us(1))    == 0);   // exact: 1ms
    CHECK(nearest_ms_index(ms2us(5))    == 1);   // exact: 5ms
    CHECK(nearest_ms_index(ms2us(10))   == 2);   // exact: 10ms
    CHECK(nearest_ms_index(ms2us(3000)) == 9);   // exact: 3000ms (last)
    CHECK(nearest_ms_index(ms2us(4))    == 1);   // 4ms closer to 5ms (dist=1) than 1ms (dist=3)
    CHECK(nearest_ms_index(ms2us(200))  == 5);   // 200ms closer to 250ms (dist=50) than 100ms (dist=100)
    CHECK(nearest_ms_index(ms2us(250))  == 5);   // exact: 250ms
}

static void test_nearest_us_index() {
    // STEP_US_LIST: {10,50,100,200,500,1000,2000,5000,10000,50000,100000}
    CHECK(nearest_us_index(10)     == 0);    // exact: 10us
    CHECK(nearest_us_index(1000)   == 5);    // exact: 1000us
    CHECK(nearest_us_index(100000) == 10);   // exact: 100000us (last)
    CHECK(nearest_us_index(76)     == 2);    // 76us closer to 100us (dist=24) than 50us (dist=26)
    CHECK(nearest_us_index(74)     == 1);    // 74us closer to 50us (dist=24) than 100us (dist=26)
}

static void test_step_lists_sync() {
    // STEP_MS_LIST_US must equal STEP_MS_LIST[i] * 1000 for every entry
    size_t n_ms = sizeof(STEP_MS_LIST)    / sizeof(STEP_MS_LIST[0]);
    size_t n_us = sizeof(STEP_MS_LIST_US) / sizeof(STEP_MS_LIST_US[0]);
    CHECK(n_ms == n_us);
    for (size_t i = 0; i < n_ms && i < n_us; ++i) {
        CHECK(STEP_MS_LIST_US[i] == (uint32_t)STEP_MS_LIST[i] * 1000UL);
    }
}

static void test_presets() {
    // Verify values via ms2us() so the test is independent of the raw literal in fn_logic.h
    CHECK(PRESET_POSITION_US.on_us  == ms2us(8));
    CHECK(PRESET_POSITION_US.off_us == ms2us(1));
    CHECK(PRESET_TURN_US.on_us      == ms2us(380));
    CHECK(PRESET_TURN_US.off_us     == ms2us(190));
    CHECK(PRESET_ESS_US.on_us       == ms2us(120));
    CHECK(PRESET_ESS_US.off_us      == ms2us(120));
    // L/R turn currently share TURN timing; both on_us and off_us must match
    CHECK(PRESET_L_TURN_US.on_us    == PRESET_TURN_US.on_us);
    CHECK(PRESET_L_TURN_US.off_us   == PRESET_TURN_US.off_us);
    CHECK(PRESET_R_TURN_US.on_us    == PRESET_TURN_US.on_us);
    CHECK(PRESET_R_TURN_US.off_us   == PRESET_TURN_US.off_us);
}

int main() {
    test_ms2us();
    test_us2ms();
    test_clamp_us();
    test_nearest_ms_index();
    test_nearest_us_index();
    test_step_lists_sync();
    test_presets();

    if (failures == 0) {
        printf("All tests passed.\n");
        return 0;
    }
    printf("%d test(s) FAILED.\n", failures);
    return 1;
}

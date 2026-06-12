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
    CHECK(us2ms(0)    == 0UL);
    CHECK(us2ms(1000) == 1UL);
    CHECK(us2ms(1500) == 1UL);  // truncates toward zero
    CHECK(us2ms(3000000) == 3000UL);
}

static void test_clamp_us() {
    CHECK(clamp_us(0)          == 1UL);      // 0 → 1 (minimum)
    CHECK(clamp_us(1)          == 1UL);
    CHECK(clamp_us(1000)       == 1000UL);
    CHECK(clamp_us(MAX_US)     == MAX_US);
    CHECK(clamp_us(MAX_US + 1) == MAX_US);   // clamped at ceiling
}

static void test_nearest_ms_index() {
    // STEP_MS_LIST: {1,5,10,50,100,250,500,1000,2000,3000}
    CHECK(nearest_ms_index(ms2us(1))    == 0);   // exact: 1ms
    CHECK(nearest_ms_index(ms2us(5))    == 1);   // exact: 5ms
    CHECK(nearest_ms_index(ms2us(10))   == 2);   // exact: 10ms
    CHECK(nearest_ms_index(ms2us(3000)) == 9);   // exact: 3000ms (last)
    CHECK(nearest_ms_index(ms2us(4))    == 1);   // 4ms closer to 5ms than 1ms
    CHECK(nearest_ms_index(ms2us(200))  == 5);   // 200ms closer to 250ms (dist=50000us) than 100ms (dist=100000us)
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

static void test_presets() {
    CHECK(PRESET_POSITION_US.on_us  == 8000UL);
    CHECK(PRESET_POSITION_US.off_us == 1000UL);
    CHECK(PRESET_TURN_US.on_us      == 380000UL);
    CHECK(PRESET_TURN_US.off_us     == 190000UL);
    CHECK(PRESET_ESS_US.on_us       == 120000UL);
    CHECK(PRESET_ESS_US.off_us      == 120000UL);
    CHECK(PRESET_L_TURN_US.on_us    == PRESET_R_TURN_US.on_us);
}

int main() {
    test_ms2us();
    test_us2ms();
    test_clamp_us();
    test_nearest_ms_index();
    test_nearest_us_index();
    test_presets();

    if (failures == 0) {
        printf("All tests passed.\n");
        return 0;
    }
    printf("%d test(s) FAILED.\n", failures);
    return 1;
}

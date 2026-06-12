#pragma once
#include <stddef.h>
#include <stdint.h>

// ---- Timing constants ----
static const uint32_t MIN_MS = 1;
static const uint32_t MAX_MS = 3000;
static const uint32_t MAX_US = 3000000UL;

// ---- Timing conversion ----
static inline uint32_t ms2us(uint32_t ms) { return ms * 1000UL; }
static inline uint32_t us2ms(uint32_t us) { return us / 1000UL; }

static inline uint32_t clamp_us(uint32_t v) {
    return v == 0 ? 1u : (v > MAX_US ? MAX_US : v);
}

// ---- Pattern type ----
struct PatternUS { uint32_t on_us, off_us; };

// ---- Preset patterns ----
static const PatternUS PRESET_POSITION_US = { 8000UL,   1000UL   };  // 8ms on, 1ms off
static const PatternUS PRESET_R_TURN_US   = { 380000UL, 190000UL };
static const PatternUS PRESET_L_TURN_US   = { 380000UL, 190000UL };
static const PatternUS PRESET_TURN_US     = { 380000UL, 190000UL };
static const PatternUS PRESET_ESS_US      = { 120000UL, 120000UL };

// ---- Step candidates ----
static const uint16_t STEP_MS_LIST[] = {1,5,10,50,100,250,500,1000,2000,3000};
static const uint32_t STEP_US_LIST[] = {10,50,100,200,500,1000,2000,5000,10000,50000,100000};

static inline int nearest_ms_index(uint32_t step_us) {
    int best = 0;
    uint32_t bestd = 0xFFFFFFFFUL;
    for (size_t i = 0; i < sizeof(STEP_MS_LIST)/sizeof(STEP_MS_LIST[0]); ++i) {
        uint32_t cand = (uint32_t)STEP_MS_LIST[i] * 1000UL;
        uint32_t d = (cand > step_us) ? (cand - step_us) : (step_us - cand);
        if (d < bestd) { bestd = d; best = (int)i; }
    }
    return best;
}

static inline int nearest_us_index(uint32_t step_us) {
    int best = 0;
    uint32_t bestd = 0xFFFFFFFFUL;
    for (size_t i = 0; i < sizeof(STEP_US_LIST)/sizeof(STEP_US_LIST[0]); ++i) {
        uint32_t cand = STEP_US_LIST[i];
        uint32_t d = (cand > step_us) ? (cand - step_us) : (step_us - cand);
        if (d < bestd) { bestd = d; best = (int)i; }
    }
    return best;
}

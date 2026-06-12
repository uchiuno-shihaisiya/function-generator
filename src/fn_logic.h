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
static const PatternUS PRESET_POSITION_US = { ms2us(8),   ms2us(1)   };
static const PatternUS PRESET_TURN_US     = { ms2us(380), ms2us(190) };
static const PatternUS PRESET_ESS_US      = { ms2us(120), ms2us(120) };
// L/R turn currently share hazard timing; defined separately for independent future tuning
static const PatternUS PRESET_L_TURN_US   = PRESET_TURN_US;
static const PatternUS PRESET_R_TURN_US   = PRESET_TURN_US;

// ---- Step candidates ----
// STEP_MS_LIST: display values in ms (uint16_t to save flash)
static const uint16_t STEP_MS_LIST[] = {1,5,10,50,100,250,500,1000,2000,3000};
// STEP_MS_LIST_US: same candidates pre-converted to us for nearest-index search
static const uint32_t STEP_MS_LIST_US[] = {
    1000UL, 5000UL, 10000UL, 50000UL, 100000UL,
    250000UL, 500000UL, 1000000UL, 2000000UL, 3000000UL
};
static const uint32_t STEP_US_LIST[] = {10,50,100,200,500,1000,2000,5000,10000,50000,100000};

// Common nearest-index search over a us-unit candidate array
static inline int _nearest_index_us(uint32_t step_us, const uint32_t* cands, size_t n) {
    int best = 0;
    uint32_t bestd = UINT32_MAX;
    for (size_t i = 0; i < n; ++i) {
        uint32_t d = (cands[i] > step_us) ? (cands[i] - step_us) : (step_us - cands[i]);
        if (d < bestd) { bestd = d; best = (int)i; }
    }
    return best;
}

static inline int nearest_ms_index(uint32_t step_us) {
    return _nearest_index_us(step_us, STEP_MS_LIST_US,
                             sizeof(STEP_MS_LIST_US)/sizeof(STEP_MS_LIST_US[0]));
}

static inline int nearest_us_index(uint32_t step_us) {
    return _nearest_index_us(step_us, STEP_US_LIST,
                             sizeof(STEP_US_LIST)/sizeof(STEP_US_LIST[0]));
}

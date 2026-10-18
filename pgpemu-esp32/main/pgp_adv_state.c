#include "pgp_adv_state.h"

#define ADV_BACKOFF_BASE_MS 1000
#define ADV_BACKOFF_MAX_MS 30000

adv_action_t adv_decide(adv_state_t* state, bool want) {
    if (want && *state == ADV_OFF) {
        *state = ADV_STARTING;
        return ADV_ACT_START;
    }
    if (!want && (*state == ADV_ON || *state == ADV_STARTING)) {
        *state = ADV_STOPPING;
        return ADV_ACT_STOP;
    }
    // STOPPING waits for its stop event, which re-applies the wanted state
    return ADV_ACT_NONE;
}

uint32_t adv_backoff_ms(uint8_t failures) {
    // 1000 << 5 already exceeds the cap; clamping first avoids shifting by up to 255
    if (failures >= 5) {
        return ADV_BACKOFF_MAX_MS;
    }
    uint32_t ms = ADV_BACKOFF_BASE_MS << failures;
    return ms > ADV_BACKOFF_MAX_MS ? ADV_BACKOFF_MAX_MS : ms;
}

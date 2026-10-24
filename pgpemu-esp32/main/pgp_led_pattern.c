#include "pgp_led_pattern.h"

#define LED_ADV_CYCLE_MS 2000
#define LED_ADV_ON_MS 500
#define LED_PULSE_MS 200
#define LED_CONNECTED_GAP_MS 1200
#define LED_PROV_CYCLE_MS 200
#define LED_PROV_ON_MS 100
#define LED_CRASH_PULSE_MS 500
#define LED_MAX_PULSES 4

led_pattern_t led_pattern_select(bool enabled,
    bool provisioning,
    bool crash_window,
    adv_state_t adv,
    uint8_t connections) {
    if (!enabled) {
        return LED_PAT_OFF;
    }
    if (crash_window) {
        return LED_PAT_CRASHED;
    }
    if (provisioning) {
        return LED_PAT_PROVISIONING;
    }
    if (connections >= 1) {
        return LED_PAT_CONNECTED;
    }
    return adv == ADV_ON ? LED_PAT_ADVERTISING : LED_PAT_OFF;
}

bool led_pattern_level(led_pattern_t p, uint8_t connections, uint32_t t_ms) {
    switch (p) {
    case LED_PAT_ADVERTISING:
        return t_ms % LED_ADV_CYCLE_MS < LED_ADV_ON_MS;
    case LED_PAT_CONNECTED: {
        uint32_t n = connections < 1 ? 1 : connections > LED_MAX_PULSES ? LED_MAX_PULSES : connections;
        uint32_t t = t_ms % (n * 2 * LED_PULSE_MS + LED_CONNECTED_GAP_MS);
        return t < n * 2 * LED_PULSE_MS && (t / LED_PULSE_MS) % 2 == 0;
    }
    case LED_PAT_PROVISIONING:
        return t_ms % LED_PROV_CYCLE_MS < LED_PROV_ON_MS;
    case LED_PAT_CRASHED:
        return t_ms < LED_CRASH_WINDOW_MS && (t_ms / LED_CRASH_PULSE_MS) % 2 == 0;
    default:
        return false;
    }
}

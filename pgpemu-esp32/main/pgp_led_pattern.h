#ifndef PGP_LED_PATTERN_H
#define PGP_LED_PATTERN_H

#include "pgp_adv_state.h"

#include <stdbool.h>
#include <stdint.h>

#define LED_CRASH_WINDOW_MS 3000

typedef enum {
    LED_PAT_OFF = 0,
    LED_PAT_ADVERTISING,
    LED_PAT_CONNECTED,
    LED_PAT_PROVISIONING,
    LED_PAT_CRASHED,
} led_pattern_t;

// First match wins: stealth, crash window, no secrets, connected, advertising.
led_pattern_t led_pattern_select(bool enabled,
    bool provisioning,
    bool crash_window,
    adv_state_t adv,
    uint8_t connections);

// LED level `t_ms` into the pattern cycle. CONNECTED clamps connections to 1..4.
bool led_pattern_level(led_pattern_t p, uint8_t connections, uint32_t t_ms);

#endif /* PGP_LED_PATTERN_H */

#ifndef PGP_ADV_STATE_H
#define PGP_ADV_STATE_H

#include <stdbool.h>
#include <stdint.h>

// values are sent to the Companion App as GET_GLOBAL_SETTINGS byte 4
typedef enum { ADV_OFF = 0, ADV_STARTING = 1, ADV_ON = 2, ADV_STOPPING = 3 } adv_state_t;

typedef enum { ADV_ACT_NONE, ADV_ACT_START, ADV_ACT_STOP } adv_action_t;

// Moves *state toward the wanted radio state and returns the radio call to make.
adv_action_t adv_decide(adv_state_t* state, bool want);

// Delay before the next start attempt after `failures` earlier failures.
uint32_t adv_backoff_ms(uint8_t failures);

#endif /* PGP_ADV_STATE_H */

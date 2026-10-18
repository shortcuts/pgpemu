#ifndef PGP_GAP_H
#define PGP_GAP_H

#include "esp_gap_ble_api.h"
#include "pgp_adv_state.h"

#include <stdbool.h>
#include <stdint.h>

static const uint8_t ADV_CONFIG_FLAG = (1 << 0);
static const uint8_t SCAN_RSP_CONFIG_FLAG = (1 << 1);
extern uint8_t adv_config_done;

// start BT advertising if we have fewer connections than configured
void advertise_if_needed();

// close every connections
void pgp_disconnect();

// create the retry timer; call once before bluetooth init
bool pgp_adv_init(void);

// current radio state, for status reports
adv_state_t pgp_adv_get_state(void);

// tell the state machine a client link came up (the radio stops by itself)
void pgp_adv_on_connect(void);

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);

// True if bda has a live BLE bond (LTK) in the local bond store, i.e. the
// esp_ble stack — not the PGP-protocol session cache — considers this
// device already paired.
bool pgp_gap_is_bonded(esp_bd_addr_t bda);

#endif /* PGP_GAP_H */

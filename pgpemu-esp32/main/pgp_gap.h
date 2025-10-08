#ifndef PGP_GAP_H
#define PGP_GAP_H

#include "esp_gap_ble_api.h"

#include <stdint.h>

static const uint8_t ADV_CONFIG_FLAG = (1 << 0);
static const uint8_t SCAN_RSP_CONFIG_FLAG = (1 << 1);
extern uint8_t adv_config_done;

// start BT advertising if we have fewer connections than configured
void advertise_if_needed();

// close every connections
void pgp_disconnect();

// explicitly start BT advertising
void pgp_advertise();

// explicitly stop BT advertising
void pgp_advertise_stop();

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);

#endif /* PGP_GAP_H */

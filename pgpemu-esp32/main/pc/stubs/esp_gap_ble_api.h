// PC-test stub
#pragma once
#include "esp_gatt_defs.h"
typedef int esp_gap_ble_cb_event_t;
typedef struct esp_ble_gap_cb_param_t esp_ble_gap_cb_param_t;
int esp_ble_gap_disconnect(unsigned char* bd_addr);
int esp_ble_remove_bond_device(unsigned char* bd_addr);

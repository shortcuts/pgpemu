// PC-test stub: the two GATT calls the handshake code makes are counting mocks defined in the test
#pragma once
#include "esp_gatt_defs.h"
int esp_ble_gatts_set_attr_value(uint16_t handle, uint16_t len, const uint8_t* value);
int esp_ble_gatts_send_indicate(esp_gatt_if_t gatts_if,
    uint16_t conn_id,
    uint16_t handle,
    uint16_t len,
    uint8_t* value,
    bool need_confirm);
typedef int esp_gatts_cb_event_t;
typedef struct esp_ble_gatts_cb_param_t esp_ble_gatts_cb_param_t;

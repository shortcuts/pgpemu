// PC-test stub: the two GATT calls the handshake code makes are counting mocks defined in the test
#pragma once
#include "esp_gatt_defs.h"
#include "esp_system.h"
int esp_ble_gatts_set_attr_value(uint16_t handle, uint16_t len, const uint8_t* value);
int esp_ble_gatts_send_indicate(esp_gatt_if_t gatts_if,
    uint16_t conn_id,
    uint16_t handle,
    uint16_t len,
    uint8_t* value,
    bool need_confirm);
typedef int esp_gatts_cb_event_t;
struct esp_ble_gatts_cb_param_t {
    struct {
        uint16_t conn_id;
        uint16_t handle;
        uint16_t len;
        uint8_t* value;
    } write;
    struct {
        struct {
            uint16_t len;
            struct {
                uint8_t uuid128[16];
            } uuid;
        } svc_uuid;
        uint16_t* handles;
        uint16_t num_handle;
    } add_attr_tab;
};
typedef struct esp_ble_gatts_cb_param_t esp_ble_gatts_cb_param_t;
int esp_ble_gatts_create_attr_tab(const esp_gatts_attr_db_t* db, esp_gatt_if_t gatts_if, uint8_t n, uint8_t inst);
int esp_ble_gatts_start_service(uint16_t handle);

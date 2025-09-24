#include "pgp_gap.h"

#include "config_storage.h"
#include "esp_log.h"
#include "log_tags.h"
#include "pgp_handshake_multi.h"
#include "settings.h"

uint8_t adv_config_done = 0;

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr = { 0 },
    .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

void advertise_if_needed() {
    int target_active_connections = get_setting_uint8(&settings.target_active_connections);
    if (get_active_connections() < target_active_connections) {
        pgp_advertise();
    } else {
        ESP_LOGI(
            BT_GAP_TAG, "not advertising again, %d connections reached", target_active_connections);
    }
}

void pgp_advertise() {
    esp_ble_gap_start_advertising(&adv_params);
}

void pgp_advertise_stop() {
    esp_ble_gap_stop_advertising();
}

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0) {
            pgp_advertise();
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0) {
            pgp_advertise();
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        /* advertising start complete event to indicate advertising start successfully or failed
         */
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(BT_GAP_TAG, "advertising start failed");
        } else {
            ESP_LOGI(BT_GAP_TAG, "advertising start successful");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(BT_GAP_TAG, "advertising stop failed");
        } else {
            ESP_LOGI(BT_GAP_TAG, "advertising stop successful");
        }
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        ESP_LOGI(BT_GAP_TAG, "New bonding done with device %d", esp_ble_get_bond_device_num());

        read_stored_settings(true);

        break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGI(BT_GAP_TAG, "Bond not found");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(BT_GAP_TAG,
            "update connection params status=%d, min_int=%d, max_int=%d, conn_int=%d, "
            "latency=%d, timeout=%d",
            param->update_conn_params.status,
            param->update_conn_params.min_int,
            param->update_conn_params.max_int,
            param->update_conn_params.conn_int,
            param->update_conn_params.latency,
            param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

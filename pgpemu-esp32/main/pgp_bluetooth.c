#include "pgp_bluetooth.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "log_tags.h"
#include "pgp_gap.h"
#include "pgp_gatts.h"
#include "pgp_handshake_multi.h"
#include "secrets.h"

#include <string.h>

static const uint16_t ESP_APP_ID = 0x55;

// in ESP32, bt_mac is base_mac + 2
static uint8_t mac[6];
uint8_t bt_mac[6];

bool init_bluetooth() {
    init_handshake_multi();

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // set mac address for pgp clone device
    memcpy(bt_mac, PGP_MAC, 6);
    memcpy(mac, PGP_MAC, 6);

    mac[5] -= 2;  // TODO: check what happens if last byte is 0 or 1

    esp_base_mac_addr_set(mac);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(BT_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return false;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(BT_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return false;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(BT_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return false;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(BT_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return false;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret) {
        ESP_LOGE(BT_TAG, "gatts register error, error code = %x", ret);
        return false;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(BT_TAG, "gap register error, error code = %x", ret);
        return false;
    }

    ret = esp_ble_gatts_app_register(ESP_APP_ID);
    if (ret) {
        ESP_LOGE(BT_TAG, "gatts app register error, error code = %x", ret);
        return false;
    }

    /* set the security iocap & auth_req & key size & init key response key parameters to the
     * stack*/
    esp_ble_auth_req_t auth_req =
        ESP_LE_AUTH_BOND;                      // bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;  // set the IO capability to No output No input
    uint8_t key_size = 16;                     // the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the
    master should distribut to you, and the response key means which key you can distribut to the
    Master; If your BLE device act as a master, the response key means you hope which types of key
    of the slave should distribut to you, and the init key means which key you can distribut to the
    slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret) {
        ESP_LOGE(BT_TAG, "set local MTU failed, error code = %x", local_mtu_ret);
    }

    return true;
}

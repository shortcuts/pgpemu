#include "pgp_gap.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "led_output.h"
#include "log_tags.h"
#include "pgp_adv_state.h"
#include "pgp_handshake_multi.h"
#include "settings.h"

#include <string.h>

uint8_t adv_config_done = 0;

static adv_state_t adv_state = ADV_OFF;
static uint8_t adv_failures = 0;
// guards the two vars above; never held across an esp_ble_gap_* call
static portMUX_TYPE adv_lock = portMUX_INITIALIZER_UNLOCKED;
static esp_timer_handle_t adv_retry_timer = NULL;

// review with
// https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_security_client/main/example_ble_sec_gattc_demo.c
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

static void adv_retry_cb(void* arg) {
    (void)arg;
    advertise_if_needed();
}

bool pgp_adv_init(void) {
    esp_timer_create_args_t args = { .callback = adv_retry_cb, .name = "adv_retry" };
    return esp_timer_create(&args, &adv_retry_timer) == ESP_OK;
}

adv_state_t pgp_adv_get_state(void) {
    portENTER_CRITICAL(&adv_lock);
    adv_state_t state = adv_state;
    portEXIT_CRITICAL(&adv_lock);
    return state;
}

// Moves the state under the lock; the LED mirrors the radio, so it follows only ON and OFF.
static void adv_set_state(adv_state_t state) {
    portENTER_CRITICAL(&adv_lock);
    adv_state = state;
    portEXIT_CRITICAL(&adv_lock);
    set_led_advertising(state == ADV_ON);
}

static void adv_start_failed(void) {
    portENTER_CRITICAL(&adv_lock);
    uint8_t failures = adv_failures;
    if (adv_failures < UINT8_MAX) {
        adv_failures++;
    }
    portEXIT_CRITICAL(&adv_lock);
    adv_set_state(ADV_OFF);

    uint32_t delay_ms = adv_backoff_ms(failures);
    ESP_LOGW(BT_GAP_TAG, "advertising start failed, retry in %lu ms", (unsigned long)delay_ms);
    // INVALID_STATE means a retry is already pending
    if (adv_retry_timer) {
        esp_timer_start_once(adv_retry_timer, (uint64_t)delay_ms * 1000);
    }
}

void advertise_if_needed() {
    int target_active_connections = get_setting_uint8(&global_settings.target_active_connections);
    bool enabled = get_setting(&global_settings.advertising_enabled);
    bool want = enabled && get_active_connections() < target_active_connections;
    if (enabled && !want) {
        ESP_LOGI(BT_GAP_TAG, "not advertising again, %d connections reached", target_active_connections);
    }

    portENTER_CRITICAL(&adv_lock);
    adv_action_t action = adv_decide(&adv_state, want);
    portEXIT_CRITICAL(&adv_lock);

    if (action == ADV_ACT_START) {
        if (esp_ble_gap_start_advertising(&adv_params) != ESP_OK) {
            adv_start_failed();
        }
    } else if (action == ADV_ACT_STOP) {
        // no stop event follows a rejected call
        if (esp_ble_gap_stop_advertising() != ESP_OK) {
            adv_set_state(ADV_OFF);
        }
    }
}

// Bluedroid stops connectable advertising when a link comes up and sends no stop event.
void pgp_adv_on_connect(void) {
    adv_set_state(ADV_OFF);
}

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0) {
            advertise_if_needed();
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0) {
            advertise_if_needed();
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        // a stop or connect may have moved the state while the start was in flight
        if (pgp_adv_get_state() == ADV_STARTING) {
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                adv_start_failed();
            } else {
                ESP_LOGI(BT_GAP_TAG, "advertising start successful");
                portENTER_CRITICAL(&adv_lock);
                adv_failures = 0;
                portEXIT_CRITICAL(&adv_lock);
                adv_set_state(ADV_ON);
            }
        }
        advertise_if_needed();
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(BT_GAP_TAG, "advertising stop failed");
        } else {
            ESP_LOGI(BT_GAP_TAG, "advertising stop successful");
        }
        // a failed stop means the radio was already off
        adv_set_state(ADV_OFF);
        advertise_if_needed();
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
        ESP_LOGI(BT_GAP_TAG,
            "authentication completed: success=%d, device_count=%d",
            param->ble_security.auth_cmpl.success,
            esp_ble_get_bond_device_num());
        client_state_t* auth_entry = get_client_state_entry_by_bda(param->ble_security.auth_cmpl.bd_addr);
        if (param->ble_security.auth_cmpl.success) {
            if (auth_entry) {
                auth_entry->auth_succeeded = true;
            }
        } else if (auth_entry && auth_entry->auth_succeeded) {
            // This connection already authenticated successfully once, so both sides still
            // consider it live. A later auth failure here is a transient hiccup (e.g. a
            // spurious re-encryption retry), not a stale bond. Removing our bond now would
            // break a connection that still works instead of fixing anything.
            ESP_LOGW(BT_GAP_TAG, "auth failed on an already-authenticated connection, leaving bond in place");
        } else {
            // Leaving the stale bond in place makes the peer's Android bond (recreated after
            // it forgets/re-pairs) permanently mismatch ours, so every future write on an
            // encrypted characteristic keeps failing with GATT_INSUF_* until reboot. Drop our
            // side too so the next pairing attempt starts clean.
            esp_ble_remove_bond_device(param->ble_security.auth_cmpl.bd_addr);
        }
        break;
    }
    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGI(BT_GAP_TAG, "security request received, accepting");
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    // CONNECTION STATUS
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

bool pgp_gap_is_bonded(esp_bd_addr_t bda) {
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) {
        return false;
    }
    // Bluedroid's bond store is capped at CONFIG_BT_SMP_MAX_BONDS entries
    // (BTM_SEC_MAX_DEVICE_RECORDS), not CONFIG_BT_ACL_CONNECTIONS — bonds
    // persist across reconnects and can outnumber concurrent connections.
    if (dev_num > CONFIG_BT_SMP_MAX_BONDS) {
        dev_num = CONFIG_BT_SMP_MAX_BONDS;
    }

    // static: 15 * sizeof(esp_ble_bond_dev_t) is too big to put on this
    // function's stack frame — this runs in BTC_TASK on every GATTS connect
    // event, the hottest path in the firmware. Safe as static: Bluedroid
    // dispatches GATTS callbacks serially from one task, so no concurrent
    // callers.
    static esp_ble_bond_dev_t dev_list[CONFIG_BT_SMP_MAX_BONDS];

    bool found = false;
    if (esp_ble_get_bond_device_list(&dev_num, dev_list) == ESP_OK) {
        for (int i = 0; i < dev_num; i++) {
            if (memcmp(dev_list[i].bd_addr, bda, sizeof(esp_bd_addr_t)) == 0) {
                found = true;
                break;
            }
        }
    }
    return found;
}

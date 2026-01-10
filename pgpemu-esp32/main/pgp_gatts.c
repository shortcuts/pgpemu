#include "pgp_gatts.h"

#include "config_storage.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "log_tags.h"
#include "nvs_flash.h"
#include "pgp_gap.h"
#include "pgp_gatts_debug.h"
#include "pgp_handshake.h"
#include "pgp_handshake_multi.h"
#include "pgp_led_handler.h"
#include "secrets.h"
#include "settings.h"

#include <stdlib.h>
#include <string.h>

#define PROFILE_NUM 1
#define PROFILE_APP_IDX 0

#if 0
#define PGP_DEVICE_NAME "Pokemon PBP"
#define SVC_INST_ID 0
#define BATTERY_INST_ID 1
#define LED_BUTTON_INST_ID 2
#else
static const char* PGP_DEVICE_NAME = "Pokemon GO Plus";
static const uint8_t BATTERY_INST_ID = 0;
static const uint8_t LED_BUTTON_INST_ID = 1;
static const uint8_t CERT_INST_ID = 2;
#endif

/* The max length of characteristic value. When the gatt client write or prepare write,
 *  the data length must be less than MAX_VALUE_LENGTH.
 */
#define MAX_VALUE_LENGTH 500
#define PREPARE_BUF_MAX_SIZE 1024
#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

uint16_t battery_handle_table[BATTERY_LAST_IDX];
uint16_t led_button_handle_table[LED_BUTTON_LAST_IDX];
uint16_t certificate_handle_table[CERT_LAST_IDX];

static uint8_t raw_adv_data[] = {
    /* flags */
    0x02,
    0x01,
    0x06,
    /* device name */
    0x10,
    0x09,
    'P',
    'o',
    'k',
    'e',
    'm',
    'o',
    'n',
    ' ',
    'G',
    'O',
    ' ',
    'P',
    'l',
    'u',
    's',
    // 0x0c, 0x09, 'P', 'o', 'k', 'e', 'm', 'o', 'n', ' ', 'P', 'B', 'P',
    0x06,
    0x20,
    0x62,
    0x04,
    0xC5,
    0x21,
    0x00
};
static uint8_t raw_scan_rsp_data[] = {
    /* flags */
    0x02,
    0x01,
    0x06,
    /* tx power */
    0x02,
    0x0a,
    0xeb,
    /* service uuid */
    0x03,
    0x03,
    0xFF,
    0x00
};

typedef struct {
    uint16_t handle;
    uint8_t* prepare_buf;
    int prepare_len;
} prepare_type_env_t;

static prepare_type_env_t prepare_write_env;

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
    esp_gatt_if_t gatts_if,
    esp_ble_gatts_cb_param_t* param);
static void pgp_prepare_write_event_env(esp_gatt_if_t gatts_if,
    prepare_type_env_t* prepare_write_env,
    esp_ble_gatts_cb_param_t* param);
static void pgp_exec_write_event_env(esp_gatt_if_t gatts_if,
    prepare_type_env_t* prepare_write_env,
    esp_ble_gatts_cb_param_t* param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned
 * by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst pgp_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_IDX] =
        {
            .gatts_cb = gatts_profile_event_handler,
            .gatts_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
            .app_id = 0,
            .conn_id = 0,
            .service_handle = 0,
            .char_handle = 0,
            .descr_handle = 0,
        },
};

/* Service */
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;

static const uint8_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t dummy_value[2] = { 0x00, 0x00 };
static uint8_t cert_buffer[378] = { 0 };

static const uint16_t GATTS_SERVICE_UUID_BATTERY = 0x180f;
static const uint16_t GATTS_CHAR_UUID_BATTERY_LEVEL = 0x2a19;
static const uint8_t battery_char_value[1] = { 0x60 };

static const esp_gatts_attr_db_t gatt_db_battery[BATTERY_LAST_IDX] = {

    // Service Declaration
    [IDX_BATTERY_SVC] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&primary_service_uuid,
            ESP_GATT_PERM_READ,
            sizeof(uint16_t),
            sizeof(GATTS_SERVICE_UUID_BATTERY),
            (uint8_t*)&GATTS_SERVICE_UUID_BATTERY } },

    /* Characteristic Declaration */
    [IDX_CHAR_BATTERY_LEVEL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&character_declaration_uuid,
            ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE,
            CHAR_DECLARATION_SIZE,
            (uint8_t*)&char_prop_read_notify } },
    /* Characteristic Value */
    [IDX_CHAR_BATTERY_LEVEL_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&GATTS_CHAR_UUID_BATTERY_LEVEL,
            ESP_GATT_PERM_READ,
            MAX_VALUE_LENGTH,
            sizeof(battery_char_value),
            (uint8_t*)battery_char_value } },

    /* Client Characteristic Configuration Descriptor */
    [IDX_CHAR_BATTERY_LEVEL_CFG] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&character_client_config_uuid,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(uint16_t),
            sizeof(dummy_value),
            (uint8_t*)dummy_value } },
};

// uuid in reversed order
uint8_t GATTS_SERVICE_UUID_LED_BUTTON[ESP_UUID_LEN_128] = { 0xeb,
    0x9a,
    0x93,
    0xb9,
    0xb5,
    0x82,
    0x4c,
    0x5c,
    0xa3,
    0x63,
    0xcb,
    0x67,
    0x62,
    0x4,
    0xc5,
    0x21 };
uint8_t GATTS_CHAR_UUID_LED[ESP_UUID_LEN_128] = { 0xec,
    0x9a,
    0x93,
    0xb9,
    0xb5,
    0x82,
    0x4c,
    0x5c,
    0xa3,
    0x63,
    0xcb,
    0x67,
    0x62,
    0x4,
    0xc5,
    0x21 };
uint8_t GATTS_CHAR_UUID_BUTTON[ESP_UUID_LEN_128] = { 0xed,
    0x9a,
    0x93,
    0xb9,
    0xb5,
    0x82,
    0x4c,
    0x5c,
    0xa3,
    0x63,
    0xcb,
    0x67,
    0x62,
    0x4,
    0xc5,
    0x21 };
uint8_t GATTS_CHAR_UUID_UNKNOWN[ESP_UUID_LEN_128] = { 0xee,
    0x9a,
    0x93,
    0xb9,
    0xb5,
    0x82,
    0x4c,
    0x5c,
    0xa3,
    0x63,
    0xcb,
    0x67,
    0x62,
    0x4,
    0xc5,
    0x21 };
uint8_t GATTS_CHAR_UUID_UPDATE_REQUEST[ESP_UUID_LEN_128] = { 0xef,
    0x9a,
    0x93,
    0xb9,
    0xb5,
    0x82,
    0x4c,
    0x5c,
    0xa3,
    0x63,
    0xcb,
    0x67,
    0x62,
    0x4,
    0xc5,
    0x21 };
uint8_t GATTS_CHAR_UUID_FW_VERSION[ESP_UUID_LEN_128] = { 0xf0,
    0x9a,
    0x93,
    0xb9,
    0xb5,
    0x82,
    0x4c,
    0x5c,
    0xa3,
    0x63,
    0xcb,
    0x67,
    0x62,
    0x4,
    0xc5,
    0x21 };

static const uint8_t led_char_value[100] = { 0 };

static const uint8_t button_value[2] = { 0 };

uint8_t GATTS_SERVICE_UUID_CERTIFICATE[ESP_UUID_LEN_128] = { 0x37,
    0x8e,
    0xd,
    0xef,
    0x8e,
    0x8b,
    0x7f,
    0xab,
    0x33,
    0x44,
    0x89,
    0x5b,
    0x9,
    0x77,
    0xe8,
    0xbb };
uint8_t GATTS_CHAR_UUID_CENTRAL_TO_SFIDA[ESP_UUID_LEN_128] = { 0x38,
    0x8e,
    0xd,
    0xef,
    0x8e,
    0x8b,
    0x7f,
    0xab,
    0x33,
    0x44,
    0x89,
    0x5b,
    0x9,
    0x77,
    0xe8,
    0xbb };
uint8_t GATTS_CHAR_UUID_SFIDA_COMMANDS[ESP_UUID_LEN_128] = { 0x39,
    0x8e,
    0xd,
    0xef,
    0x8e,
    0x8b,
    0x7f,
    0xab,
    0x33,
    0x44,
    0x89,
    0x5b,
    0x9,
    0x77,
    0xe8,
    0xbb };
uint8_t GATTS_CHAR_UUID_SFIDA_TO_CENTRAL[ESP_UUID_LEN_128] = { 0x3a,
    0x8e,
    0xd,
    0xef,
    0x8e,
    0x8b,
    0x7f,
    0xab,
    0x33,
    0x44,
    0x89,
    0x5b,
    0x9,
    0x77,
    0xe8,
    0xbb };
/*
  {ESP_GATT_AUTO_RSP/ESP_GATT_RSP_BY_APP},
  {uuid_length, uuid_val, attribute permission, max length of element, current length of element,
  element value array}
*/

static const esp_gatts_attr_db_t gatt_db_led_button[LED_BUTTON_LAST_IDX] = {
    // Service Declaration
    [IDX_LED_BUTTON_SVC] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&primary_service_uuid,
            ESP_GATT_PERM_READ,
            ESP_UUID_LEN_128,
            ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_SERVICE_UUID_LED_BUTTON } },

    /* Characteristic Declaration */
    [IDX_CHAR_LED] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&character_declaration_uuid,
            ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE,
            CHAR_DECLARATION_SIZE,
            (uint8_t*)&char_prop_write } },
    [IDX_CHAR_LED_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_CHAR_UUID_LED,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            MAX_VALUE_LENGTH,
            sizeof(led_char_value),
            (uint8_t*)led_char_value } },

    [IDX_CHAR_BUTTON] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&character_declaration_uuid,
            ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE,
            CHAR_DECLARATION_SIZE,
            (uint8_t*)&char_prop_notify } },
    [IDX_CHAR_BUTTON_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_CHAR_UUID_BUTTON,
            ESP_GATT_PERM_READ,
            MAX_VALUE_LENGTH,
            sizeof(button_value),
            (uint8_t*)button_value } },

    /* Client Characteristic Configuration Descriptor */
    [IDX_CHAR_BUTTON_CFG] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&character_client_config_uuid,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(uint16_t),
            sizeof(dummy_value),
            (uint8_t*)dummy_value } },

    /* Characteristic Declaration */
    [IDX_CHAR_UNKNOWN] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&character_declaration_uuid,
            ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE,
            CHAR_DECLARATION_SIZE,
            (uint8_t*)&char_prop_write } },
    [IDX_CHAR_UNKNOWN_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_CHAR_UUID_UNKNOWN,
            ESP_GATT_PERM_READ,
            MAX_VALUE_LENGTH,
            sizeof(led_char_value),
            (uint8_t*)led_char_value } },

    /* Characteristic Declaration */
    [IDX_CHAR_UPDATE_REQUEST] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&character_declaration_uuid,
            ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE,
            CHAR_DECLARATION_SIZE,
            (uint8_t*)&char_prop_write } },
    [IDX_CHAR_UPDATE_REQUEST_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_CHAR_UUID_UPDATE_REQUEST,
            ESP_GATT_PERM_READ,
            MAX_VALUE_LENGTH,
            sizeof(led_char_value),
            (uint8_t*)led_char_value } },

    /* Characteristic Declaration */
    [IDX_CHAR_FW_VERSION] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&character_declaration_uuid,
            ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE,
            CHAR_DECLARATION_SIZE,
            (uint8_t*)&char_prop_read } },
    [IDX_CHAR_FW_VERSION_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_CHAR_UUID_FW_VERSION,
            ESP_GATT_PERM_READ,
            MAX_VALUE_LENGTH,
            sizeof(led_char_value),
            (uint8_t*)led_char_value } },

};

static const esp_gatts_attr_db_t gatt_db_certificate[CERT_LAST_IDX] = {
    // Service Declaration
    [IDX_CERT_SVC] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&primary_service_uuid,
            ESP_GATT_PERM_READ,
            ESP_UUID_LEN_128,
            ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_SERVICE_UUID_CERTIFICATE } },

    /* Characteristic Declaration */
    [IDX_CHAR_CENTRAL_TO_SFIDA] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&character_declaration_uuid,
            ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE,
            CHAR_DECLARATION_SIZE,
            (uint8_t*)&char_prop_write } },
    [IDX_CHAR_CENTRAL_TO_SFIDA_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_CHAR_UUID_CENTRAL_TO_SFIDA,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            MAX_VALUE_LENGTH,
            sizeof(led_char_value),
            (uint8_t*)led_char_value } },

    [IDX_CHAR_SFIDA_COMMANDS] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&character_declaration_uuid,
            ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE,
            CHAR_DECLARATION_SIZE,
            (uint8_t*)&char_prop_notify } },
    [IDX_CHAR_SFIDA_COMMANDS_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_CHAR_UUID_SFIDA_COMMANDS,
            ESP_GATT_PERM_READ,
            MAX_VALUE_LENGTH,
            sizeof(led_char_value),
            (uint8_t*)led_char_value } },

    /* Client Characteristic Configuration Descriptor */
    [IDX_CHAR_SFIDA_COMMANDS_CFG] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&character_client_config_uuid,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(uint16_t),
            sizeof(dummy_value),
            (uint8_t*)dummy_value } },

    /* Characteristic Declaration */
    [IDX_CHAR_SFIDA_TO_CENTRAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&character_declaration_uuid,
            ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE,
            CHAR_DECLARATION_SIZE,
            (uint8_t*)&char_prop_read } },
    [IDX_CHAR_SFIDA_TO_CENTRAL_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_CHAR_UUID_SFIDA_TO_CENTRAL,
            ESP_GATT_PERM_READ,
            MAX_VALUE_LENGTH,
            sizeof(cert_buffer),
            (uint8_t*)cert_buffer } },
};

void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param) {
    ESP_LOGD(BT_GATTS_TAG, "%s: received event %d", __func__, event);
    switch (event) {
    case ESP_GATTS_REG_EVT: {
        esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(PGP_DEVICE_NAME);
        if (set_dev_name_ret) {
            ESP_LOGE(BT_GATTS_TAG, "set device name failed, error code = %x", set_dev_name_ret);
        }
        esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
        if (raw_adv_ret) {
            ESP_LOGE(BT_GATTS_TAG, "config raw adv data failed, error code = %x ", raw_adv_ret);
        }
        adv_config_done |= ADV_CONFIG_FLAG;

        esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
        if (raw_scan_ret) {
            ESP_LOGE(BT_GATTS_TAG, "config raw scan rsp data failed, error code = %x", raw_scan_ret);
        }
        adv_config_done |= SCAN_RSP_CONFIG_FLAG;
        /* esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if,
         * HRS_IDX_NB, SVC_INST_ID); */
        /* if (create_attr_ret){ */
        /*     ESP_LOGE(GATTS_TABLE_TAG, "create attr table failed, error code = %x",
         * create_attr_ret); */
        /* } */
        esp_err_t create_attr_ret =
            esp_ble_gatts_create_attr_tab(gatt_db_battery, gatts_if, BATTERY_LAST_IDX, BATTERY_INST_ID);
        if (create_attr_ret) {
            ESP_LOGE(BT_GATTS_TAG, "create attr table for battery failed, error code = %x", create_attr_ret);
        }

        create_attr_ret =
            esp_ble_gatts_create_attr_tab(gatt_db_led_button, gatts_if, LED_BUTTON_LAST_IDX, LED_BUTTON_INST_ID);
        if (create_attr_ret) {
            ESP_LOGE(BT_GATTS_TAG, "create attr table for led button failed, error code = %x", create_attr_ret);
        }

        create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db_certificate, gatts_if, CERT_LAST_IDX, CERT_INST_ID);
        if (create_attr_ret) {
            ESP_LOGE(BT_GATTS_TAG, "create attr table for cert  failed, error code = %x", create_attr_ret);
        }
    } break;
    case ESP_GATTS_READ_EVT:
        ESP_LOGD(BT_GATTS_TAG,
            "[%d] ESP_GATTS_READ_EVT: %s",
            param->read.conn_id,
            char_name_from_handle(param->read.handle));
        if (pgp_get_handshake_state(param->read.conn_id) == 1) {
            if (esp_log_level_get(BT_GATTS_TAG) >= ESP_LOG_VERBOSE) {
                ESP_LOGV(BT_GATTS_TAG, "DATA SENT TO APP");
                if (gatt_db_certificate[IDX_CHAR_SFIDA_TO_CENTRAL_VAL].att_desc.value) {
                    ESP_LOG_BUFFER_HEX(
                        BT_GATTS_TAG, gatt_db_certificate[IDX_CHAR_SFIDA_TO_CENTRAL_VAL].att_desc.value, 52);
                }
            }
        }
        break;
    case ESP_GATTS_WRITE_EVT:
        ESP_LOGD(BT_GATTS_TAG,
            "[%d] ESP_GATTS_WRITE_EVT: %s",
            param->write.conn_id,
            char_name_from_handle(param->write.handle));
        if (!param->write.is_prep) {
            // the data length of gattc write  must be less than MAX_VALUE_LENGTH.
            ESP_LOGD(BT_GATTS_TAG, "GATT_WRITE_EVT handle %d, value len %d", param->write.handle, param->write.len);
            if (esp_log_level_get(BT_GATTS_TAG) >= ESP_LOG_VERBOSE) {
                ESP_LOGV(BT_GATTS_TAG, "DATA FROM APP");
                ESP_LOG_BUFFER_HEX(BT_GATTS_TAG, param->write.value, param->write.len);
            }

            ESP_LOGD(BT_GATTS_TAG,
                "idx %d tabl %d",
                IDX_CHAR_SFIDA_COMMANDS_CFG,
                certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_CFG]);
            ESP_LOGD(BT_GATTS_TAG,
                "idx %d tabl %d",
                IDX_CHAR_CENTRAL_TO_SFIDA_VAL,
                certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_CFG]);

            if (certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_CFG] == param->write.handle) {
                uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
                handle_pgp_handshake_first(gatts_if, descr_value, param->write.conn_id);
            } else if (certificate_handle_table[IDX_CHAR_CENTRAL_TO_SFIDA_VAL] == param->write.handle) {
                handle_pgp_handshake_second(gatts_if, param->write.value, param->write.len, param->write.conn_id);
            } else if (led_button_handle_table[IDX_CHAR_LED_VAL] == param->write.handle) {
                handle_led_notify_from_app(gatts_if, param->write.conn_id, param->write.value);
                return;
            } else if (led_button_handle_table[IDX_CHAR_BUTTON_CFG] == param->write.handle) {
                ESP_LOGW(BT_GATTS_TAG, "%s: unhandled CHAR_BUTTON_CFG", __func__);
                return;
            } else {
                ESP_LOGW(BT_GATTS_TAG, "%s: unknown handle %d", __func__, param->write.handle);
                // dump all tables
                if (esp_log_level_get(BT_GATTS_TAG) >= ESP_LOG_DEBUG) {
                    for (int i = 0; i < BATTERY_LAST_IDX; i++) {
                        ESP_LOGD(BT_GATTS_TAG,
                            "handle: batt %d=%s",
                            battery_handle_table[i],
                            char_name_from_handle(battery_handle_table[i]));
                    }
                    for (int i = 0; i < LED_BUTTON_LAST_IDX; i++) {
                        ESP_LOGD(BT_GATTS_TAG,
                            "handle: ledb %d=%s",
                            led_button_handle_table[i],
                            char_name_from_handle(led_button_handle_table[i]));
                    }
                    for (int i = 0; i < CERT_LAST_IDX; i++) {
                        ESP_LOGD(BT_GATTS_TAG,
                            "handle: cert %d=%s",
                            certificate_handle_table[i],
                            char_name_from_handle(certificate_handle_table[i]));
                    }
                }
            }

            /* send response when param->write.need_rsp is true*/
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
        } else {
            /* handle prepare write */
            pgp_prepare_write_event_env(gatts_if, &prepare_write_env, param);
        }
        break;
    case ESP_GATTS_EXEC_WRITE_EVT:
        // the length of gattc prapare write data must be less than MAX_VALUE_LENGTH.
        ESP_LOGD(BT_GATTS_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
        pgp_exec_write_event_env(gatts_if, &prepare_write_env, param);
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGD(BT_GATTS_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        break;
    case ESP_GATTS_CONF_EVT:
        ESP_LOGD(BT_GATTS_TAG, "ESP_GATTS_CONF_EVT, status = %d", param->conf.status);
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGD(BT_GATTS_TAG,
            "SERVICE_START_EVT, status %d, service_handle %d",
            param->start.status,
            param->start.service_handle);
        break;
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGD(BT_GATTS_TAG,
            "[%d] ESP_GATTS_CONNECT_EVT, mac=%02x:%02x:%02x:%02x:%02x:%02x",
            param->connect.conn_id,
            param->connect.remote_bda[0],
            param->connect.remote_bda[1],
            param->connect.remote_bda[2],
            param->connect.remote_bda[3],
            param->connect.remote_bda[4],
            param->connect.remote_bda[5]);

        esp_ble_conn_update_params_t conn_params = {
            .min_int = 0,
            .max_int = 0,
            .latency = 0,
            .timeout = 0,
        };
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the IOS system, please reference the apple official documents about the ble
         * connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x20;  // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10;  // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;   // timeout = 400*10ms = 4000ms
        // start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);

        set_remote_bda(param->connect.conn_id, conn_params.bda);

        // Load device settings for this connection
        client_state_t* client_entry = get_client_state_entry(param->connect.conn_id);
        if (client_entry) {
            // Allocate memory for device settings
            client_entry->settings = (DeviceSettings*)malloc(sizeof(DeviceSettings));
            if (client_entry->settings) {
                memset(client_entry->settings, 0, sizeof(DeviceSettings));
                read_stored_device_settings(conn_params.bda, client_entry->settings);
                ESP_LOGI(BT_GATTS_TAG, "[%d] device settings loaded", param->connect.conn_id);
            } else {
                ESP_LOGE(BT_GATTS_TAG, "[%d] failed to allocate device settings", param->connect.conn_id);
            }
        }

        // Only request encryption for new devices. For reconnections with cached session,
        // let the BLE stack handle encryption silently using the existing bond.
        // Check if we have cached session keys (reconnection)
        if (!has_cached_session(conn_params.bda)) {
            // New device: request encryption which will trigger pairing on Android
            ESP_LOGD(BT_GATTS_TAG, "[%d] new device, requesting encryption", param->connect.conn_id);
            esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
        } else {
            // Reconnection: let BLE stack handle encryption silently using existing bond
            ESP_LOGD(BT_GATTS_TAG, "[%d] cached session found, skipping encryption request", param->connect.conn_id);
        }
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        pgp_handshake_disconnect(param->disconnect.conn_id);

        ESP_LOGW(BT_GATTS_TAG, "[%d/%d] disconnected", param->disconnect.conn_id, get_active_connections());

        switch (param->disconnect.reason) {
        case ESP_GATT_CONN_TERMINATE_PEER_USER:
        case ESP_GATT_CONN_TERMINATE_LOCAL_HOST:
            ESP_LOGI(BT_GATTS_TAG, "[%d] disconect requested %d", param->disconnect.conn_id, param->disconnect.reason);
            break;
        case ESP_GATT_CONN_UNKNOWN:
        case ESP_GATT_CONN_L2C_FAILURE:
        case ESP_GATT_CONN_TIMEOUT:
        case ESP_GATT_CONN_FAIL_ESTABLISH:
        case ESP_GATT_CONN_LMP_TIMEOUT:
        case ESP_GATT_CONN_CONN_CANCEL:
        case ESP_GATT_CONN_NONE:
        default:
            ESP_LOGW(
                BT_GATTS_TAG, "[%d] disconnect error reason %d", param->disconnect.conn_id, param->disconnect.reason);
        }

        advertise_if_needed();
        break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
        if (param->add_attr_tab.status != ESP_GATT_OK) {
            ESP_LOGE(BT_GATTS_TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
        }
        int found = 0;
        if (param->add_attr_tab.svc_uuid.len == ESP_UUID_LEN_16) {
            if (param->add_attr_tab.svc_uuid.uuid.uuid16 == GATTS_SERVICE_UUID_BATTERY) {
                memcpy(battery_handle_table, param->add_attr_tab.handles, sizeof(battery_handle_table));
                esp_ble_gatts_start_service(battery_handle_table[IDX_BATTERY_SVC]);
                ESP_LOGD(BT_GATTS_TAG,
                    "create battery attribute table success, handle = %d",
                    param->add_attr_tab.num_handle);
                found = 1;
            }
        }

        if (param->add_attr_tab.svc_uuid.len == ESP_UUID_LEN_128) {
            if (memcmp(param->add_attr_tab.svc_uuid.uuid.uuid128, GATTS_SERVICE_UUID_LED_BUTTON, ESP_UUID_LEN_128)
                == 0) {
                memcpy(led_button_handle_table, param->add_attr_tab.handles, sizeof(led_button_handle_table));
                esp_err_t response_err = esp_ble_gatts_start_service(led_button_handle_table[IDX_LED_BUTTON_SVC]);
                if (response_err != ESP_OK) {
                    ESP_LOGE(BT_GATTS_TAG, "failed starting service: %d", response_err);
                }
                ESP_LOGD(BT_GATTS_TAG,
                    "create led button attribute table success, handle = %d",
                    param->add_attr_tab.num_handle);
                found = 1;
            }
            if (memcmp(param->add_attr_tab.svc_uuid.uuid.uuid128, GATTS_SERVICE_UUID_CERTIFICATE, ESP_UUID_LEN_128)
                == 0) {
                memcpy(certificate_handle_table, param->add_attr_tab.handles, sizeof(certificate_handle_table));
                esp_err_t response_err = esp_ble_gatts_start_service(certificate_handle_table[IDX_CERT_SVC]);
                if (response_err != ESP_OK) {
                    ESP_LOGE(BT_GATTS_TAG, "failed starting service: %d", response_err);
                }
                ESP_LOGD(
                    BT_GATTS_TAG, "create cert attribute table success, handle = %d", param->add_attr_tab.num_handle);
                found = 1;
            }
        }

        if (!found) {
            ESP_LOGE(BT_GATTS_TAG, "service not found");
        }
        break;
    }
    case ESP_GATTS_SET_ATTR_VAL_EVT:
        break;
    default:
        ESP_LOGW(BT_GATTS_TAG, "%s: unhandled event %d", __func__, event);
        break;
    }
}

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param) {
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            pgp_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
        } else {
            ESP_LOGE(BT_GATTS_TAG, "reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    }
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb
             * function */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == pgp_profile_tab[idx].gatts_if) {
                if (pgp_profile_tab[idx].gatts_cb) {
                    pgp_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

void pgp_prepare_write_event_env(esp_gatt_if_t gatts_if,
    prepare_type_env_t* prepare_write_env,
    esp_ble_gatts_cb_param_t* param) {
    ESP_LOGD(BT_GATTS_TAG, "prepare write, handle=%d, value len=%d", param->write.handle, param->write.len);
    esp_gatt_status_t status = ESP_GATT_OK;
    if (prepare_write_env->prepare_buf == NULL) {
        prepare_write_env->handle = param->write.handle;
        prepare_write_env->prepare_buf = (uint8_t*)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
        prepare_write_env->prepare_len = 0;
        if (prepare_write_env->prepare_buf == NULL) {
            ESP_LOGE(BT_GATTS_TAG, "%s, Gatt_server prep no mem", __func__);
            status = ESP_GATT_NO_RESOURCES;
        }
    } else {
        if (param->write.offset > PREPARE_BUF_MAX_SIZE) {
            status = ESP_GATT_INVALID_OFFSET;
        } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
            status = ESP_GATT_INVALID_ATTR_LEN;
        }
    }
    /*send response when param->write.need_rsp is true */
    if (param->write.need_rsp) {
        ESP_LOGE(BT_GATTS_TAG, "need_rsp not implemented");
#if 0
        esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
        if (gatt_rsp != NULL)
        {

            /*
                gatt_rsp->attr_value.len = param->write.len;
                gatt_rsp->attr_value.handle = param->write.handle;
                gatt_rsp->attr_value.offset = param->write.offset;
                gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
                memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
                esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
                if (response_err != ESP_OK){
                   ESP_LOGE(GATTS_TABLE_TAG, "Send response error");
                }
                free(gatt_rsp);
            */
        }
        else
        {
            ESP_LOGE(BT_GATTS_TAG, "%s, malloc failed", __func__);
        }
#endif
    }
    if (status != ESP_GATT_OK) {
        return;
    }
    memcpy(prepare_write_env->prepare_buf + param->write.offset, param->write.value, param->write.len);
    prepare_write_env->prepare_len += param->write.len;
}

void pgp_exec_write_event_env(esp_gatt_if_t gatts_if,
    prepare_type_env_t* prepare_write_env,
    esp_ble_gatts_cb_param_t* param) {
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC && prepare_write_env->prepare_buf) {
        ESP_LOG_BUFFER_HEX(BT_GATTS_TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len);

        // it seems that this will be called only by Android version of the Pokemon Go App
        ESP_LOGD(BT_GATTS_TAG, "WRITE EVT");

        if (certificate_handle_table[IDX_CHAR_CENTRAL_TO_SFIDA_VAL] == prepare_write_env->handle) {
            handle_pgp_handshake_second(
                gatts_if, prepare_write_env->prepare_buf, prepare_write_env->prepare_len, param->write.conn_id);
        } else if (led_button_handle_table[IDX_CHAR_LED_VAL] == prepare_write_env->handle) {
            handle_led_notify_from_app(gatts_if, param->write.conn_id, prepare_write_env->prepare_buf);
        }
    } else {
        ESP_LOGI(BT_GATTS_TAG, "ESP_GATT_PREP_WRITE_CANCEL");
    }
    if (prepare_write_env->prepare_buf) {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}

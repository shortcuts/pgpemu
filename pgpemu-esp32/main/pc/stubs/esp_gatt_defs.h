// PC-test stub of ESP-IDF's esp_gatt_defs.h
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef int esp_gatt_if_t;
typedef unsigned char esp_bd_addr_t[6];
#define ESP_GATT_CONN_TERMINATE_PEER_USER 0x13
#define ESP_GATT_CONN_TERMINATE_LOCAL_HOST 0x16
#define ESP_UUID_LEN_16 2
#define ESP_UUID_LEN_128 16
#define ESP_GATT_UUID_PRI_SERVICE 0x2800
#define ESP_GATT_UUID_CHAR_DECLARE 0x2803
#define ESP_GATT_UUID_CHAR_CLIENT_CONFIG 0x2902
#define ESP_GATT_CHAR_PROP_BIT_WRITE 0x08
#define ESP_GATT_CHAR_PROP_BIT_INDICATE 0x20
#define ESP_GATT_PERM_READ 0x01
#define ESP_GATT_PERM_READ_ENCRYPTED 0x02
#define ESP_GATT_PERM_WRITE 0x10
#define ESP_GATT_PERM_WRITE_ENCRYPTED 0x20
#define ESP_GATT_AUTO_RSP 0
typedef struct {
    uint8_t auto_rsp;
} esp_attr_control_t;
typedef struct {
    uint16_t uuid_length;
    uint8_t* uuid_p;
    uint16_t perm;
    uint16_t max_length;
    uint16_t length;
    uint8_t* value;
} esp_attr_desc_t;
typedef struct {
    esp_attr_control_t attr_control;
    esp_attr_desc_t att_desc;
} esp_gatts_attr_db_t;

// PC-test stub: the nvs_* calls config_storage.c and nvs_helper.c make; implemented in nvs_mem.h
#pragma once
#include "esp_system.h"

#include <stddef.h>
#include <stdint.h>
typedef uint32_t nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define ESP_ERR_NVS_NO_FREE_PAGES 0x110d
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x110e
#define ESP_ERR_NVS_INVALID_LENGTH 0x1106
#define ESP_ERR_NVS_INVALID_HANDLE 0x1104
esp_err_t nvs_open(const char* ns, nvs_open_mode_t mode, nvs_handle_t* out);
void nvs_close(nvs_handle_t h);
esp_err_t nvs_commit(nvs_handle_t h);
esp_err_t nvs_set_u8(nvs_handle_t h, const char* key, uint8_t v);
esp_err_t nvs_get_u8(nvs_handle_t h, const char* key, uint8_t* v);
esp_err_t nvs_set_i8(nvs_handle_t h, const char* key, int8_t v);
esp_err_t nvs_get_i8(nvs_handle_t h, const char* key, int8_t* v);
esp_err_t nvs_set_blob(nvs_handle_t h, const char* key, const void* v, size_t len);
esp_err_t nvs_get_blob(nvs_handle_t h, const char* key, void* v, size_t* len);
esp_err_t nvs_erase_key(nvs_handle_t h, const char* key);
esp_err_t nvs_open_from_partition(const char* part, const char* ns, nvs_open_mode_t mode, nvs_handle_t* out);
esp_err_t nvs_get_str(nvs_handle_t h, const char* key, char* v, size_t* len);
esp_err_t nvs_set_str(nvs_handle_t h, const char* key, const char* v);
esp_err_t nvs_erase_all(nvs_handle_t h);

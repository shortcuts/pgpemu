// PC-test stub: counting esp_ota_* mocks with settable failures and image name
#pragma once
#include "esp_system.h"

#include <stdint.h>
#include <string.h>

#define ESP_ERR_NOT_FOUND 0x105
#define OTA_WITH_SEQUENTIAL_WRITES 0xFFFFFFFEUL
typedef uint32_t esp_ota_handle_t;
typedef struct {
    uint32_t size;
} esp_partition_t;
typedef struct {
    char project_name[32];
} esp_app_desc_t;

static esp_partition_t ota_stub_partition = { 0x1c0000 };
static int ota_begin_calls, ota_write_calls, ota_end_calls, ota_abort_calls, ota_boot_calls, ota_valid_calls;
static uint32_t ota_written;
static esp_err_t ota_write_result, ota_end_result, ota_valid_result;
static char ota_stub_project[32] = "pgpemu";

static inline const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t* from) {
    (void)from;
    return &ota_stub_partition;
}
static inline esp_err_t esp_ota_begin(const esp_partition_t* p, unsigned long size, esp_ota_handle_t* h) {
    (void)p, (void)size;
    *h = 1;
    ota_begin_calls++;
    return ESP_OK;
}
static inline esp_err_t esp_ota_write(esp_ota_handle_t h, const void* d, size_t n) {
    (void)h, (void)d;
    ota_write_calls++;
    ota_written += (uint32_t)n;
    return ota_write_result;
}
static inline esp_err_t esp_ota_end(esp_ota_handle_t h) {
    (void)h;
    ota_end_calls++;
    return ota_end_result;
}
static inline esp_err_t esp_ota_abort(esp_ota_handle_t h) {
    (void)h;
    ota_abort_calls++;
    return ESP_OK;
}
static inline esp_err_t esp_ota_get_partition_description(const esp_partition_t* p, esp_app_desc_t* d) {
    (void)p;
    memset(d, 0, sizeof(*d));
    memcpy(d->project_name, ota_stub_project, sizeof(ota_stub_project));
    return ESP_OK;
}
static inline esp_err_t esp_ota_set_boot_partition(const esp_partition_t* p) {
    (void)p;
    ota_boot_calls++;
    return ESP_OK;
}
static inline esp_err_t esp_ota_mark_app_valid_cancel_rollback(void) {
    ota_valid_calls++;
    return ota_valid_result;
}

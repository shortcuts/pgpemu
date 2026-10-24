// PC-test stub: core dump calls return values the test sets, erase calls are counted
#pragma once
#include "esp_system.h"

#include <stdint.h>

#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_INVALID_CRC 0x109
typedef struct {
    uint8_t stackdump[1024];
    uint32_t dump_size;
} esp_core_dump_bt_info_t;
typedef struct {
    uint32_t mcause, mtval, ra, sp;
} esp_core_dump_summary_extra_info_t;
typedef struct {
    char exc_task[16];
    uint32_t exc_pc;
    esp_core_dump_bt_info_t exc_bt_info;
    uint8_t app_elf_sha256[10];
    esp_core_dump_summary_extra_info_t ex_info;
} esp_core_dump_summary_t;

static esp_core_dump_summary_t core_dump_stub_summary;
static esp_err_t core_dump_check_result = ESP_ERR_NOT_FOUND, core_dump_erase_result;
static int core_dump_erase_calls;

static inline esp_err_t esp_core_dump_image_check(void) {
    return core_dump_check_result;
}
static inline esp_err_t esp_core_dump_get_summary(esp_core_dump_summary_t* s) {
    *s = core_dump_stub_summary;
    return ESP_OK;
}
static inline esp_err_t esp_core_dump_image_erase(void) {
    core_dump_erase_calls++;
    return core_dump_erase_result;
}

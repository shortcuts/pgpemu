// PC-test stub: error codes, esp_restart records a call, heap size is a constant
#pragma once
#include <stdint.h>
#include <stdlib.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
static inline const char* esp_err_to_name(esp_err_t e) {
    return e == ESP_OK ? "ESP_OK" : "ESP_ERR";
}
#define ESP_ERROR_CHECK(x)   \
    do {                     \
        if ((x) != ESP_OK) { \
            abort();         \
        }                    \
    } while (0)
static int esp_restart_calls;
static inline void esp_restart(void) {
    esp_restart_calls++;
}
static inline uint32_t esp_get_free_heap_size(void) {
    return 0;
}

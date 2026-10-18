// PC-test stub: logs go to stdout, level is always DEBUG
#pragma once
#include <stdio.h>
#define ESP_LOG_DEBUG 4
static inline int esp_log_level_get(const char* tag) {
    (void)tag;
    return 0;
}
#define ESP_LOGD(tag, fmt, ...) printf("D [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) printf("I [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("W [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("E [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOG_BUFFER_HEX(tag, data, len) ((void)0)

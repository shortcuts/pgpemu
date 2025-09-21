// Minimal unit test for nvs_helper.c (PC build)
#include "nvs_helper.h"

#include <assert.h>
#include <stdio.h>

// Dummy error codes for PC test
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 1
#define ESP_FAIL -1

typedef int esp_err_t;

// Dummy log macros
#define ESP_LOGW(tag, fmt, ...) printf("W [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("E [%s] " fmt "\n", tag, ##__VA_ARGS__)
const char* esp_err_to_name(esp_err_t err) {
    return err == ESP_OK ? "ESP_OK"
                         : (err == ESP_ERR_NVS_NOT_FOUND ? "ESP_ERR_NVS_NOT_FOUND" : "ESP_FAIL");
}

int main() {
    // Test nvs_read_check
    assert(nvs_read_check("TEST", ESP_OK, "foo"));
    assert(!nvs_read_check("TEST", ESP_ERR_NVS_NOT_FOUND, "foo"));
    assert(!nvs_read_check("TEST", ESP_FAIL, "foo"));

    // Test nvs_write_check
    assert(nvs_write_check("TEST", ESP_OK, "bar"));
    assert(!nvs_write_check("TEST", ESP_FAIL, "bar"));

    printf("nvs_helper tests passed.\n");
    return 0;
}

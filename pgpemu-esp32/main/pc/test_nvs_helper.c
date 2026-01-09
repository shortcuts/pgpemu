// Comprehensive unit tests for nvs_helper.c (PC build)
#include "nvs_helper.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// Dummy error codes for PC test
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 1
#define ESP_ERR_NVS_INVALID_NAME 2
#define ESP_ERR_NVS_READ_ONLY 3
#define ESP_ERR_NVS_INVALID_LENGTH 4
#define ESP_FAIL -1

typedef int esp_err_t;

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

// Dummy log macros
#define ESP_LOGW(tag, fmt, ...) printf("  W [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("  E [%s] " fmt "\n", tag, ##__VA_ARGS__)

const char* esp_err_to_name(esp_err_t err) {
    switch (err) {
        case ESP_OK:
            return "ESP_OK";
        case ESP_ERR_NVS_NOT_FOUND:
            return "ESP_ERR_NVS_NOT_FOUND";
        case ESP_ERR_NVS_INVALID_NAME:
            return "ESP_ERR_NVS_INVALID_NAME";
        case ESP_ERR_NVS_READ_ONLY:
            return "ESP_ERR_NVS_READ_ONLY";
        case ESP_ERR_NVS_INVALID_LENGTH:
            return "ESP_ERR_NVS_INVALID_LENGTH";
        default:
            return "ESP_FAIL";
    }
}

// Test helper macro
#define test_assert(condition, message)                                                                            \
    if (condition) {                                                                                               \
        printf("  ✓ %s\n", message);                                                                              \
        tests_passed++;                                                                                            \
    } else {                                                                                                       \
        printf("  ✗ %s\n", message);                                                                              \
        tests_failed++;                                                                                            \
    }

int main() {
    printf("========================================\n");
    printf("NVS Helper Unit Tests\n");
    printf("========================================\n\n");

    // Test 1: nvs_read_check with success
    printf("=== Test: nvs_read_check Success Cases ===\n");
    test_assert(nvs_read_check("TEST", ESP_OK, "key_exists"), "nvs_read_check returns true on ESP_OK");
    test_assert(nvs_read_check("APP", ESP_OK, "valid_key"), "nvs_read_check works with different tags");

    // Test 2: nvs_read_check with not found
    printf("\n=== Test: nvs_read_check Not Found ===\n");
    test_assert(!nvs_read_check("TEST", ESP_ERR_NVS_NOT_FOUND, "missing_key"),
                "nvs_read_check returns false on ESP_ERR_NVS_NOT_FOUND");
    test_assert(!nvs_read_check("APP", ESP_ERR_NVS_NOT_FOUND, "nonexistent"),
                "nvs_read_check handles missing key with different tag");

    // Test 3: nvs_read_check with generic failure
    printf("\n=== Test: nvs_read_check Failure Cases ===\n");
    test_assert(!nvs_read_check("TEST", ESP_FAIL, "read_error"),
                "nvs_read_check returns false on generic ESP_FAIL");
    test_assert(!nvs_read_check("TEST", ESP_ERR_NVS_INVALID_NAME, "bad_key_name"),
                "nvs_read_check returns false on ESP_ERR_NVS_INVALID_NAME");
    test_assert(!nvs_read_check("TEST", ESP_ERR_NVS_INVALID_LENGTH, "size_mismatch"),
                "nvs_read_check returns false on ESP_ERR_NVS_INVALID_LENGTH");
    test_assert(!nvs_read_check("TEST", -999, "unknown_error"),
                "nvs_read_check returns false on unknown error code");

    // Test 4: nvs_write_check with success
    printf("\n=== Test: nvs_write_check Success Cases ===\n");
    test_assert(nvs_write_check("TEST", ESP_OK, "write_key"), "nvs_write_check returns true on ESP_OK");
    test_assert(nvs_write_check("CONFIG", ESP_OK, "setting_value"), "nvs_write_check works with different tags");

    // Test 5: nvs_write_check with failures
    printf("\n=== Test: nvs_write_check Failure Cases ===\n");
    test_assert(!nvs_write_check("TEST", ESP_FAIL, "write_failed"),
                "nvs_write_check returns false on ESP_FAIL");
    test_assert(!nvs_write_check("TEST", ESP_ERR_NVS_READ_ONLY, "read_only_partition"),
                "nvs_write_check returns false on ESP_ERR_NVS_READ_ONLY");
    test_assert(!nvs_write_check("TEST", ESP_ERR_NVS_INVALID_LENGTH, "value_too_large"),
                "nvs_write_check returns false on ESP_ERR_NVS_INVALID_LENGTH");
    test_assert(!nvs_write_check("TEST", ESP_ERR_NVS_NOT_FOUND, "invalid_error_code"),
                "nvs_write_check returns false on ESP_ERR_NVS_NOT_FOUND");

    // Test 6: Tag handling
    printf("\n=== Test: Tag Handling ===\n");
    test_assert(nvs_read_check("CONFIG", ESP_OK, "config_key"), "nvs_read_check handles CONFIG tag");
    test_assert(nvs_read_check("DEVICE", ESP_OK, "device_key"), "nvs_read_check handles DEVICE tag");
    test_assert(nvs_read_check("CRYPTO", ESP_OK, "crypto_key"), "nvs_read_check handles CRYPTO tag");
    test_assert(nvs_write_check("CONFIG", ESP_OK, "config_val"), "nvs_write_check handles CONFIG tag");
    test_assert(nvs_write_check("DEVICE", ESP_OK, "device_val"), "nvs_write_check handles DEVICE tag");

    // Test 7: Key name handling
    printf("\n=== Test: Key Name Handling ===\n");
    test_assert(nvs_read_check("TEST", ESP_OK, "short"), "nvs_read_check handles short key names");
    test_assert(nvs_read_check("TEST", ESP_OK, "very_long_key_name_with_many_characters"),
                "nvs_read_check handles long key names");
    test_assert(nvs_read_check("TEST", ESP_OK, "key-with-special_chars.123"),
                "nvs_read_check handles special characters in key names");
    test_assert(nvs_write_check("TEST", ESP_OK, "write_key"), "nvs_write_check handles key names");

    // Test 8: Consistency between read and write checks
    printf("\n=== Test: Read/Write Consistency ===\n");
    test_assert(nvs_read_check("TEST", ESP_OK, "key") && nvs_write_check("TEST", ESP_OK, "key"),
                "Both read and write succeed on ESP_OK");
    test_assert(!nvs_read_check("TEST", ESP_FAIL, "key") && !nvs_write_check("TEST", ESP_FAIL, "key"),
                "Both read and write fail on ESP_FAIL");
    test_assert(!nvs_read_check("TEST", ESP_ERR_NVS_NOT_FOUND, "key") &&
                    nvs_write_check("TEST", ESP_OK, "key"),
                "Read fails on not found, but write can succeed");

    // Summary
    printf("\n========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);

    if (tests_failed == 0) {
        printf("\n✓ All nvs_helper tests passed!\n");
        return 0;
    } else {
        printf("\n✗ %d tests failed!\n", tests_failed);
        return 1;
    }
}

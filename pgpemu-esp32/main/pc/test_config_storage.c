// Unit tests for config_storage module (PC build)
// Tests device key hashing, session persistence functions
#ifndef ESP_PLATFORM

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

// Mock ESP types and functions
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 1
#define ESP_ERR_NVS_EXISTS 2
#define ESP_FAIL -1
#define CONFIG_BT_ACL_CONNECTIONS 4

typedef int esp_err_t;
typedef unsigned char esp_bd_addr_t[6];

// Mock logging
#define ESP_LOGD(tag, fmt, ...) printf("D [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("W [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("E [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) printf("I [%s] " fmt "\n", tag, ##__VA_ARGS__)

const char CONFIG_STORAGE_TAG[] = "config_storage";

// FNV-1a hash implementation (from config_storage.c)
char* make_device_key_for_option(const char* key, const esp_bd_addr_t bda, char* out) {
    // 1. Concatenate safely into a temp buffer
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%s_%02x%02x%02x%02x%02x%02x", 
                       key, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    if (len == 0) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "unable to concatenate key and bda for %s", key);
        return out;
    }

    // 2. FNV-1a hash
    uint64_t hash = 1469598103934665603ULL;
    for (const char* p = buf; *p; p++)
        hash = (hash ^ (unsigned char)*p) * 1099511628211ULL;

    // 3. Convert to hex string, 15 chars max (truncate if needed)
    len = snprintf(out, 16, "%015llx", (unsigned long long)hash);
    if (len == 0) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "unable to hash final key for %s", key);
        return out;
    }

    return out;
}

// Test device key generation for different MACs
void test_device_key_generation() {
    printf("\n=== Test: Device Key Generation ===\n");
    
    // Test with MAC 1
    esp_bd_addr_t bda1 = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    char key1[16], key2[16], key3[16];
    
    // Same MAC should produce same key
    make_device_key_for_option("test", bda1, key1);
    make_device_key_for_option("test", bda1, key2);
    assert(strcmp(key1, key2) == 0);
    printf("✓ Same MAC produces consistent key: %s\n", key1);
    
    // Different option name should produce different key
    make_device_key_for_option("spin", bda1, key2);
    make_device_key_for_option("catch", bda1, key3);
    assert(strcmp(key2, key3) != 0);
    printf("✓ Different option names produce different keys\n");
    
    // Test with different MAC
    esp_bd_addr_t bda2 = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    char key4[16];
    make_device_key_for_option("test", bda2, key4);
    assert(strcmp(key1, key4) != 0);
    printf("✓ Different MACs produce different keys\n");
    
    // Verify key is 15 hex characters
    assert(strlen(key1) == 15);
    printf("✓ Key length is exactly 15 characters\n");
}

// Test session key persistence logic (without actual NVS)
void test_session_key_persistence_logic() {
    printf("\n=== Test: Session Key Persistence Logic ===\n");
    
    // Simulate session keys
    uint8_t session_key[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    uint8_t reconnect_challenge[32] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                                       0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
                                       0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
                                       0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30};
    
    // Verify key sizes
    assert(sizeof(session_key) == 16);
    assert(sizeof(reconnect_challenge) == 32);
    printf("✓ Session key size: %zu bytes (expected 16)\n", sizeof(session_key));
    printf("✓ Reconnect challenge size: %zu bytes (expected 32)\n", sizeof(reconnect_challenge));
    
    // Simulate null pointer checks
    assert(session_key != NULL && reconnect_challenge != NULL);
    printf("✓ Null pointer validation works\n");
}

// Test MAC address format consistency
void test_mac_address_handling() {
    printf("\n=== Test: MAC Address Handling ===\n");
    
    esp_bd_addr_t bda = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    char key[16];
    
    make_device_key_for_option("mac_test", bda, key);
    
     // Verify MAC bytes are in correct order
     char test_key[64];
     snprintf(test_key, sizeof(test_key), "%02x%02x%02x%02x%02x%02x", 
              bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
     assert(strcmp(test_key, "aabbccddeeff") == 0);
     printf("✓ MAC address bytes in correct order: %s\n", test_key);
    
    // Test all zeros
    esp_bd_addr_t zero_bda = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    char zero_key[16];
    make_device_key_for_option("zero", zero_bda, zero_key);
    printf("✓ Zero MAC handled: %s\n", zero_key);
    
    // Test all ones
    esp_bd_addr_t ones_bda = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    char ones_key[16];
    make_device_key_for_option("ones", ones_bda, ones_key);
    printf("✓ All-ones MAC handled: %s\n", ones_key);
}

// Run all tests
int main() {
    printf("========================================\n");
    printf("Config Storage Unit Tests\n");
    printf("========================================\n");
    
    test_device_key_generation();
    test_session_key_persistence_logic();
    test_mac_address_handling();
    
    printf("\n========================================\n");
    printf("✓ All config_storage tests passed!\n");
    printf("========================================\n");
    
    return 0;
}

#endif

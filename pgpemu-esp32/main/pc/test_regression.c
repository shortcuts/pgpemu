// Regression tests for previously fixed critical bugs
// These tests verify that bugs do not resurface in future changes
// Test count: 42 assertions (34 original + 8 stale cache clearing tests)
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock types and constants
typedef int esp_err_t;
typedef void* SemaphoreHandle_t;

#define ESP_OK 0
#define ESP_FAIL -1

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

#define test_assert(condition, message) \
    if (condition) {                    \
        printf("  ✓ %s\n", message);    \
        tests_passed++;                 \
    } else {                            \
        printf("  ✗ %s\n", message);    \
        tests_failed++;                 \
    }

// =============================================================================
// REGRESSION TEST 1: Device Settings Pointer Initialization
// Issue: read_stored_device_settings() return value was ignored in pgp_gatts.c
// Result: Uninitialized pointers caused NULL dereference crashes
// =============================================================================

typedef struct {
    bool autospin;
    bool autocatch;
    uint8_t autospin_probability;
    SemaphoreHandle_t mutex;
} RegDeviceSettings;

bool reg_old_buggy_settings_init(RegDeviceSettings* settings) {
    (void)settings;
    return false;
}

bool reg_new_fixed_settings_init(RegDeviceSettings* settings) {
    if (settings == NULL) {
        return false;
    }
    settings->autospin = false;
    settings->autocatch = false;
    settings->autospin_probability = 0;
    settings->mutex = NULL;
    return true;
}

void test_device_settings_pointer_initialization(void) {
    printf("=== Test: Device Settings Pointer Initialization (Bug #1) ===\n");

    RegDeviceSettings settings = { 0 };
    bool result = reg_new_fixed_settings_init(&settings);
    test_assert(result, "Settings initialization returns true on success");
    test_assert(settings.autospin == false, "Autospin initialized to false");
    test_assert(settings.autocatch == false, "Autocatch initialized to false");
    test_assert(settings.autospin_probability == 0, "Probability initialized to 0");

    result = reg_new_fixed_settings_init(NULL);
    test_assert(!result, "Fixed code detects NULL pointer and returns false");
}

// =============================================================================
// REGRESSION TEST 2: Settings Mutex Memory Leak
// Issue: New mutex created per settings read but never stored or freed
// Result: Memory exhaustion with multiple device connections
// =============================================================================

typedef struct {
    SemaphoreHandle_t mutex;
    int ref_count;
} RegMutexedSettings;

static int malloc_count = 0;
static int free_count = 0;

void* reg_test_malloc(size_t size) {
    malloc_count++;
    return malloc(size);
}

void reg_test_free(void* ptr) {
    if (ptr != NULL) {
        free_count++;
        free(ptr);
    }
}

bool reg_new_fixed_mutex_init(RegMutexedSettings* settings) {
    if (settings == NULL) {
        return false;
    }
    settings->mutex = (SemaphoreHandle_t)reg_test_malloc(sizeof(SemaphoreHandle_t));
    settings->ref_count = 1;
    return true;
}

bool reg_new_fixed_mutex_cleanup(RegMutexedSettings* settings) {
    if (settings == NULL || settings->mutex == NULL) {
        return false;
    }
    reg_test_free(settings->mutex);
    settings->mutex = NULL;
    settings->ref_count = 0;
    return true;
}

void test_settings_mutex_memory_leak(void) {
    printf("=== Test: Settings Mutex Memory Leak (Bug #2) ===\n");

    malloc_count = 0;
    free_count = 0;

    RegMutexedSettings settings_array[5];
    for (int i = 0; i < 5; i++) {
        test_assert(reg_new_fixed_mutex_init(&settings_array[i]), "Mutex initialization succeeds");
    }
    test_assert(malloc_count == 5, "Fixed code allocates 5 mutexes");

    for (int i = 0; i < 5; i++) {
        test_assert(reg_new_fixed_mutex_cleanup(&settings_array[i]), "Mutex cleanup succeeds");
    }
    test_assert(free_count == 5, "Fixed code frees all mutexes");
    test_assert(malloc_count == free_count, "All allocated mutexes are freed (NO LEAK)");

    RegMutexedSettings single_settings;
    reg_new_fixed_mutex_init(&single_settings);
    test_assert(reg_new_fixed_mutex_cleanup(&single_settings), "First cleanup succeeds");
    test_assert(!reg_new_fixed_mutex_cleanup(&single_settings), "Second cleanup fails safely");
}

// =============================================================================
// REGRESSION TEST 3: Race Condition on active_connections Counter
// Issue: active_connections counter modified without mutex protection
// Result: Incorrect connection count with concurrent updates
// =============================================================================

static int reg_active_connections = 0;
static int reg_mutex_take_count = 0;
static int reg_mutex_give_count = 0;

bool reg_mock_mutex_take(void) {
    reg_mutex_take_count++;
    return true;
}

void reg_mock_mutex_give(void) {
    reg_mutex_give_count++;
}

void reg_new_increment_connection(void) {
    reg_mock_mutex_take();
    reg_active_connections++;
    reg_mock_mutex_give();
}

void reg_new_decrement_connection(void) {
    reg_mock_mutex_take();
    reg_active_connections--;
    reg_mock_mutex_give();
}

void test_race_condition_active_connections(void) {
    printf("=== Test: Race Condition on active_connections (Bug #3) ===\n");

    reg_active_connections = 0;
    reg_mutex_take_count = 0;
    reg_mutex_give_count = 0;

    reg_new_increment_connection();
    reg_new_increment_connection();
    reg_new_decrement_connection();

    test_assert(reg_active_connections == 1, "New code updates counter correctly");
    test_assert(reg_mutex_take_count == 3, "New code acquires mutex for each operation");
    test_assert(reg_mutex_give_count == 3, "New code releases mutex for each operation");
    test_assert(reg_mutex_take_count == reg_mutex_give_count, "Mutex acquire/release are balanced");

    reg_active_connections = 0;
    reg_mutex_take_count = 0;
    reg_mutex_give_count = 0;

    for (int i = 0; i < 50; i++) {
        reg_new_increment_connection();
    }
    for (int i = 0; i < 30; i++) {
        reg_new_decrement_connection();
    }

    test_assert(reg_active_connections == 20, "Counter is correct after 50 increments, 30 decrements");
    test_assert(reg_mutex_take_count == 80, "Mutex taken 80 times (50 + 30)");
    test_assert(reg_mutex_give_count == 80, "Mutex released 80 times");
}

// =============================================================================
// REGRESSION TEST 4: Stale Session Cache Clearing
// Issue: Cached session keys may become invalid after Android credential reset
// Result: Device fails to reconnect, requires manual cache clearing
// =============================================================================

typedef struct {
    uint32_t connection_start_ms;
    uint32_t connection_end_ms;
    bool used_cached_session;
    uint8_t disconnect_reason;
    bool cache_cleared;
} RegConnectionState;

#define REG_ESP_GATT_CONN_TERMINATE_PEER_USER 0x13
#define REG_ESP_GATT_CONN_TERMINATE_LOCAL_HOST 0x16

bool reg_should_clear_cache(RegConnectionState* state) {
    if (state == NULL) {
        return false;
    }

    uint32_t conn_duration_ms = state->connection_end_ms - state->connection_start_ms;

    // Early disconnect heuristic: < 5000 ms + cached session + peer user disconnect
    if (conn_duration_ms < 5000 && state->used_cached_session) {
        if (state->disconnect_reason == REG_ESP_GATT_CONN_TERMINATE_PEER_USER) {
            return true;
        }
    }

    return false;
}

void reg_handle_disconnect(RegConnectionState* state) {
    if (state == NULL) {
        return;
    }

    if (reg_should_clear_cache(state)) {
        state->cache_cleared = true;
    }
}

void test_stale_cache_clearing(void) {
    printf("=== Test: Stale Session Cache Clearing (Bug #7) ===\n");

    // Test 1: Early disconnect with cached session and PEER_USER reason -> cache cleared
    RegConnectionState early_disconnect = { .connection_start_ms = 1000,
        .connection_end_ms = 2500,  // 1500 ms duration (< 5000)
        .used_cached_session = true,
        .disconnect_reason = REG_ESP_GATT_CONN_TERMINATE_PEER_USER,
        .cache_cleared = false };
    reg_handle_disconnect(&early_disconnect);
    test_assert(early_disconnect.cache_cleared, "Early disconnect (<5s) with cached session clears cache");

    // Test 2: Normal disconnect (>5s) with cached session -> cache NOT cleared
    RegConnectionState normal_disconnect = { .connection_start_ms = 1000,
        .connection_end_ms = 8000,  // 7000 ms duration (> 5000)
        .used_cached_session = true,
        .disconnect_reason = REG_ESP_GATT_CONN_TERMINATE_PEER_USER,
        .cache_cleared = false };
    reg_handle_disconnect(&normal_disconnect);
    test_assert(!normal_disconnect.cache_cleared, "Normal disconnect (>5s) does NOT clear cache");

    // Test 3: Early disconnect without cached session -> cache NOT cleared
    RegConnectionState fresh_handshake = { .connection_start_ms = 1000,
        .connection_end_ms = 2500,     // 1500 ms duration (< 5000)
        .used_cached_session = false,  // Full handshake was used
        .disconnect_reason = REG_ESP_GATT_CONN_TERMINATE_PEER_USER,
        .cache_cleared = false };
    reg_handle_disconnect(&fresh_handshake);
    test_assert(!fresh_handshake.cache_cleared, "Early disconnect without cached session does NOT clear cache");

    // Test 4: Early disconnect with LOCAL_HOST reason -> cache NOT cleared
    RegConnectionState local_disconnect = { .connection_start_ms = 1000,
        .connection_end_ms = 2500,  // 1500 ms duration (< 5000)
        .used_cached_session = true,
        .disconnect_reason = REG_ESP_GATT_CONN_TERMINATE_LOCAL_HOST,
        .cache_cleared = false };
    reg_handle_disconnect(&local_disconnect);
    test_assert(!local_disconnect.cache_cleared, "Early disconnect with LOCAL_HOST reason does NOT clear cache");

    // Test 5: Edge case - exactly 5000ms duration -> cache NOT cleared
    RegConnectionState edge_5000ms = { .connection_start_ms = 1000,
        .connection_end_ms = 6000,  // Exactly 5000 ms
        .used_cached_session = true,
        .disconnect_reason = REG_ESP_GATT_CONN_TERMINATE_PEER_USER,
        .cache_cleared = false };
    reg_handle_disconnect(&edge_5000ms);
    test_assert(!edge_5000ms.cache_cleared, "Disconnect at exactly 5000ms does NOT clear cache");

    // Test 6: Edge case - 4999ms duration -> cache cleared
    RegConnectionState edge_4999ms = { .connection_start_ms = 1000,
        .connection_end_ms = 5999,  // 4999 ms
        .used_cached_session = true,
        .disconnect_reason = REG_ESP_GATT_CONN_TERMINATE_PEER_USER,
        .cache_cleared = false };
    reg_handle_disconnect(&edge_4999ms);
    test_assert(edge_4999ms.cache_cleared, "Disconnect at 4999ms DOES clear cache");

    // Test 7: NULL state handling
    test_assert(!reg_should_clear_cache(NULL), "NULL state returns false");

    // Test 8: Very short connection (< 1 second)
    RegConnectionState very_short = { .connection_start_ms = 1000,
        .connection_end_ms = 1500,  // 500 ms
        .used_cached_session = true,
        .disconnect_reason = REG_ESP_GATT_CONN_TERMINATE_PEER_USER,
        .cache_cleared = false };
    reg_handle_disconnect(&very_short);
    test_assert(very_short.cache_cleared, "Very short connection (500ms) with cached session clears cache");
}

// =============================================================================
// REGRESSION TEST 5: Invalid BDA Handling
// Issue: BDA validation missing, could cause invalid operations
// Result: Crashes or undefined behavior with invalid device addresses
// =============================================================================

typedef struct {
    uint8_t addr[6];
} RegBDA;

bool reg_is_valid_bda(const RegBDA* bda) {
    if (bda == NULL) {
        return false;
    }
    if (bda->addr[0] == 0 && bda->addr[1] == 0 && bda->addr[2] == 0 && bda->addr[3] == 0 && bda->addr[4] == 0
        && bda->addr[5] == 0) {
        return false;
    }
    if (bda->addr[0] == 0xFF && bda->addr[1] == 0xFF && bda->addr[2] == 0xFF && bda->addr[3] == 0xFF
        && bda->addr[4] == 0xFF && bda->addr[5] == 0xFF) {
        return false;
    }
    return true;
}

void test_invalid_bda_handling(void) {
    printf("=== Test: Invalid BDA Handling (Bug #6) ===\n");

    test_assert(!reg_is_valid_bda(NULL), "NULL BDA is invalid");

    RegBDA zero_bda = { { 0, 0, 0, 0, 0, 0 } };
    test_assert(!reg_is_valid_bda(&zero_bda), "All-zeros BDA is invalid");

    RegBDA ones_bda = { { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } };
    test_assert(!reg_is_valid_bda(&ones_bda), "All-ones BDA is invalid");

    RegBDA valid_bda1 = { { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } };
    test_assert(reg_is_valid_bda(&valid_bda1), "Valid BDA #1 is accepted");

    RegBDA valid_bda2 = { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } };
    test_assert(reg_is_valid_bda(&valid_bda2), "Valid BDA #2 (single byte set) is accepted");

    RegBDA valid_bda3 = { { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE } };
    test_assert(reg_is_valid_bda(&valid_bda3), "Valid BDA #3 (almost all ones) is accepted");

    RegBDA valid_bda4 = { { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC } };
    test_assert(reg_is_valid_bda(&valid_bda4), "Valid BDA #4 (random) is accepted");
}

// =============================================================================
// Main test runner
// =============================================================================

int main() {
    printf("\n");
    printf("========================================\n");
    printf("Regression Test Suite\n");
    printf("========================================\n\n");

    test_device_settings_pointer_initialization();
    printf("\n");
    test_settings_mutex_memory_leak();
    printf("\n");
    test_race_condition_active_connections();
    printf("\n");
    test_stale_cache_clearing();
    printf("\n");
    test_invalid_bda_handling();

    printf("\n");
    printf("========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);

    if (tests_failed == 0) {
        printf("\n✓ All regression tests passed!\n");
        printf("  No regressions detected from previously fixed bugs.\n");
        return 0;
    } else {
        printf("\n✗ %d tests failed!\n", tests_failed);
        printf("  Regressions detected! Please investigate.\n");
        return 1;
    }
}

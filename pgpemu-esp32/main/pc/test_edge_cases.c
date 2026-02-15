// Edge case and boundary value tests
// Tests for maximum connections, boundary values, and invalid inputs
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

static void test_assert_impl(int condition, const char* message) {
    if (condition) {
        printf("  ✓ %s\n", message);
        tests_passed++;
    } else {
        printf("  ✗ %s\n", message);
        tests_failed++;
    }
}

#define test_assert(condition, message) test_assert_impl(condition, message)

// =============================================================================
// EDGE CASE TEST 1: Maximum Connections
// Issue: Ensure correct behavior at and beyond max connection limit (4)
// =============================================================================

#define MAX_CONNECTIONS 4

typedef struct {
    int conn_id;
    bool active;
} EdgeConnection;

typedef struct {
    EdgeConnection connections[MAX_CONNECTIONS];
    int active_count;
} EdgeConnectionManager;

EdgeConnectionManager edge_init_connection_manager(void) {
    EdgeConnectionManager manager = { 0 };
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        manager.connections[i].conn_id = -1;
        manager.connections[i].active = false;
    }
    manager.active_count = 0;
    return manager;
}

bool edge_add_connection(EdgeConnectionManager* mgr, int conn_id) {
    if (mgr == NULL || mgr->active_count >= MAX_CONNECTIONS) {
        return false;  // Cannot add more connections
    }
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!mgr->connections[i].active) {
            mgr->connections[i].conn_id = conn_id;
            mgr->connections[i].active = true;
            mgr->active_count++;
            return true;
        }
    }
    return false;
}

bool edge_remove_connection(EdgeConnectionManager* mgr, int conn_id) {
    if (mgr == NULL) {
        return false;
    }
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (mgr->connections[i].conn_id == conn_id && mgr->connections[i].active) {
            mgr->connections[i].active = false;
            mgr->connections[i].conn_id = -1;
            mgr->active_count--;
            return true;
        }
    }
    return false;
}

int edge_get_active_connections(EdgeConnectionManager* mgr) {
    return mgr ? mgr->active_count : 0;
}

void test_max_connections_edge_cases(void) {
    printf("=== Test: Maximum Connections Edge Cases ===\n");

    EdgeConnectionManager mgr = edge_init_connection_manager();
    test_assert(edge_get_active_connections(&mgr) == 0, "Initial active connections is 0");

    // Add connections up to the limit
    test_assert(edge_add_connection(&mgr, 1), "Add connection 1");
    test_assert(edge_add_connection(&mgr, 2), "Add connection 2");
    test_assert(edge_add_connection(&mgr, 3), "Add connection 3");
    test_assert(edge_add_connection(&mgr, 4), "Add connection 4 (max reached)");
    test_assert(edge_get_active_connections(&mgr) == 4, "Active connections count is 4");

    // Try to exceed limit
    test_assert(!edge_add_connection(&mgr, 5), "Adding 5th connection fails");
    test_assert(edge_get_active_connections(&mgr) == 4, "Active connections still 4 after failed add");

    // Remove and re-add
    test_assert(edge_remove_connection(&mgr, 2), "Remove connection 2");
    test_assert(edge_get_active_connections(&mgr) == 3, "Active connections count is 3");
    test_assert(edge_add_connection(&mgr, 5), "Add connection 5 (now possible)");
    test_assert(edge_get_active_connections(&mgr) == 4, "Back to 4 active connections");

    // Remove all
    for (int i = 1; i <= 5; i++) {
        if (edge_get_active_connections(&mgr) > 0) {
            // Find and remove
            for (int j = 0; j < MAX_CONNECTIONS; j++) {
                if (mgr.connections[j].active) {
                    edge_remove_connection(&mgr, mgr.connections[j].conn_id);
                    break;
                }
            }
        }
    }
    test_assert(edge_get_active_connections(&mgr) == 0, "All connections removed");
}

// =============================================================================
// EDGE CASE TEST 2: Probability Boundary Values
// Issue: Test probability validation at boundaries (0-9 is valid)
// =============================================================================

bool edge_is_valid_probability(uint8_t probability) {
    return probability <= 9;
}

void test_probability_boundary_values(void) {
    printf("=== Test: Probability Boundary Values ===\n");

    // Valid boundaries
    test_assert(edge_is_valid_probability(0), "Probability 0 is valid (min)");
    test_assert(edge_is_valid_probability(1), "Probability 1 is valid");
    test_assert(edge_is_valid_probability(5), "Probability 5 is valid (middle)");
    test_assert(edge_is_valid_probability(9), "Probability 9 is valid (max)");

    // Invalid values
    test_assert(!edge_is_valid_probability(10), "Probability 10 is invalid (1 over max)");
    test_assert(!edge_is_valid_probability(99), "Probability 99 is invalid");
    test_assert(!edge_is_valid_probability(255), "Probability 255 is invalid (max uint8)");

    // Ensure all 0-9 are valid
    for (int i = 0; i <= 9; i++) {
        test_assert(edge_is_valid_probability((uint8_t)i), "All probabilities 0-9 valid");
    }

    // Ensure all 10+ are invalid
    for (int i = 10; i <= 20; i++) {
        test_assert(!edge_is_valid_probability((uint8_t)i), "All probabilities 10+ invalid");
    }
}

// =============================================================================
// EDGE CASE TEST 4: Boolean Toggle Edge Cases
// Issue: Test toggle behavior with edge cases and repeated toggles
// =============================================================================

bool edge_toggle_bool(bool* value) {
    if (value == NULL) {
        return false;
    }
    *value = !*value;
    return true;
}

void test_boolean_toggle_edge_cases(void) {
    printf("=== Test: Boolean Toggle Edge Cases ===\n");

    // Test NULL pointer
    test_assert(!edge_toggle_bool(NULL), "NULL pointer returns false");

    // Test normal toggle
    bool value = false;
    test_assert(edge_toggle_bool(&value), "Toggle succeeds");
    test_assert(value == true, "Value toggled to true");
    test_assert(edge_toggle_bool(&value), "Toggle succeeds");
    test_assert(value == false, "Value toggled back to false");

    // Test repeated toggles
    for (int i = 0; i < 100; i++) {
        edge_toggle_bool(&value);
    }
    test_assert(value == false, "After 100 toggles from false, value is false (even count)");

    for (int i = 0; i < 101; i++) {
        edge_toggle_bool(&value);
    }
    test_assert(value == true, "After 101 toggles from false, value is true (odd count)");
}

// =============================================================================
// EDGE CASE TEST 5: Array Index Boundary Cases
// Issue: Test accessing device settings at boundary indices
// =============================================================================

#define EDGE_MAX_DEVICES 4

typedef struct {
    bool autospin;
    uint8_t conn_id;
} EdgeDeviceSettingsArray;

EdgeDeviceSettingsArray* edge_get_device_settings(EdgeDeviceSettingsArray* array, int index) {
    if (array == NULL || index < 0 || index >= EDGE_MAX_DEVICES) {
        return NULL;
    }
    return &array[index];
}

void test_array_index_boundary_cases(void) {
    printf("=== Test: Array Index Boundary Cases ===\n");

    EdgeDeviceSettingsArray devices[EDGE_MAX_DEVICES];
    for (int i = 0; i < EDGE_MAX_DEVICES; i++) {
        devices[i].autospin = false;
        devices[i].conn_id = (uint8_t)i;
    }

    // Test valid boundaries
    test_assert(edge_get_device_settings(devices, 0) != NULL, "Index 0 (min) is valid");
    test_assert(edge_get_device_settings(devices, 1) != NULL, "Index 1 is valid");
    test_assert(edge_get_device_settings(devices, EDGE_MAX_DEVICES - 1) != NULL, "Index 3 (max) is valid");

    // Test invalid boundaries
    test_assert(edge_get_device_settings(devices, -1) == NULL, "Index -1 (negative) is invalid");
    test_assert(edge_get_device_settings(devices, EDGE_MAX_DEVICES) == NULL, "Index 4 (just beyond max) is invalid");
    test_assert(edge_get_device_settings(devices, 100) == NULL, "Index 100 (far beyond) is invalid");

    // Test NULL array
    test_assert(edge_get_device_settings(NULL, 0) == NULL, "NULL array returns NULL");

    // Verify accessing valid indices works correctly
    EdgeDeviceSettingsArray* first = edge_get_device_settings(devices, 0);
    test_assert(first != NULL && first->conn_id == 0, "Can access first element");

    EdgeDeviceSettingsArray* last = edge_get_device_settings(devices, EDGE_MAX_DEVICES - 1);
    test_assert(last != NULL && last->conn_id == EDGE_MAX_DEVICES - 1, "Can access last element");
}

// =============================================================================
// EDGE CASE TEST 6: String Key Length Boundaries
// Issue: Test NVS key length constraints (max 15 chars + null)
// =============================================================================

#define EDGE_NVS_KEY_MAX_LEN 15

bool edge_is_valid_nvs_key(const char* key) {
    if (key == NULL) {
        return false;
    }
    // Check length: 1 to 15 characters (excluding null terminator)
    size_t len = 0;
    for (len = 0; len < EDGE_NVS_KEY_MAX_LEN + 1; len++) {
        if (key[len] == '\0') {
            break;
        }
    }
    return len > 0 && len <= EDGE_NVS_KEY_MAX_LEN;
}

void test_nvs_key_length_boundaries(void) {
    printf("=== Test: NVS Key Length Boundaries ===\n");

    // Test NULL
    test_assert(!edge_is_valid_nvs_key(NULL), "NULL key is invalid");

    // Test empty string
    test_assert(!edge_is_valid_nvs_key(""), "Empty string is invalid");

    // Test valid lengths (1-15)
    test_assert(edge_is_valid_nvs_key("a"), "Length 1 is valid");
    test_assert(edge_is_valid_nvs_key("ab"), "Length 2 is valid");
    test_assert(edge_is_valid_nvs_key("abcdefghijklmno"), "Length 15 is valid (max)");

    // Test invalid length (too long)
    test_assert(!edge_is_valid_nvs_key("abcdefghijklmnop"), "Length 16 is invalid");
    test_assert(!edge_is_valid_nvs_key("abcdefghijklmnopqrstuvwxyz"), "Length 26 is invalid");

    // Test boundary cases
    char key14[15];
    memset(key14, 'x', 14);
    key14[14] = '\0';
    test_assert(edge_is_valid_nvs_key(key14), "Length 14 is valid");

    char key15[16];
    memset(key15, 'x', 15);
    key15[15] = '\0';
    test_assert(edge_is_valid_nvs_key(key15), "Length 15 is valid");

    char key16[17];
    memset(key16, 'x', 16);
    key16[16] = '\0';
    test_assert(!edge_is_valid_nvs_key(key16), "Length 16 is invalid");
}

// =============================================================================
// Main test runner
// =============================================================================

int main() {
    printf("\n");
    printf("========================================\n");
    printf("Edge Case and Boundary Value Tests\n");
    printf("========================================\n\n");

    test_max_connections_edge_cases();
    printf("\n");
    test_probability_boundary_values();
    printf("\n");
    test_boolean_toggle_edge_cases();
    printf("\n");
    test_array_index_boundary_cases();
    printf("\n");
    test_nvs_key_length_boundaries();

    printf("\n");
    printf("========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);

    if (tests_failed == 0) {
        printf("\n✓ All edge case tests passed!\n");
        return 0;
    } else {
        printf("\n✗ %d tests failed!\n", tests_failed);
        return 1;
    }
}

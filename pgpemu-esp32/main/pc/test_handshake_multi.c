// Unit tests for handshake_multi connection management (PC build)
// Tests connection tracking, device state lookup, and multi-connection logic
#ifndef ESP_PLATFORM

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Mock ESP/FreeRTOS types and functions
#define ESP_OK 0
#define ESP_FAIL -1
#define CONFIG_BT_ACL_CONNECTIONS 4
#define portMAX_DELAY 0xFFFFFFFF
#define portTICK_PERIOD_MS 1

typedef int esp_err_t;
typedef unsigned char esp_bd_addr_t[6];
typedef unsigned long TickType_t;
typedef void* SemaphoreHandle_t;
typedef void* TaskHandle_t;

// Mock logging
#define ESP_LOGD(tag, fmt, ...) printf("D [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("W [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("E [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) printf("I [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOG_BUFFER_HEX(tag, data, len) printf("I [%s] buffer_hex\n", tag)

const char HANDSHAKE_TAG[] = "handshake";

// Mock FreeRTOS functions
static TickType_t mock_tick_count = 1000;

static TickType_t xTaskGetTickCount() {
    return mock_tick_count;
}

static unsigned long pdTICKS_TO_MS(TickType_t ticks) {
    return (unsigned long)ticks;
}

static SemaphoreHandle_t xSemaphoreCreateMutex() {
    return (SemaphoreHandle_t)1;
}

// Settings structures
typedef struct {
    SemaphoreHandle_t mutex;
    uint8_t target_active_connections;
    uint8_t log_level;
} GlobalSettings;

typedef struct {
    SemaphoreHandle_t mutex;
    esp_bd_addr_t bda;
    bool autocatch, autospin;
    uint8_t autospin_probability;
} DeviceSettings;

// Helper functions for settings module
bool get_setting(bool* var) {
    return var ? *var : false;
}

char* get_setting_log_value(bool* var) {
    return get_setting(var) ? "on" : "off";
}

uint8_t get_setting_uint8(uint8_t* var) {
    return var ? *var : 0;
}

// Connection management structures and implementation
static const size_t CERT_BUFFER_LEN = 378;

typedef struct {
    uint16_t conn_id;
    esp_bd_addr_t remote_bda;
    DeviceSettings* settings;
    int cert_state;
    bool has_reconnect_key;
    bool notify;
    uint8_t cert_buffer[378];
    uint8_t state_0_nonce[16];
    uint8_t the_challenge[16];
    uint8_t main_nonce[16];
    uint8_t outer_nonce[16];
    uint8_t session_key[16];
    uint8_t reconnect_challenge[32];
    TickType_t handshake_start, reconnection_at, connection_start, connection_end;
    bool auth_succeeded;
} client_state_t;

static int active_connections = 0;
#define MAX_CONNECTIONS CONFIG_BT_ACL_CONNECTIONS
static uint16_t conn_id_map[MAX_CONNECTIONS] = { 0 };
static client_state_t client_states[MAX_CONNECTIONS] = { 0 };

void init_handshake_multi() {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        conn_id_map[i] = 0xffff;
    }
    active_connections = 0;
}

int get_active_connections() {
    return active_connections;
}

int get_max_connections() {
    return MAX_CONNECTIONS;
}

client_state_t* get_client_state_entry(uint16_t conn_id) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] == conn_id) {
            return &client_states[i];
        }
    }
    return NULL;
}

client_state_t* get_client_state_entry_by_idx(int i) {
    if (i >= 0 && i < MAX_CONNECTIONS && conn_id_map[i] != 0xffff) {
        return &client_states[i];
    }
    return NULL;
}

client_state_t* get_client_state_entry_by_bda(esp_bd_addr_t bda) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] != 0xffff && memcmp(client_states[i].remote_bda, bda, sizeof(esp_bd_addr_t)) == 0) {
            return &client_states[i];
        }
    }
    return NULL;
}

// Mirrors pgp_gap.c's ESP_GAP_BLE_AUTH_CMPL_EVT decision: only remove the bond when this
// connection has never authenticated successfully before. An auth failure on a connection
// that already authenticated once is a transient hiccup on a still-live link, not a stale
// bond, and must not remove the bond.
bool should_remove_bond_on_auth_fail(client_state_t* entry) {
    return !(entry && entry->auth_succeeded);
}

client_state_t* get_or_create_client_state_entry(uint16_t conn_id) {
    // Check if exists
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] == conn_id) {
            return &client_states[i];
        }
    }

    // Look for empty slot
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] == 0xffff) {
            conn_id_map[i] = conn_id;
            memset(&client_states[i], 0, sizeof(client_state_t));
            client_states[i].conn_id = conn_id;
            client_states[i].handshake_start = xTaskGetTickCount();
            return &client_states[i];
        }
    }

    return NULL;
}

static void delete_client_state_entry(client_state_t* entry) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] == entry->conn_id) {
            conn_id_map[i] = 0xffff;
        }
    }
    memset(entry, 0, sizeof(client_state_t));
}

void connection_start(uint16_t conn_id) {
    active_connections++;
    client_state_t* entry = get_client_state_entry(conn_id);
    if (entry) {
        entry->connection_start = xTaskGetTickCount();
    }
}

void connection_stop(uint16_t conn_id) {
    active_connections--;
    if (active_connections < 0)
        active_connections = 0;

    client_state_t* entry = get_client_state_entry(conn_id);
    if (entry) {
        entry->connection_end = xTaskGetTickCount();
        entry->cert_state = 0;
        delete_client_state_entry(entry);
    }
}

// Test initialization
void test_init_handshake_multi() {
    printf("\n=== Test: Initialize Handshake Multi ===\n");

    init_handshake_multi();

    assert(get_active_connections() == 0);
    printf("✓ Active connections initialized to 0\n");

    assert(get_max_connections() == 4);
    printf("✓ Max connections is 4\n");

    // All slots should be empty
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        assert(get_client_state_entry_by_idx(i) == NULL);
    }
    printf("✓ All connection slots are empty\n");
}

// Test single connection
void test_single_connection() {
    printf("\n=== Test: Single Connection Management ===\n");

    init_handshake_multi();
    uint16_t conn_id = 0x0001;

    // Create connection
    client_state_t* entry = get_or_create_client_state_entry(conn_id);
    assert(entry != NULL);
    printf("✓ Connection created for conn_id 0x%04x\n", conn_id);

    assert(entry->conn_id == conn_id);
    printf("✓ Connection ID stored correctly\n");

    // Lookup by conn_id
    client_state_t* found = get_client_state_entry(conn_id);
    assert(found == entry);
    printf("✓ Connection found by conn_id\n");

    // Start connection
    connection_start(conn_id);
    assert(get_active_connections() == 1);
    printf("✓ Active connection count incremented\n");

    // Stop connection
    connection_stop(conn_id);
    assert(get_active_connections() == 0);
    printf("✓ Active connection count decremented\n");

    // Lookup should return NULL after stop
    found = get_client_state_entry(conn_id);
    assert(found == NULL);
    printf("✓ Connection cleaned up after stop\n");
}

// Test multiple connections
void test_multiple_connections() {
    printf("\n=== Test: Multiple Simultaneous Connections ===\n");

    init_handshake_multi();

    uint16_t conn_ids[] = { 0x0001, 0x0002, 0x0003, 0x0004 };
    client_state_t* entries[4];

    // Create 4 connections
    for (int i = 0; i < 4; i++) {
        entries[i] = get_or_create_client_state_entry(conn_ids[i]);
        assert(entries[i] != NULL);
        assert(entries[i]->conn_id == conn_ids[i]);
    }
    printf("✓ Created 4 simultaneous connections\n");

    // Start all connections
    for (int i = 0; i < 4; i++) {
        connection_start(conn_ids[i]);
    }
    assert(get_active_connections() == 4);
    printf("✓ All 4 connections are active\n");

    // Verify lookup by index
    for (int i = 0; i < 4; i++) {
        client_state_t* entry = get_client_state_entry_by_idx(i);
        assert(entry != NULL);
        assert(entry->conn_id == conn_ids[i]);
    }
    printf("✓ All connections accessible by index\n");

    // Stop one connection
    connection_stop(conn_ids[2]);
    assert(get_active_connections() == 3);
    printf("✓ Stopping one connection decrements count\n");

    // That slot should be empty now
    client_state_t* entry = get_client_state_entry_by_idx(2);
    assert(entry == NULL);
    printf("✓ Stopped connection slot is cleaned up\n");
}

// Test remote MAC address handling
void test_remote_bda_handling() {
    printf("\n=== Test: Remote BDA (MAC Address) Handling ===\n");

    init_handshake_multi();
    uint16_t conn_id = 0x0001;

    esp_bd_addr_t bda = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };

    client_state_t* entry = get_or_create_client_state_entry(conn_id);
    memcpy(entry->remote_bda, bda, sizeof(esp_bd_addr_t));

    // Verify MAC is stored
    for (int i = 0; i < 6; i++) {
        assert(entry->remote_bda[i] == bda[i]);
    }
    printf("✓ Remote MAC address stored correctly\n");

    // Test different MAC
    esp_bd_addr_t bda2 = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    memcpy(entry->remote_bda, bda2, sizeof(esp_bd_addr_t));

    for (int i = 0; i < 6; i++) {
        assert(entry->remote_bda[i] == bda2[i]);
    }
    printf("✓ Remote MAC address can be updated\n");
}

// Test max connections limit
void test_max_connections_limit() {
    printf("\n=== Test: Max Connections Limit ===\n");

    init_handshake_multi();

    // Try to create 5 connections (max is 4)
    for (int i = 0; i < 5; i++) {
        client_state_t* entry = get_or_create_client_state_entry(0x0001 + i);

        if (i < 4) {
            assert(entry != NULL);
            printf("✓ Connection %d created successfully\n", i + 1);
        } else {
            assert(entry == NULL);
            printf("✓ Connection %d rejected (max reached)\n", i + 1);
        }
    }
}

// Test connection state transitions
void test_connection_state_transitions() {
    printf("\n=== Test: Connection State Transitions ===\n");

    init_handshake_multi();
    uint16_t conn_id = 0x0001;

    // Create
    client_state_t* entry = get_or_create_client_state_entry(conn_id);
    assert(entry->cert_state == 0);
    printf("✓ New connection has cert_state = 0\n");

    // Update cert state
    entry->cert_state = 5;
    assert(entry->cert_state == 5);
    printf("✓ Cert state can be updated\n");

    // Start connection
    connection_start(conn_id);
    assert(entry->connection_start > 0);
    printf("✓ Connection start time recorded\n");

    // Stop connection clears cert state
    connection_stop(conn_id);

    // After stop, lookup should fail
    client_state_t* found = get_client_state_entry(conn_id);
    assert(found == NULL);
    printf("✓ Connection cleaned up\n");
}

// Test device settings linkage
void test_device_settings_linkage() {
    printf("\n=== Test: Device Settings Linkage ===\n");

    init_handshake_multi();
    uint16_t conn_id = 0x0001;

    // Create device settings
    DeviceSettings device_settings = { 0 };
    device_settings.autospin = true;
    device_settings.autocatch = false;
    device_settings.autospin_probability = 5;

    // Link to connection
    client_state_t* entry = get_or_create_client_state_entry(conn_id);
    entry->settings = &device_settings;

    assert(entry->settings != NULL);
    assert(entry->settings->autospin == true);
    assert(entry->settings->autocatch == false);
    printf("✓ Device settings linked to connection\n");

    // Modify settings through connection
    entry->settings->autospin = false;
    assert(device_settings.autospin == false);
    printf("✓ Settings modifications affect original device settings\n");
}

// Test lookup consistency
void test_lookup_consistency() {
    printf("\n=== Test: Lookup Consistency ===\n");

    init_handshake_multi();

    uint16_t conn_ids[] = { 0x0001, 0x0002, 0x0003 };

    for (int i = 0; i < 3; i++) {
        get_or_create_client_state_entry(conn_ids[i]);
        connection_start(conn_ids[i]);
    }

    // Lookup by conn_id and by index should return same results
    for (int i = 0; i < 3; i++) {
        client_state_t* by_id = get_client_state_entry(conn_ids[i]);

        // Find by index
        client_state_t* by_idx = NULL;
        for (int j = 0; j < MAX_CONNECTIONS; j++) {
            if (get_client_state_entry_by_idx(j) == by_id) {
                by_idx = get_client_state_entry_by_idx(j);
                break;
            }
        }

        assert(by_id == by_idx);
    }
    printf("✓ Lookup by conn_id and by index are consistent\n");
}

// Test auth-failure bond removal decision (regression test for the BLE scan-timeout bug:
// removing the bond on every failed auth stole the advertising slot from an already-live
// connection experiencing a transient hiccup).
void test_auth_fail_bond_removal_decision() {
    printf("\n=== Test: Auth-Fail Bond Removal Decision ===\n");

    init_handshake_multi();
    uint16_t conn_id = 0x0001;
    esp_bd_addr_t bda = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };

    client_state_t* entry = get_or_create_client_state_entry(conn_id);
    memcpy(entry->remote_bda, bda, sizeof(esp_bd_addr_t));

    // Fresh connection, never authenticated: a failure here is a genuine stale-bond
    // re-pair, so the bond must be removed.
    assert(should_remove_bond_on_auth_fail(get_client_state_entry_by_bda(bda)) == true);
    printf("✓ Fresh connection auth failure removes the bond\n");

    // Auth succeeds once on this connection.
    entry->auth_succeeded = true;

    // A later auth failure on the same still-live connection is a transient hiccup, so
    // the bond must be left in place.
    assert(should_remove_bond_on_auth_fail(get_client_state_entry_by_bda(bda)) == false);
    printf("✓ Auth failure after a prior success on the same connection keeps the bond\n");

    // No connection tracked for this address at all (e.g. already disconnected): treat as
    // a genuine stale-bond re-pair too.
    esp_bd_addr_t unknown_bda = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    assert(should_remove_bond_on_auth_fail(get_client_state_entry_by_bda(unknown_bda)) == true);
    printf("✓ Auth failure with no tracked connection removes the bond\n");
}

// Run all tests
int main() {
    printf("========================================\n");
    printf("Handshake Multi Connection Tests\n");
    printf("========================================\n");

    test_init_handshake_multi();
    test_single_connection();
    test_multiple_connections();
    test_remote_bda_handling();
    test_max_connections_limit();
    test_connection_state_transitions();
    test_device_settings_linkage();
    test_lookup_consistency();
    test_auth_fail_bond_removal_decision();

    printf("\n========================================\n");
    printf("✓ All handshake_multi tests passed!\n");
    printf("========================================\n");

    return 0;
}

#endif

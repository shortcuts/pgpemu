// Unit tests for the real known_devices.c over an in-memory NVS (PC build)
#ifndef ESP_PLATFORM

#define CONFIG_BT_ACL_CONNECTIONS 4
#define CONFIG_BT_SMP_MAX_BONDS 15

#include "../pgp_handshake_multi.h"
#include "esp_gap_ble_api.h"
#include "nvs_mem.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static client_state_t g_slots[2];
static client_state_t* connected_entry;
static uint8_t removed_bond[6];
static int remove_bond_calls;

client_state_t* get_client_state_entry_by_idx(int i) {
    return (i >= 0 && i < 2) ? &g_slots[i] : NULL;
}
int get_max_connections(void) {
    return 2;
}
client_state_t* get_client_state_entry_by_bda(esp_bd_addr_t bda) {
    return connected_entry && memcmp(connected_entry->remote_bda, bda, 6) == 0 ? connected_entry : NULL;
}
int esp_ble_remove_bond_device(unsigned char* bda) {
    memcpy(removed_bond, bda, 6);
    remove_bond_calls++;
    return 0;
}

#include "../config_storage.c"
#include "../known_devices.c"
#include "../nvs_helper.c"
#include "../settings.c"

static void addr(esp_bd_addr_t out, uint8_t n) {
    const esp_bd_addr_t base = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0 };
    memcpy(out, base, 6);
    out[5] = n;
}

static void reset(void) {
    nvs_mem_reset();
    memset(g_slots, 0, sizeof(g_slots));
    connected_entry = NULL;
    remove_bond_calls = 0;
    known_devices_init();
}

static size_t list(uint8_t* buf) {
    size_t len = 0;
    assert(known_devices_list(buf, KNOWN_DEVICES_LIST_MAX_LEN, &len) == KNOWN_DEVICES_OK);
    return len;
}

// n-th record of a list reply: its last address byte
static uint8_t listed(const uint8_t* buf, int n) {
    return buf[1 + n * KNOWN_DEVICE_WIRE_LEN + 5];
}

static void give_keys(uint8_t n) {
    esp_bd_addr_t a;
    uint8_t key[16] = { 1 }, chal[32] = { 2 };
    addr(a, n);
    assert(persist_device_session_keys(a, key, chal));
}

static void test_empty(void) {
    uint8_t buf[KNOWN_DEVICES_LIST_MAX_LEN];
    reset();
    assert(list(buf) == 1 && buf[0] == 0);
}

static void test_touch_orders_most_recent_first(void) {
    uint8_t buf[KNOWN_DEVICES_LIST_MAX_LEN];
    esp_bd_addr_t a, b;
    addr(a, 1);
    addr(b, 2);
    reset();
    known_devices_touch(a);
    known_devices_touch(b);
    assert(list(buf) == 1 + 2 * KNOWN_DEVICE_WIRE_LEN && buf[0] == 2);
    assert(listed(buf, 0) == 2 && listed(buf, 1) == 1);
    assert(buf[1 + 6] == 2 && buf[1 + 7] == 0);  // seq u32 LE of the newest

    known_devices_touch(a);
    list(buf);
    assert(listed(buf, 0) == 1 && listed(buf, 1) == 2);
}

static void test_touch_of_latest_writes_nothing(void) {
    esp_bd_addr_t a;
    addr(a, 1);
    reset();
    known_devices_touch(a);
    nvs_mem_reset();  // flash is empty; a write by the next touch would bring the record back
    known_devices_touch(a);
    known_devices_init();
    assert(find(a) < 0);
}

static void test_zero_address_is_ignored(void) {
    uint8_t buf[KNOWN_DEVICES_LIST_MAX_LEN];
    esp_bd_addr_t zero = { 0 };
    reset();
    known_devices_touch(zero);
    list(buf);
    assert(buf[0] == 0);
}

static void test_rename_and_persistence(void) {
    uint8_t buf[KNOWN_DEVICES_LIST_MAX_LEN];
    esp_bd_addr_t a, missing;
    addr(a, 1);
    addr(missing, 9);
    reset();
    known_devices_touch(a);
    assert(known_devices_rename(a, "Pixel 8", 7) == KNOWN_DEVICES_OK);
    assert(known_devices_rename(missing, "x", 1) == KNOWN_DEVICES_NOT_FOUND);

    known_devices_init();  // simulated reboot
    list(buf);
    assert(buf[0] == 1 && strcmp((const char*)buf + 1 + 11, "Pixel 8") == 0);

    known_devices_touch(a);  // a touch keeps the name
    list(buf);
    assert(strcmp((const char*)buf + 1 + 11, "Pixel 8") == 0);

    assert(known_devices_rename(a, "", 0) == KNOWN_DEVICES_OK);
    list(buf);
    assert(buf[1 + 11] == 0);

    assert(known_devices_rename(a, "0123456789abcdefXYZ", 19) == KNOWN_DEVICES_OK);
    list(buf);
    assert(strcmp((const char*)buf + 1 + 11, "0123456789abcde") == 0);
}

static void test_forget(void) {
    uint8_t buf[KNOWN_DEVICES_LIST_MAX_LEN];
    esp_bd_addr_t a, missing;
    DeviceSettings settings = { 0 };
    addr(a, 1);
    addr(missing, 9);
    reset();
    known_devices_touch(a);
    give_keys(1);
    memcpy(g_slots[1].remote_bda, a, 6);
    g_slots[1].has_settings = true;
    g_slots[1].settings.mutex = xSemaphoreCreateMutex();
    assert(write_device_settings_to_nvs(1));
    assert(read_stored_device_settings(a, &settings) && !settings.autospin);

    assert(known_devices_forget(missing) == KNOWN_DEVICES_NOT_FOUND);

    connected_entry = &g_slots[0];
    memcpy(connected_entry->remote_bda, a, 6);
    assert(known_devices_forget(a) == KNOWN_DEVICES_BUSY);
    assert(has_cached_session(a) && remove_bond_calls == 0);
    list(buf);
    assert(buf[0] == 1 && buf[1 + 10] == 1);  // connected flag

    connected_entry = NULL;
    assert(known_devices_forget(a) == KNOWN_DEVICES_OK);
    assert(!has_cached_session(a));
    assert(read_stored_device_settings(a, &settings) && settings.autospin);
    assert(remove_bond_calls == 1 && memcmp(removed_bond, a, 6) == 0);
    list(buf);
    assert(buf[0] == 0);
    known_devices_init();
    list(buf);
    assert(buf[0] == 0);
}

static void test_full_index_evicts_least_recently_seen(void) {
    uint8_t buf[KNOWN_DEVICES_LIST_MAX_LEN];
    esp_bd_addr_t a;
    reset();
    for (uint8_t n = 1; n <= KNOWN_DEVICES_MAX; n++) {
        addr(a, n);
        known_devices_touch(a);
    }
    give_keys(1);
    give_keys(2);
    addr(a, 1);
    known_devices_touch(a);  // 1 is now the newest, 2 the oldest
    assert(remove_bond_calls == 0);

    addr(a, 100);
    known_devices_touch(a);
    assert(remove_bond_calls == 1 && removed_bond[5] == 2);
    addr(a, 2);
    assert(!has_cached_session(a));
    addr(a, 1);
    assert(has_cached_session(a));

    list(buf);
    assert(buf[0] == KNOWN_DEVICES_MAX && listed(buf, 0) == 100 && listed(buf, 1) == 1);
    for (int k = 0; k < buf[0]; k++) {
        assert(listed(buf, k) != 2);
    }
    known_devices_init();
    list(buf);
    assert(buf[0] == KNOWN_DEVICES_MAX && listed(buf, 0) == 100);
}

static void test_connected_record_is_not_evicted(void) {
    uint8_t buf[KNOWN_DEVICES_LIST_MAX_LEN];
    esp_bd_addr_t a;
    reset();
    for (uint8_t n = 1; n <= KNOWN_DEVICES_MAX; n++) {
        addr(a, n);
        known_devices_touch(a);
    }
    addr(a, 1);  // the oldest is connected
    connected_entry = &g_slots[0];
    memcpy(connected_entry->remote_bda, a, 6);
    addr(a, 100);
    known_devices_touch(a);
    assert(removed_bond[5] == 2);
    list(buf);
    int has_one = 0;
    for (int k = 0; k < buf[0]; k++) {
        has_one += listed(buf, k) == 1;
    }
    assert(has_one == 1);
}

static void test_contains(void) {
    esp_bd_addr_t a, missing;
    addr(a, 1);
    addr(missing, 9);
    reset();
    assert(!known_devices_contains(a));
    known_devices_touch(a);
    assert(known_devices_contains(a) && !known_devices_contains(missing));
    assert(known_devices_forget(a) == KNOWN_DEVICES_OK);
    assert(!known_devices_contains(a));
}

static void test_list_needs_room(void) {
    uint8_t buf[KNOWN_DEVICES_LIST_MAX_LEN];
    size_t len = 0;
    reset();
    assert(known_devices_list(buf, KNOWN_DEVICES_LIST_MAX_LEN - 1, &len) == KNOWN_DEVICES_INTERNAL);
}

int main(void) {
    test_empty();
    test_touch_orders_most_recent_first();
    test_touch_of_latest_writes_nothing();
    test_zero_address_is_ignored();
    test_rename_and_persistence();
    test_forget();
    test_full_index_evicts_least_recently_seen();
    test_connected_record_is_not_evicted();
    test_list_needs_room();
    test_contains();
    printf("test_known_devices: all passed\n");
    return 0;
}

#endif

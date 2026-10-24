// Unit tests for the real config_storage.c over an in-memory NVS (PC build)
#ifndef ESP_PLATFORM

#define CONFIG_BT_ACL_CONNECTIONS 4

#include "../pgp_handshake_multi.h"
#include "nvs_mem.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static client_state_t g_slots[2];

client_state_t* get_client_state_entry_by_idx(int i) {
    return (i >= 0 && i < 2) ? &g_slots[i] : NULL;
}
int get_max_connections(void) {
    return 2;
}

#include "../config_storage.c"
#include "../nvs_helper.c"
#include "../settings.c"

static esp_bd_addr_t BDA_A = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01 };
static esp_bd_addr_t BDA_B = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x02 };

static void test_device_key_fits_nvs_and_differs(void) {
    char a[16], a2[16], b[16], c[16];
    make_device_key_for_option("spin", BDA_A, a);
    make_device_key_for_option("spin", BDA_A, a2);
    make_device_key_for_option("catch", BDA_A, b);
    make_device_key_for_option("spin", BDA_B, c);
    assert(strlen(a) == 15 && strcmp(a, a2) == 0);
    assert(strcmp(a, b) != 0 && strcmp(a, c) != 0);
}

static void test_session_keys_round_trip_and_clear(void) {
    uint8_t key[16], chal[32], k2[16], c2[32];
    memset(key, 0x5a, 16);
    memset(chal, 0xc3, 32);
    nvs_mem_reset();

    assert(!has_cached_session(BDA_A));
    assert(persist_device_session_keys(BDA_A, key, chal));
    assert(has_cached_session(BDA_A));
    assert(!has_cached_session(BDA_B));

    assert(retrieve_device_session_keys(BDA_A, k2, c2));
    assert(memcmp(k2, key, 16) == 0 && memcmp(c2, chal, 32) == 0);
    assert(!retrieve_device_session_keys(BDA_B, k2, c2));

    assert(clear_device_session(BDA_A));
    assert(!has_cached_session(BDA_A));
    assert(!retrieve_device_session_keys(BDA_A, k2, c2));

    assert(!persist_device_session_keys(BDA_A, NULL, chal));
    assert(!retrieve_device_session_keys(BDA_A, NULL, c2));
}

static void test_erase_device_data_clears_one_address(void) {
    DeviceSettings out = { 0 };
    uint8_t key[16] = { 1 }, chal[32] = { 2 };
    nvs_mem_reset();
    memset(g_slots, 0, sizeof(g_slots));
    for (int i = 0; i < 2; i++) {
        memcpy(g_slots[i].remote_bda, i ? BDA_B : BDA_A, 6);
        g_slots[i].has_settings = true;
        g_slots[i].settings.mutex = xSemaphoreCreateMutex();
        g_slots[i].settings.autospin = false;
        assert(persist_device_session_keys(g_slots[i].remote_bda, key, chal));
    }
    assert(write_devices_settings_to_nvs());

    assert(erase_device_data(BDA_A));
    assert(!has_cached_session(BDA_A));
    assert(read_stored_device_settings(BDA_A, &out) && out.autospin);  // back to the default
    assert(has_cached_session(BDA_B));
    assert(read_stored_device_settings(BDA_B, &out) && !out.autospin);
    assert(erase_device_data(BDA_A));  // nothing left to erase is not an error
}

static void test_lifetime_stats_blob_round_trip_and_erase(void) {
    esp_bd_addr_t a = { 1, 2, 3, 4, 5, 6 }, b = { 1, 2, 3, 4, 5, 7 };
    uint8_t blob[LIFETIME_STATS_BLOB_LEN], in[LIFETIME_STATS_BLOB_LEN];
    for (int i = 0; i < LIFETIME_STATS_BLOB_LEN; i++) {
        in[i] = (uint8_t)(i + 1);
    }
    nvs_mem_reset();
    assert(!read_lifetime_stats(a, blob));
    assert(write_lifetime_stats(a, in) && write_lifetime_stats(b, in));
    assert(read_lifetime_stats(a, blob) && memcmp(blob, in, sizeof(in)) == 0);
    assert(erase_device_data(a));
    assert(!read_lifetime_stats(a, blob) && read_lifetime_stats(b, blob));
}

static void test_persist_fails_when_commit_fails(void) {
    uint8_t key[16] = { 1 }, chal[32] = { 2 };
    nvs_mem_reset();
    nvs_mem_fail_commit = 1;
    assert(!persist_device_session_keys(BDA_A, key, chal));
    nvs_mem_reset();
}

static void test_global_settings_round_trip(void) {
    nvs_mem_reset();
    init_global_settings();
    global_settings.log_level = 3;
    global_settings.target_active_connections = 2;
    global_settings.advertising_enabled = false;
    global_settings_ready();
    assert(write_global_settings_to_nvs());

    global_settings.log_level = 1;
    global_settings.target_active_connections = 1;
    global_settings.advertising_enabled = true;
    read_stored_global_settings(true);
    assert(global_settings.log_level == 3);
    assert(global_settings.target_active_connections == 2);
    assert(!global_settings.advertising_enabled);
}

static void test_global_settings_reject_out_of_range(void) {
    nvs_handle_t h;
    nvs_mem_reset();
    assert(nvs_open("global_settings", NVS_READWRITE, &h) == ESP_OK);
    nvs_set_u8(h, "llevel", 9);
    nvs_set_u8(h, "maxcon", 0);
    nvs_close(h);
    global_settings.log_level = 2;
    global_settings.target_active_connections = 1;
    read_stored_global_settings(true);
    assert(global_settings.log_level == 2 && global_settings.target_active_connections == 1);
}

static void test_device_settings_round_trip(void) {
    DeviceSettings out = { 0 };
    nvs_mem_reset();
    memset(g_slots, 0, sizeof(g_slots));
    memcpy(g_slots[0].remote_bda, BDA_A, 6);
    g_slots[0].has_settings = true;
    g_slots[0].settings.mutex = xSemaphoreCreateMutex();
    g_slots[0].settings.autospin = false;
    g_slots[0].settings.autocatch = true;
    memcpy(g_slots[1].remote_bda, BDA_B, 6);
    g_slots[1].has_settings = true;
    g_slots[1].settings.mutex = xSemaphoreCreateMutex();
    g_slots[1].settings.autospin = true;
    assert(write_devices_settings_to_nvs());

    assert(read_stored_device_settings(BDA_A, &out));
    assert(!out.autospin && out.autocatch);
    assert(read_stored_device_settings(BDA_B, &out));
    assert(out.autospin && !out.autocatch);
}

static void test_device_settings_defaults_and_invalid_bda(void) {
    DeviceSettings out = { 0 };
    esp_bd_addr_t zero = { 0 };
    nvs_mem_reset();
    assert(!read_stored_device_settings(BDA_A, &out));  // namespace never written
    assert(out.autospin && out.autocatch);              // defaults survive
    assert(!read_stored_device_settings(zero, &out));
    assert(!read_stored_device_settings(BDA_A, NULL));
}

static void test_write_single_device_slot(void) {
    DeviceSettings out = { 0 };
    nvs_mem_reset();
    memset(g_slots, 0, sizeof(g_slots));
    for (int i = 0; i < 2; i++) {
        memcpy(g_slots[i].remote_bda, i ? BDA_B : BDA_A, 6);
        g_slots[i].has_settings = true;
        g_slots[i].settings.mutex = xSemaphoreCreateMutex();
        g_slots[i].settings.autospin = i == 1;
    }
    assert(write_device_settings_to_nvs(1));
    assert(read_stored_device_settings(BDA_B, &out) && out.autospin);
    // slot 0 holds autospin=false in RAM: a read that still yields the default true proves it was not written
    assert(read_stored_device_settings(BDA_A, &out) && out.autospin);

    g_slots[0].has_settings = false;
    assert(!write_device_settings_to_nvs(0));
    assert(!write_device_settings_to_nvs(5));
}

int main(void) {
    test_device_key_fits_nvs_and_differs();
    test_session_keys_round_trip_and_clear();
    test_erase_device_data_clears_one_address();
    test_lifetime_stats_blob_round_trip_and_erase();
    test_persist_fails_when_commit_fails();
    test_global_settings_round_trip();
    test_global_settings_reject_out_of_range();
    test_device_settings_round_trip();
    test_device_settings_defaults_and_invalid_bda();
    test_write_single_device_slot();
    printf("test_config_storage: all passed\n");
    return 0;
}

#endif

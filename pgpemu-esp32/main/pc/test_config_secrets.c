// Unit tests for the real config_secrets.c over an in-memory NVS (PC build)
#ifndef ESP_PLATFORM

#include "../config_secrets.c"
#include "../nvs_helper.c"
#include "../secrets.c"
#include "nvs_mem.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// the stub nvs_flash.h is pulled in by config_secrets.c
static void store_secrets(void) {
    nvs_handle_t h;
    uint8_t mac[6] = { 0xe4, 0xcb, 0x0b, 0xc3, 0x59, 0x63 };
    uint8_t key[16] = { 1, 2, 3 };
    uint8_t blob[256] = { 9 };
    assert(nvs_open("pgpsecret", NVS_READWRITE, &h) == ESP_OK);
    assert(nvs_set_blob(h, "name", "PKLMGOPLUS", 11) == ESP_OK);
    assert(nvs_set_blob(h, "mac", mac, 6) == ESP_OK);
    assert(nvs_set_blob(h, "dkey", key, 16) == ESP_OK);
    assert(nvs_set_blob(h, "blob", blob, 256) == ESP_OK);
    nvs_close(h);
}

static void test_read_from_secrets_partition(void) {
    nvs_mem_reset();
    store_secrets();
    char name[16];
    uint8_t mac[6], key[16], blob[256];
    assert(init_secrets_partition());
    assert(read_secrets(name, mac, key, blob));
    assert(strcmp(name, "PKLMGOPLUS") == 0);
    assert(mac[0] == 0xe4 && mac[5] == 0x63 && key[2] == 3 && blob[0] == 9);
    assert(strcmp(nvs_mem_last_partition, "pgpsecret") == 0);
}

static void test_corrupt_partition_is_not_erased(void) {
    nvs_mem_reset();
    store_secrets();
    nvs_mem_init_part_err = ESP_ERR_NVS_NO_FREE_PAGES;
    char name[16];
    uint8_t mac[6], key[16], blob[256];
    assert(!init_secrets_partition());
    // stored data stays: a later fixed partition still reads it
    nvs_mem_init_part_err = ESP_OK;
    assert(read_secrets(name, mac, key, blob));
}

static void test_reset_clears_and_closes(void) {
    nvs_mem_reset();
    store_secrets();
    char name[16];
    uint8_t mac[6], key[16], blob[256];
    assert(reset_secrets());
    assert(!read_secrets(name, mac, key, blob));
    for (int h = 1; h < 8; h++) {
        assert(!nvs_mem_handle_ns[h][0]);
    }
}

static const uint8_t WRITE_MAC[6] = { 0xe4, 0xcb, 0x0b, 0xc3, 0x59, 0x63 };

static void test_write_round_trip(void) {
    nvs_mem_reset();
    uint8_t key[16] = { 7, 8 }, blob[256] = { 5 };
    blob[255] = 6;
    assert(write_secrets("MYCLONE", WRITE_MAC, key, blob));
    char name[16];
    uint8_t mac[6], rkey[16], rblob[256];
    assert(read_secrets(name, mac, rkey, rblob));
    assert(strcmp(name, "MYCLONE") == 0 && memcmp(mac, WRITE_MAC, 6) == 0);
    assert(memcmp(rkey, key, 16) == 0 && memcmp(rblob, blob, 256) == 0);
    for (int h = 1; h < 8; h++) {
        assert(!nvs_mem_handle_ns[h][0]);
    }
}

static void test_write_failure_leaves_no_secrets(void) {
    uint8_t key[16] = { 7 }, blob[256] = { 5 };
    char name[16];
    uint8_t mac[6], rkey[16], rblob[256];
    for (int fail_at = 1; fail_at <= 4; fail_at++) {
        nvs_mem_reset();
        nvs_mem_fail_set = fail_at;
        assert(!write_secrets("MYCLONE", WRITE_MAC, key, blob));
        assert(!read_secrets(name, mac, rkey, rblob));
    }
    nvs_mem_reset();
    nvs_mem_fail_commit = 1;
    assert(!write_secrets("MYCLONE", WRITE_MAC, key, blob));
    assert(!read_secrets(name, mac, rkey, rblob));
}

int main(void) {
    test_read_from_secrets_partition();
    test_corrupt_partition_is_not_erased();
    test_reset_clears_and_closes();
    test_write_round_trip();
    test_write_failure_leaves_no_secrets();
    printf("test_config_secrets ok\n");
    return 0;
}

#endif

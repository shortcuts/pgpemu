// Unit tests for the real settings.c (PC build)
#ifndef ESP_PLATFORM

#include "../pgp_handshake_multi.h"
#include "../settings.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static client_state_t g_slots[2];

client_state_t* get_client_state_entry_by_idx(int i) {
    return (i >= 0 && i < 2) ? &g_slots[i] : NULL;
}

#include "../settings.c"

static void test_toggle_and_null(void) {
    bool v = false;
    assert(toggle_setting(&v) && v);
    assert(toggle_setting(&v) && !v);
    assert(!toggle_setting(NULL));
}

static void test_cycle_log_level_wraps_after_3(void) {
    uint8_t lvl = 1;
    assert(cycle_log_level(&lvl) && lvl == 2);
    assert(cycle_log_level(&lvl) && lvl == 3);
    assert(cycle_log_level(&lvl) && lvl == 1);
    assert(!cycle_log_level(NULL));
}

static void test_get_set(void) {
    uint8_t n = 0;
    bool b = false;
    assert(set_setting_uint8(&n, 7) && get_setting_uint8(&n) == 7);
    assert(set_setting_bool(&b, true) && get_setting(&b));
    assert(strcmp(get_setting_log_value(&b), "on") == 0);
    assert(!set_setting_uint8(NULL, 1) && get_setting_uint8(NULL) == 0);
}

static void test_device_toggle_is_per_slot(void) {
    memset(g_slots, 0, sizeof(g_slots));
    for (int i = 0; i < 2; i++) {
        g_slots[i].settings.mutex = xSemaphoreCreateMutex();
        g_slots[i].has_settings = true;
    }
    assert(toggle_device_autospin(0) == true);
    assert(g_slots[0].settings.autospin && !g_slots[1].settings.autospin);
    assert(toggle_device_autospin(0) == false);
    assert(toggle_device_autocatch(1) == true);
    assert(g_slots[1].settings.autocatch && !g_slots[0].settings.autocatch);

    // unknown slot or slot without loaded settings: no change, returns false
    g_slots[1].has_settings = false;
    assert(!toggle_device_autocatch(1) && g_slots[1].settings.autocatch);
    assert(!toggle_device_autospin(5));
}

int main(void) {
    init_global_settings();
    test_toggle_and_null();
    test_cycle_log_level_wraps_after_3();
    test_get_set();
    test_device_toggle_is_per_slot();
    printf("test_settings: all passed\n");
    return 0;
}

#endif

// Unit tests for the real forget_pairings.c: the erase runs only after a requested software reset (PC build)
#ifndef ESP_PLATFORM

#include "nvs_flash.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static int erase_calls;
static int erase_result;
#define nvs_flash_erase counted_erase
static int counted_erase(void) {
    erase_calls++;
    return erase_result;
}

#include "../forget_pairings.c"

static void setup(int reason) {
    erase_calls = erase_result = 0;
    forget_request = 0;
    esp_reset_reason_value = reason;
}

static void test_requested_sw_reset_erases_once(void) {
    setup(ESP_RST_SW);
    esp_restart_calls = 0;
    forget_pairings_and_restart();
    assert(esp_restart_calls == 1 && forget_request == 0xF0261E57u);
    forget_pairings_if_requested();
    assert(erase_calls == 1 && forget_request == 0);
    forget_pairings_if_requested();
    assert(erase_calls == 1);
}

static void test_no_magic_does_not_erase(void) {
    setup(ESP_RST_SW);
    forget_pairings_if_requested();
    assert(erase_calls == 0);
}

static void test_magic_after_power_on_does_not_erase(void) {
    setup(1);
    forget_request = 0xF0261E57u;
    forget_pairings_if_requested();
    assert(erase_calls == 0 && forget_request == 0);
}

static void test_failed_erase_clears_request(void) {
    setup(ESP_RST_SW);
    forget_request = 0xF0261E57u;
    erase_result = -1;
    forget_pairings_if_requested();
    assert(erase_calls == 1 && forget_request == 0);
}

int main(void) {
    test_requested_sw_reset_erases_once();
    test_no_magic_does_not_erase();
    test_magic_after_power_on_does_not_erase();
    test_failed_erase_clears_request();
    printf("test_forget_pairings: all passed\n");
    return 0;
}

#endif

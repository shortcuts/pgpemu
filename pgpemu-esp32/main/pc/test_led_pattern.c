// Unit tests for the status LED pattern table (PC build)
#ifndef ESP_PLATFORM

#include "../pgp_led_pattern.c"

#include <assert.h>
#include <stdio.h>

static void test_select_priority(void) {
    assert(led_pattern_select(false, true, true, ADV_ON, 3) == LED_PAT_OFF);
    assert(led_pattern_select(true, true, true, ADV_ON, 3) == LED_PAT_CRASHED);
    assert(led_pattern_select(true, true, false, ADV_ON, 3) == LED_PAT_PROVISIONING);
    assert(led_pattern_select(true, false, false, ADV_ON, 1) == LED_PAT_CONNECTED);
    assert(led_pattern_select(true, false, false, ADV_OFF, 2) == LED_PAT_CONNECTED);
    assert(led_pattern_select(true, false, false, ADV_ON, 0) == LED_PAT_ADVERTISING);
    assert(led_pattern_select(true, false, false, ADV_OFF, 0) == LED_PAT_OFF);
    assert(led_pattern_select(true, false, false, ADV_STARTING, 0) == LED_PAT_OFF);
    assert(led_pattern_select(true, false, false, ADV_STOPPING, 0) == LED_PAT_OFF);
}

static void test_advertising_levels(void) {
    assert(led_pattern_level(LED_PAT_ADVERTISING, 0, 0));
    assert(led_pattern_level(LED_PAT_ADVERTISING, 0, 499));
    assert(!led_pattern_level(LED_PAT_ADVERTISING, 0, 500));
    assert(!led_pattern_level(LED_PAT_ADVERTISING, 0, 1999));
    assert(led_pattern_level(LED_PAT_ADVERTISING, 0, 2000));
}

static void test_connected_levels(void) {
    // one pulse: on 0-199, off 200-1599, cycle 1600
    assert(led_pattern_level(LED_PAT_CONNECTED, 1, 0));
    assert(led_pattern_level(LED_PAT_CONNECTED, 1, 199));
    assert(!led_pattern_level(LED_PAT_CONNECTED, 1, 200));
    assert(!led_pattern_level(LED_PAT_CONNECTED, 1, 1599));
    assert(led_pattern_level(LED_PAT_CONNECTED, 1, 1600));
    // two pulses: on 0-199 and 400-599, cycle 2000
    assert(!led_pattern_level(LED_PAT_CONNECTED, 2, 399));
    assert(led_pattern_level(LED_PAT_CONNECTED, 2, 400));
    assert(led_pattern_level(LED_PAT_CONNECTED, 2, 599));
    assert(!led_pattern_level(LED_PAT_CONNECTED, 2, 600));
    assert(!led_pattern_level(LED_PAT_CONNECTED, 2, 1999));
    assert(led_pattern_level(LED_PAT_CONNECTED, 2, 2000));
    // four pulses: last on 1200-1399, gap to 2799, cycle 2800
    assert(led_pattern_level(LED_PAT_CONNECTED, 4, 1200));
    assert(!led_pattern_level(LED_PAT_CONNECTED, 4, 1400));
    assert(!led_pattern_level(LED_PAT_CONNECTED, 4, 2799));
    assert(led_pattern_level(LED_PAT_CONNECTED, 4, 2800));
    // clamp: 0 acts as 1, 9 acts as 4
    assert(!led_pattern_level(LED_PAT_CONNECTED, 0, 400));
    assert(led_pattern_level(LED_PAT_CONNECTED, 9, 1200));
    assert(led_pattern_level(LED_PAT_CONNECTED, 9, 2800));
}

static void test_provisioning_levels(void) {
    assert(led_pattern_level(LED_PAT_PROVISIONING, 0, 0));
    assert(led_pattern_level(LED_PAT_PROVISIONING, 0, 99));
    assert(!led_pattern_level(LED_PAT_PROVISIONING, 0, 100));
    assert(!led_pattern_level(LED_PAT_PROVISIONING, 0, 199));
    assert(led_pattern_level(LED_PAT_PROVISIONING, 0, 200));
}

static void test_crashed_levels(void) {
    assert(led_pattern_level(LED_PAT_CRASHED, 0, 0));
    assert(led_pattern_level(LED_PAT_CRASHED, 0, 499));
    assert(!led_pattern_level(LED_PAT_CRASHED, 0, 500));
    assert(!led_pattern_level(LED_PAT_CRASHED, 0, 999));
    assert(led_pattern_level(LED_PAT_CRASHED, 0, 1000));
    assert(led_pattern_level(LED_PAT_CRASHED, 0, 2000));
    assert(!led_pattern_level(LED_PAT_CRASHED, 0, 2999));
    assert(!led_pattern_level(LED_PAT_CRASHED, 0, 3000));
}

static void test_off_is_dark(void) {
    assert(!led_pattern_level(LED_PAT_OFF, 0, 0));
    assert(!led_pattern_level(LED_PAT_OFF, 3, 1234));
}

int main(void) {
    test_select_priority();
    test_advertising_levels();
    test_connected_levels();
    test_provisioning_levels();
    test_crashed_levels();
    test_off_is_dark();
    printf("test_led_pattern: all passed\n");
    return 0;
}

#endif

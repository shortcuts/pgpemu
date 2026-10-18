// Unit tests for the advertising state machine (PC build)
#ifndef ESP_PLATFORM

#include "../pgp_adv_state.c"

#include <assert.h>
#include <stdio.h>

static void check(adv_state_t from, bool want, adv_action_t act, adv_state_t to) {
    adv_state_t s = from;
    assert(adv_decide(&s, want) == act);
    assert(s == to);
}

static void test_decide_table(void) {
    check(ADV_OFF, true, ADV_ACT_START, ADV_STARTING);
    check(ADV_OFF, false, ADV_ACT_NONE, ADV_OFF);
    check(ADV_STARTING, true, ADV_ACT_NONE, ADV_STARTING);
    check(ADV_STARTING, false, ADV_ACT_STOP, ADV_STOPPING);
    check(ADV_ON, true, ADV_ACT_NONE, ADV_ON);
    check(ADV_ON, false, ADV_ACT_STOP, ADV_STOPPING);
    check(ADV_STOPPING, true, ADV_ACT_NONE, ADV_STOPPING);
    check(ADV_STOPPING, false, ADV_ACT_NONE, ADV_STOPPING);
}

static void test_backoff_doubles_then_caps(void) {
    assert(adv_backoff_ms(0) == 1000);
    assert(adv_backoff_ms(1) == 2000);
    assert(adv_backoff_ms(2) == 4000);
    assert(adv_backoff_ms(5) == 30000);
    assert(adv_backoff_ms(255) == 30000);
}

int main(void) {
    test_decide_table();
    test_backoff_doubles_then_caps();
    printf("test_adv_state: all passed\n");
    return 0;
}

#endif

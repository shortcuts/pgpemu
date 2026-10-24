// Unit tests for per-connection runtime stats (PC build)
#ifndef ESP_PLATFORM

#define CONFIG_BT_ACL_CONNECTIONS 4

#include "../lifetime_stats.h"

static int last_kind = -1, add_calls;
static uint16_t last_conn;
void lifetime_stats_add(uint16_t conn_id, lifetime_stat_t kind) {
    last_conn = conn_id;
    last_kind = kind;
    add_calls++;
}

#include "../buf_writer.c"
#include "../stats.c"

#include <assert.h>
#include <string.h>

// Bluedroid reuses conn_id values: a new phone must not inherit the old phone's counters.
static void test_reused_conn_id_starts_at_zero(void) {
    Stats s;
    increment_caught(1);
    increment_fled(1);
    increment_spin(1);
    assert(stats_get_for_conn(1, &s));
    assert(s.caught == 1 && s.fled == 1 && s.spin == 1);

    stats_clear_for_conn(1);
    assert(!stats_get_for_conn(1, &s));

    increment_spin(1);
    assert(stats_get_for_conn(1, &s));
    assert(s.caught == 0 && s.fled == 0 && s.spin == 1);
}

static void test_clear_keeps_other_conns(void) {
    Stats s;
    increment_caught(2);
    increment_caught(3);
    increment_caught(3);
    stats_clear_for_conn(2);
    assert(!stats_get_for_conn(2, &s));
    assert(stats_get_for_conn(3, &s));
    assert(s.caught == 2);
}

static void test_new_counters_and_lifetime_hook(void) {
    Stats s;
    stats_clear_for_conn(4);
    add_calls = 0;
    increment_bag_full(4);
    assert(last_kind == LIFETIME_BAG_FULL && last_conn == 4);
    increment_box_full(4);
    assert(last_kind == LIFETIME_BOX_FULL);
    increment_balls_empty(4);
    increment_balls_empty(4);
    assert(last_kind == LIFETIME_BALLS_EMPTY && add_calls == 4);
    increment_caught(4);
    assert(last_kind == LIFETIME_CAUGHT);
    increment_fled(4);
    assert(last_kind == LIFETIME_FLED);
    increment_spin(4);
    assert(last_kind == LIFETIME_SPIN && add_calls == 7);
    assert(stats_get_for_conn(4, &s));
    assert(s.caught == 1 && s.fled == 1 && s.spin == 1);
    assert(s.bag_full == 1 && s.box_full == 1 && s.balls_empty == 2);

    char buf[256];
    size_t len = stats_format_runtime(buf, sizeof(buf));
    assert(len > 0 && strstr(buf, "- Balls empty: 2\n") != NULL);
    stats_clear_for_conn(4);
}

int main(void) {
    test_new_counters_and_lifetime_hook();
    test_reused_conn_id_starts_at_zero();
    test_clear_keeps_other_conns();
    printf("test_stats: all passed\n");
    return 0;
}

#endif

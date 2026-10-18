// Unit tests for per-connection runtime stats (PC build)
#ifndef ESP_PLATFORM

#define CONFIG_BT_ACL_CONNECTIONS 4

#include "../buf_writer.c"
#include "../stats.c"

#include <assert.h>

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

int main(void) {
    test_reused_conn_id_starts_at_zero();
    test_clear_keeps_other_conns();
    printf("test_stats: all passed\n");
    return 0;
}

#endif

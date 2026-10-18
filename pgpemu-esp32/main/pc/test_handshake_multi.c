// Unit tests for the real pgp_handshake_multi.c connection table (PC build)
#ifndef ESP_PLATFORM

#define CONFIG_BT_ACL_CONNECTIONS 4

#include "../pgp_autobutton.h"
#include "../pgp_gap.h"
#include "../settings.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// ---- mocks of everything the unit under test calls ----
static int advertise_calls, purged_conn, stats_cleared_conn, clear_session_calls, disconnect_calls;
static esp_bd_addr_t cleared_bda;

GlobalSettings global_settings = { .target_active_connections = 2 };
uint8_t get_setting_uint8(uint8_t* var) {
    return *var;
}
char* get_setting_log_value(bool* var) {
    return *var ? "on" : "off";
}
void advertise_if_needed(void) {
    advertise_calls++;
}
void purge_button_queue_for_connection(uint16_t conn_id) {
    purged_conn = conn_id;
}
void stats_clear_for_conn(uint16_t conn_id) {
    stats_cleared_conn = conn_id;
}
bool clear_device_session(esp_bd_addr_t bda) {
    clear_session_calls++;
    memcpy(cleared_bda, bda, 6);
    return true;
}
int esp_ble_gap_disconnect(unsigned char* bda) {
    (void)bda;
    return ++disconnect_calls;
}

#include "../buf_writer.c"
#include "../pgp_handshake_multi.c"

static void reset(void) {
    memset(client_states, 0, sizeof(client_states));
    active_connections = 0;
    active_connections_mutex = NULL;
    advertise_calls = clear_session_calls = disconnect_calls = 0;
    purged_conn = stats_cleared_conn = -1;
    init_handshake_multi();
}

static void test_slots_fill_and_free(void) {
    reset();
    assert(get_client_state_entry(7) == NULL);
    for (uint16_t id = 10; id < 14; id++) {
        assert(get_or_create_client_state_entry(id) != NULL);
    }
    assert(get_or_create_client_state_entry(99) == NULL);  // table full
    assert(get_or_create_client_state_entry(11) == get_client_state_entry(11));

    connection_stop(11, ESP_GATT_CONN_TERMINATE_LOCAL_HOST);
    assert(!is_connection_active(11));
    assert(get_or_create_client_state_entry(99) != NULL);  // freed slot reused
}

static void test_bda_lookup_follows_conn_id(void) {
    esp_bd_addr_t a = { 1, 2, 3, 4, 5, 6 }, b = { 6, 5, 4, 3, 2, 1 };
    reset();
    set_remote_bda(3, a);
    set_remote_bda(4, b);
    assert(get_client_state_entry_by_bda(a) == get_client_state_entry(3));
    assert(get_client_state_entry_by_bda(b) == get_client_state_entry(4));
    connection_stop(3, ESP_GATT_CONN_TERMINATE_LOCAL_HOST);
    assert(get_client_state_entry_by_bda(a) == NULL);
}

static void test_active_count_and_advertising_stop(void) {
    reset();
    get_or_create_client_state_entry(1);
    get_or_create_client_state_entry(2);
    connection_start(1);
    assert(get_active_connections() == 1 && advertise_calls == 0);
    connection_start(2);
    assert(get_active_connections() == 2 && advertise_calls == 1);  // target 2 reached
    connection_stop(1, ESP_GATT_CONN_TERMINATE_LOCAL_HOST);
    assert(get_active_connections() == 1);
    assert(purged_conn == 1 && stats_cleared_conn == 1);
}

static void test_stop_incomplete_handshake_does_not_undercount(void) {
    reset();
    get_or_create_client_state_entry(1);  // never connection_start()ed
    get_or_create_client_state_entry(2);
    connection_start(2);
    connection_stop(1, ESP_GATT_CONN_TERMINATE_LOCAL_HOST);
    assert(get_active_connections() == 1);
    connection_stop(1, ESP_GATT_CONN_TERMINATE_LOCAL_HOST);  // unknown now: no-op
    assert(get_active_connections() == 1);
}

// Stale cached keys are dropped only when the peer hangs up mid-reconnect on a cached session.
static int stop_clears_cache(pgp_cert_state_t state, bool cached, uint8_t reason) {
    esp_bd_addr_t bda = { 9, 8, 7, 6, 5, 4 };
    reset();
    set_remote_bda(5, bda);
    client_state_t* e = get_client_state_entry(5);
    e->cert_state = state;
    e->used_cached_session = cached;
    connection_stop(5, reason);
    assert(clear_session_calls == 0 || memcmp(cleared_bda, bda, 6) == 0);
    return clear_session_calls;
}

static void test_stale_cache_clearing(void) {
    const uint8_t peer = ESP_GATT_CONN_TERMINATE_PEER_USER;
    assert(stop_clears_cache(PGP_CERT_RECON_CHALLENGE_SENT, true, peer));
    assert(stop_clears_cache(PGP_CERT_RECON_RESPONSE_PENDING, true, peer));
    assert(stop_clears_cache(PGP_CERT_RECON_ESTABLISHED_PENDING, true, peer));
    assert(!stop_clears_cache(PGP_CERT_DONE, true, peer));
    assert(!stop_clears_cache(PGP_CERT_FRESH, true, peer));
    assert(!stop_clears_cache(PGP_CERT_FULL_CHALLENGE_SENT, true, peer));
    assert(!stop_clears_cache(PGP_CERT_RECON_RESPONSE_PENDING, false, peer));
    assert(!stop_clears_cache(PGP_CERT_RECON_RESPONSE_PENDING, true, ESP_GATT_CONN_TERMINATE_LOCAL_HOST));
}

static void test_settings_mutex_survives_slot_reuse(void) {
    reset();
    SemaphoreHandle_t m = client_states[0].settings.mutex;
    assert(m != NULL);
    get_or_create_client_state_entry(1);
    connection_stop(1, ESP_GATT_CONN_TERMINATE_LOCAL_HOST);
    init_handshake_multi();
    assert(client_states[0].settings.mutex == m);
}

static void test_dump_lists_state(void) {
    char buf[4096];
    reset();
    get_or_create_client_state_entry(3)->cert_state = PGP_CERT_DONE;
    size_t n = dump_client_states_format(buf, sizeof(buf));
    assert(n > 0 && strstr(buf, "conn_id=3 cert_state=6") != NULL);
    assert(strstr(buf, "active_connections: 0") != NULL);
}

static void test_reset_disconnects_only_live_slots(void) {
    reset();
    get_or_create_client_state_entry(1);
    get_or_create_client_state_entry(2);
    reset_client_states();
    assert(disconnect_calls == 2);
}

int main(void) {
    test_slots_fill_and_free();
    test_bda_lookup_follows_conn_id();
    test_active_count_and_advertising_stop();
    test_stop_incomplete_handshake_does_not_undercount();
    test_stale_cache_clearing();
    test_settings_mutex_survives_slot_reuse();
    test_dump_lists_state();
    test_reset_disconnects_only_live_slots();
    printf("test_handshake_multi: all passed\n");
    return 0;
}

#endif

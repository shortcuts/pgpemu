// Unit tests for GATT write length validation (PC build)
// Feeds short buffers to the real LED parser and handshake handlers
#ifndef ESP_PLATFORM

#include "../pgp_autobutton.h"
#include "../pgp_bluetooth.h"
#include "../pgp_cert.h"
#include "../pgp_gatts.h"
#include "../pgp_handshake_multi.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---- mocks of everything the units under test call ----
uint16_t certificate_handle_table[CERT_LAST_IDX];
QueueHandle_t button_queue;
uint8_t bt_mac[6];

static client_state_t g_state;
static DeviceSettings g_settings;
static int queue_sends, set_attr_calls, indicate_calls, decrypt_calls, reconnect_calls, persist_calls;

client_state_t* get_client_state_entry(uint16_t conn_id) {
    (void)conn_id;
    return &g_state;
}
client_state_t* get_or_create_client_state_entry(uint16_t conn_id) {
    (void)conn_id;
    return &g_state;
}
int xQueueSend(QueueHandle_t q, const void* item, TickType_t wait) {
    (void)q, (void)item, (void)wait;
    return ++queue_sends;
}
int esp_ble_gatts_set_attr_value(uint16_t handle, uint16_t len, const uint8_t* value) {
    (void)handle, (void)len, (void)value;
    return ++set_attr_calls;
}
int esp_ble_gatts_send_indicate(esp_gatt_if_t gatts_if,
    uint16_t conn_id,
    uint16_t handle,
    uint16_t len,
    uint8_t* value,
    bool need_confirm) {
    (void)gatts_if, (void)conn_id, (void)handle, (void)len, (void)value, (void)need_confirm;
    return ++indicate_calls;
}
int decrypt_next(const uint8_t* data, const uint8_t* key, uint8_t* output) {
    (void)data, (void)key, (void)output;
    return ++decrypt_calls;
}
void generate_reconnect_response(const uint8_t* key, const uint8_t* challenge, uint8_t* output) {
    (void)key, (void)challenge, (void)output;
    reconnect_calls++;
}
void randomize_buffer(uint8_t* buf, size_t len) {
    memset(buf, 0x11, len);
}
void generate_chal_0(const uint8_t* a,
    const uint8_t* b,
    const uint8_t* c,
    const uint8_t* d,
    const uint8_t* e,
    struct challenge_data* f) {
    (void)a, (void)b, (void)c, (void)d, (void)e, (void)f;
}
void generate_next_chal(const uint8_t* a, const uint8_t* b, const uint8_t* c, struct next_challenge* d) {
    (void)a, (void)b, (void)c, (void)d;
}
bool persist_device_session_keys(esp_bd_addr_t bda, const uint8_t* k, const uint8_t* r) {
    (void)bda, (void)k, (void)r;
    persist_calls++;
    return true;
}
bool retrieve_device_session_keys(esp_bd_addr_t bda, uint8_t* k, uint8_t* r) {
    (void)bda, (void)k, (void)r;
    return false;
}
bool has_cached_session(esp_bd_addr_t bda) {
    (void)bda;
    return false;
}
void advertise_if_needed() {
}
void connection_start(uint16_t conn_id) {
    (void)conn_id;
}
void connection_update(uint16_t conn_id) {
    (void)conn_id;
}
void connection_stop(uint16_t conn_id, uint8_t reason) {
    (void)conn_id, (void)reason;
}
int get_cert_state(uint16_t conn_id) {
    (void)conn_id;
    return g_state.cert_state;
}
void increment_caught(uint16_t conn_id) {
    (void)conn_id;
}
void increment_fled(uint16_t conn_id) {
    (void)conn_id;
}
void increment_spin(uint16_t conn_id) {
    (void)conn_id;
}

// ---- units under test, compiled for real ----
#include "../pgp_handshake.c"
#include "../pgp_led_handler.c"

// ---- helpers ----
static void reset(int cert_state) {
    memset(&g_state, 0, sizeof(g_state));
    memset(&g_settings, 0, sizeof(g_settings));
    g_settings.autocatch = true;
    g_state.settings = &g_settings;
    g_state.cert_state = cert_state;
    queue_sends = set_attr_calls = indicate_calls = decrypt_calls = reconnect_calls = persist_calls = 0;
}

// The three call_* wrappers are the only place that knows the handler signatures.
static void call_led(const uint8_t* buf, uint16_t len) {
    handle_led_notify_from_app(0, 0, buf, len);
}
static void call_hs1(const uint8_t* val, uint16_t len) {
    handle_pgp_handshake_first(0, val, len, 0);
}
static void call_hs2(const uint8_t* buf, int len) {
    handle_pgp_handshake_second(0, buf, len, 0);
}

// ---- tests ----
static void test_led_short_writes_ignored(void) {
    // header(3) + count byte(4 patterns' worth: 0x01) + one blinking-green pattern: 4 + 3 = 7 bytes
    uint8_t led[16] = { 0, 0, 0, 0x01, 0xff, 0x80, 0x00 };  // duration 255, green=8, others 0

    reset(0);
    call_led(led, 7);
    assert(queue_sends == 1);  // accepted packet still presses the button

    for (uint16_t len = 0; len < 7; len++) {
        reset(0);
        call_led(led, len);
        assert(queue_sends == 0);
    }

    // count byte claims 31 patterns, i.e. 97 bytes; 96 must be rejected
    uint8_t big[97] = { 0, 0, 0, 0x1f };
    reset(0);
    call_led(big, 96);
    assert(queue_sends == 0);
    printf("PASS: led short writes ignored\n");
}

static void test_cccd_short_write_ignored(void) {
    const uint8_t on[2] = { 0x01, 0x00 };

    reset(0);
    g_state.has_reconnect_key = true;
    call_hs1(on, 2);
    assert(g_state.notify && g_state.cert_state == 3 && indicate_calls == 1);

    for (uint16_t len = 0; len < 2; len++) {
        reset(0);
        g_state.has_reconnect_key = true;
        call_hs1(on, len);
        assert(!g_state.notify && g_state.cert_state == 0 && indicate_calls == 0);
    }
    printf("PASS: cccd short writes ignored\n");
}

static void test_handshake_short_writes_ignored(void) {
    uint8_t buf[64];
    memset(buf, 0x22, sizeof(buf));

    // state 1: needs a whole struct next_challenge (52 bytes)
    reset(1);
    call_hs2(buf, 52);
    assert(decrypt_calls == 1 && g_state.cert_state == 2);
    reset(1);
    call_hs2(buf, 51);
    assert(decrypt_calls == 0 && g_state.cert_state == 1 && indicate_calls == 0);

    // state 2
    reset(2);
    call_hs2(buf, 52);
    assert(decrypt_calls == 1 && g_state.cert_state == 6 && persist_calls == 1);
    reset(2);
    call_hs2(buf, 51);
    assert(decrypt_calls == 0 && g_state.cert_state == 2 && persist_calls == 0);

    // state 4: 4-byte header + 32-byte challenge
    reset(4);
    call_hs2(buf, 36);
    assert(reconnect_calls == 1 && g_state.cert_state == 5);
    reset(4);
    call_hs2(buf, 35);
    assert(reconnect_calls == 0 && g_state.cert_state == 4 && indicate_calls == 0);
    reset(4);
    call_hs2(buf, 0);
    assert(reconnect_calls == 0 && g_state.cert_state == 4);

    // states 0, 3, 5 keep their exact-length checks
    reset(0);
    call_hs2(buf, 20);
    assert(g_state.cert_state == 1);
    reset(3);
    call_hs2(buf, 20);
    assert(g_state.cert_state == 4);
    reset(5);
    call_hs2(buf, 5);
    assert(g_state.cert_state == 6);
    printf("PASS: handshake short writes ignored\n");
}

int main(void) {
    test_led_short_writes_ignored();
    test_cccd_short_write_ignored();
    test_handshake_short_writes_ignored();
    printf("All write-length tests passed\n");
    return 0;
}

#endif  // ESP_PLATFORM

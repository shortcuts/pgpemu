// Unit tests for the real pgp_control.c: dispatch, response framing, indication gating (PC build)
#ifndef ESP_PLATFORM

#define CONFIG_BT_ACL_CONNECTIONS 4
#define CONFIG_BT_SMP_MAX_BONDS 15

#include "../config_secrets.h"
#include "../forget_pairings.h"
#include "../led_output.h"
#include "../pgp_autobutton.h"
#include "../pgp_gap.h"
#include "esp_gap_ble_api.h"
#include "esp_timer.h"
#include "nvs_mem.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// ---- mocks of everything the units under test call outside the real modules ----
char PGP_CLONE_NAME[16] = "clone";
uint8_t PGP_MAC[6] = { 1, 2, 3, 4, 5, 6 };
uint8_t PGP_DEVICE_KEY[16] = { 0xd0 };
uint8_t PGP_BLOB[256] = { 0xb0 };

static uint8_t sent[600];
static uint16_t sent_len, sent_conn;
static bool sent_confirm;
static uint16_t sent_handle;
static int sent_count, advertise_calls, reset_secrets_ok = 1, save_ok = 1;
static int global_writes, device_writes, device_write_slot;

int esp_ble_gatts_create_attr_tab(const esp_gatts_attr_db_t* db, esp_gatt_if_t i, uint8_t n, uint8_t inst) {
    (void)db, (void)i, (void)n, (void)inst;
    return 0;
}
int esp_ble_gatts_start_service(uint16_t h) {
    (void)h;
    return 0;
}
int esp_ble_gatts_send_indicate(esp_gatt_if_t i, uint16_t conn, uint16_t h, uint16_t len, uint8_t* v, bool confirm) {
    (void)i;
    sent_handle = h;
    sent_confirm = confirm;
    memcpy(sent, v, len);
    sent_len = len;
    sent_conn = conn;
    sent_count++;
    return 0;
}
int esp_ble_gap_disconnect(unsigned char* bda) {
    (void)bda;
    return 0;
}
void advertise_if_needed(void) {
    advertise_calls++;
}
adv_state_t pgp_adv_get_state(void) {
    return (adv_state_t)2;
}
void purge_button_queue_for_connection(uint16_t c) {
    (void)c;
}
bool clear_device_session(esp_bd_addr_t bda) {
    (void)bda;
    return true;
}
bool erase_device_data(esp_bd_addr_t bda) {
    (void)bda;
    return true;
}
int esp_ble_remove_bond_device(unsigned char* bda) {
    (void)bda;
    return 0;
}
bool write_global_settings_to_nvs(void) {
    global_writes++;
    return save_ok;
}
bool write_device_settings_to_nvs(uint8_t slot) {
    device_writes++;
    device_write_slot = slot;
    return save_ok;
}
bool write_devices_settings_to_nvs(void) {
    return true;
}
bool reset_secrets(void) {
    return reset_secrets_ok;
}
static int pgp_valid = 1, write_secrets_calls, write_secrets_ok = 1;
static char written_name[16];
static uint8_t written_mac[6], written_key[16], written_blob[256];
bool PGP_VALID(void) {
    return pgp_valid;
}
bool write_secrets(const char* name, const uint8_t* mac, const uint8_t* key, const uint8_t* blob) {
    write_secrets_calls++;
    strcpy(written_name, name);
    memcpy(written_mac, mac, 6);
    memcpy(written_key, key, 16);
    memcpy(written_blob, blob, 256);
    return write_secrets_ok;
}
static int forget_calls;
void forget_pairings_and_restart(void) {
    forget_calls++;
}
bool get_led_advertising(void) {
    return true;
}
void log_levels_debug(void) {
}
void log_levels_info(void) {
}
void log_levels_verbose(void) {
}

#include "../buf_writer.c"
#include "../known_devices.c"
#include "../nvs_helper.c"
#include "../pgp_control.c"
#include "../pgp_handshake_multi.c"
#include "../pgp_ota.c"
#include "../settings.c"
#include "../stats.c"

// ---- helpers ----
static void reset(void) {
    memset(client_states, 0, sizeof(client_states));
    memset(indicate_in_flight, 0, sizeof(indicate_in_flight));
    memset(event_notify, 0, sizeof(event_notify));
    memset(event_seq, 0, sizeof(event_seq));
    active_connections_mutex = NULL;
    active_connections = 0;
    init_handshake_multi();
    init_global_settings();
    global_settings_ready();
    global_settings.target_active_connections = 1;
    global_settings.advertising_enabled = true;
    global_settings.log_level = 1;
    sent_count = advertise_calls = global_writes = device_writes = 0;
    sent_len = 0;
    save_ok = reset_secrets_ok = write_secrets_ok = pgp_valid = 1;
    write_secrets_calls = 0;
    nvs_mem_reset();
    known_devices_init();
    control_handle_table[IDX_CHAR_CONTROL_COMMAND_VAL] = 40;
    control_handle_table[IDX_CHAR_CONTROL_RESPONSE_CFG] = 43;
    control_handle_table[IDX_CHAR_CONTROL_EVENT_VAL] = 45;
    control_handle_table[IDX_CHAR_CONTROL_EVENT_CFG] = 46;
}

static bool write_event_cccd(uint16_t conn_id, uint8_t on) {
    uint8_t value[2] = { on, 0 };
    esp_ble_gatts_cb_param_t p = { .write = { .conn_id = conn_id, .handle = 46, .len = 2, .value = value } };
    return pgp_control_try_handle_write(3, &p);
}

// Writes `len` bytes to the Command characteristic of conn_id and reports whether the stack handled it.
static bool write_cmd(uint16_t conn_id, const uint8_t* bytes, uint16_t len) {
    esp_ble_gatts_cb_param_t p = { .write = {
                                       .conn_id = conn_id, .handle = 40, .len = len, .value = (uint8_t*)bytes } };
    return pgp_control_try_handle_write(3, &p);
}
static void cmd1(uint8_t op, uint8_t arg, int with_arg) {
    uint8_t b[2] = { op, arg };
    assert(write_cmd(0, b, with_arg ? 2 : 1));
}

static void setup_default(void) {
}
static void setup_log2(void) {
    global_settings.log_level = 2;
}
static void setup_summary(void) {
    stats_clear_for_conn(7);
    client_state_t* e = get_or_create_client_state_entry(7);
    e->has_settings = true;
    e->settings.autospin = true;
    e->settings.autocatch = false;
    e->cert_state = PGP_CERT_DONE;
    increment_caught(7);
    increment_caught(7);
    increment_fled(7);
    for (int i = 0; i < 3; i++) {
        increment_spin(7);
    }
}
static void setup_provisioning(void) {
    pgp_valid = 0;
}
static void setup_known(void) {
    const uint8_t first[6] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01 };
    const uint8_t second[6] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x02 };
    known_devices_touch(first);
    known_devices_touch(second);
    assert(known_devices_rename(first, "Pixel", 5) == KNOWN_DEVICES_OK);
    memcpy(get_or_create_client_state_entry(7)->remote_bda, second, 6);
}
static const struct {
    const char* name;
    void (*fn)(void);
} setups[] = {
    { "default", setup_default },
    { "log2", setup_log2 },
    { "summary", setup_summary },
    { "provisioning", setup_provisioning },
    { "known", setup_known },
};

int64_t esp_timer_get_time(void) {
    return 5000000;
}
esp_err_t esp_timer_create(const esp_timer_create_args_t* args, esp_timer_handle_t* out) {
    (void)args, (void)out;
    return ESP_OK;
}
esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t us) {
    (void)t, (void)us;
    return ESP_OK;
}

// ---- tests ----
static void test_unknown_opcode_and_empty_write(void) {
    reset();
    cmd1(0x7f, 0, 0);
    assert(sent_len == 2 && sent[0] == CONTROL_STATUS_ERR_UNKNOWN_OPCODE && sent[1] == 0x7f);

    pgp_control_on_conf(0);
    assert(write_cmd(0, (uint8_t*)"", 0));
    assert(sent_len == 2 && sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD && sent[1] == 0);
}

static void test_foreign_handles(void) {
    reset();
    esp_ble_gatts_cb_param_t p = { .write = { .conn_id = 0, .handle = 43, .len = 2 } };
    assert(pgp_control_try_handle_write(3, &p) && sent_count == 0);  // CCCD write: handled, no reply
    p.write.handle = 99;
    assert(!pgp_control_try_handle_write(3, &p));
}

static void test_global_settings_frame(void) {
    reset();
    global_settings.log_level = 2;
    cmd1(CONTROL_OP_GET_GLOBAL_SETTINGS, 0, 0);
    const uint8_t want[] = { CONTROL_STATUS_OK, CONTROL_OP_GET_GLOBAL_SETTINGS, 2, 1, 0, 1, 2 };
    assert(sent_len == sizeof(want) && memcmp(sent, want, sizeof(want)) == 0);
}

static void test_set_max_connections_validates_payload(void) {
    reset();
    cmd1(CONTROL_OP_SET_MAX_CONNECTIONS, 0, 0);
    assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);
    pgp_control_on_conf(0);
    cmd1(CONTROL_OP_SET_MAX_CONNECTIONS, 5, 1);
    assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD && global_settings.target_active_connections == 1);
    pgp_control_on_conf(0);
    cmd1(CONTROL_OP_SET_MAX_CONNECTIONS, 3, 1);
    assert(sent[0] == CONTROL_STATUS_OK && global_settings.target_active_connections == 3);
}

static void test_advertise_and_save_and_restart(void) {
    reset();
    cmd1(CONTROL_OP_ADVERTISE_STOP, 0, 0);
    assert(sent[0] == CONTROL_STATUS_OK && !global_settings.advertising_enabled && advertise_calls == 1);

    pgp_control_on_conf(0);
    save_ok = 0;
    cmd1(CONTROL_OP_SAVE_SETTINGS, 0, 0);
    assert(sent[0] == CONTROL_STATUS_ERR_INTERNAL && sent[1] == CONTROL_OP_SAVE_SETTINGS);

    pgp_control_on_conf(0);
    esp_restart_calls = 0;
    cmd1(CONTROL_OP_RESTART, 0, 0);
    assert(sent[0] == CONTROL_STATUS_OK && esp_restart_calls == 1);  // ack goes out before the reboot
}

static void test_forget_pairings(void) {
    reset();
    forget_calls = 0;
    cmd1(CONTROL_OP_FORGET_PAIRINGS, 0, 0);
    assert(sent_len == 2 && sent[0] == CONTROL_STATUS_OK && sent[1] == CONTROL_OP_FORGET_PAIRINGS);
    assert(sent_count == 1 && forget_calls == 1);  // ack goes out before the reboot
}

// ATT allows one unconfirmed indication per link: later responses are dropped until the CONF event.
static void test_indication_gate(void) {
    reset();
    cmd1(CONTROL_OP_GET_LED_STATE, 0, 0);
    assert(sent_count == 1 && sent_len == 3 && sent[2] == 1);
    cmd1(CONTROL_OP_GET_LED_STATE, 0, 0);
    assert(sent_count == 1);
    pgp_control_on_conf(0);
    cmd1(CONTROL_OP_GET_LED_STATE, 0, 0);
    assert(sent_count == 2);

    cmd1(CONTROL_OP_GET_LED_STATE, 0, 0);
    assert(sent_count == 2);
    pgp_control_on_disconnect(0);
    cmd1(CONTROL_OP_GET_LED_STATE, 0, 0);
    assert(sent_count == 3);

    uint8_t b[1] = { CONTROL_OP_GET_LED_STATE };
    assert(write_cmd(CONFIG_BT_ACL_CONNECTIONS, b, 1) && sent_count == 3);  // out-of-range conn_id: dropped
}

// HELP is longer than one response: the frame is cut to MAX_VALUE_LENGTH.
static void test_oversized_payload_is_truncated(void) {
    reset();
    cmd1(CONTROL_OP_HELP, 0, 0);
    assert(sent_len == 2 + CONTROL_MAX_RESPONSE_PAYLOAD);
    assert(sent[0] == CONTROL_STATUS_OK && sent[1] == CONTROL_OP_HELP && memcmp(sent + 2, "---HELP---", 10) == 0);
}

static void test_client_summary_layout(void) {
    reset();
    setup_summary();

    cmd1(CONTROL_OP_GET_CLIENT_SUMMARY, 0, 0);
    assert(sent_len == 2 + 4 * 11);
    const uint8_t slot0[11] = { 0x07, 0x00, 0x07, 1, 0, 2, 0, 1, 0, 3, 0 };  // flags: settings|stats|handshake done
    assert(memcmp(sent + 2, slot0, 11) == 0);
    const uint8_t empty[11] = { 0xff, 0xff };
    for (int i = 1; i < 4; i++) {
        assert(memcmp(sent + 2 + 11 * i, empty, 11) == 0);
    }
}

static void test_device_info_frame(void) {
    reset();
    cmd1(CONTROL_OP_GET_DEVICE_INFO, 0, 0);
    const uint8_t want[] = {
        CONTROL_STATUS_OK, CONTROL_OP_GET_DEVICE_INFO, CONTROL_PROTOCOL_VERSION, 4, 3, 5, 0, 0, 0
    };
    assert(memcmp(sent, want, sizeof(want)) == 0);
    assert(sent[2 + 15] == 0);  // flags: not provisioning
    assert(sent_len == 2 + 16 + strlen(PGP_FW_VERSION));
    assert(memcmp(sent + 2 + 16, PGP_FW_VERSION, strlen(PGP_FW_VERSION)) == 0);

    pgp_control_on_conf(0);
    pgp_valid = 0;
    cmd1(CONTROL_OP_GET_DEVICE_INFO, 0, 0);
    assert(sent[2 + 15] == 1);
}

// OTA frames: [op][payload]; every reply must be confirmed before the next write.
static void ota_frame(uint8_t op, const uint8_t* payload, uint16_t len) {
    uint8_t frame[1 + 8];
    frame[0] = op;
    memcpy(frame + 1, payload, len);
    pgp_control_on_conf(0);
    assert(write_cmd(0, frame, 1 + len));
}

static void test_ota_frames(void) {
    reset();
    pgp_ota_abort(0);
    armed_until_ms = 0;
    client_state_t* e = get_or_create_client_state_entry(0);
    e->mtu = 517;
    const uint8_t size[4] = { 0xe8, 0x03, 0, 0 };  // 1000
    ota_frame(CONTROL_OP_OTA_BEGIN, size, 3);
    assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);
    ota_frame(CONTROL_OP_OTA_BEGIN, size, 4);
    assert(sent[0] == CONTROL_STATUS_ERR_OTA_NOT_ARMED && sent_len == 2);

    pgp_ota_arm();
    e->cert_state = PGP_CERT_DONE;
    ota_frame(CONTROL_OP_OTA_BEGIN, size, 4);
    assert(sent[0] == CONTROL_STATUS_ERR_BUSY);
    e->cert_state = PGP_CERT_FRESH;
    ota_frame(CONTROL_OP_OTA_BEGIN, size, 4);
    const uint8_t begin_ok[] = { CONTROL_STATUS_OK, CONTROL_OP_OTA_BEGIN, 0xe0, 0x01 };  // chunk 480
    assert(sent_len == sizeof(begin_ok) && memcmp(sent, begin_ok, sizeof(begin_ok)) == 0);

    const uint8_t d0[] = { 0, 0, 0, 0, 0xaa, 0xbb };  // offset 0, 2 bytes
    ota_frame(CONTROL_OP_OTA_DATA, d0, sizeof(d0));
    const uint8_t data_ok[] = { CONTROL_STATUS_OK, CONTROL_OP_OTA_DATA, 2, 0, 0, 0 };
    assert(sent_len == sizeof(data_ok) && memcmp(sent, data_ok, sizeof(data_ok)) == 0);
    ota_frame(CONTROL_OP_OTA_DATA, d0, sizeof(d0));  // repeated chunk
    assert(sent[0] == CONTROL_STATUS_ERR_OTA_STATE && sent_len == 2);
    ota_frame(CONTROL_OP_OTA_DATA, d0, 4);  // no data bytes
    assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);
    ota_frame(CONTROL_OP_OTA_END, d0, 0);  // 2 of 1000 bytes
    assert(sent[0] == CONTROL_STATUS_ERR_OTA_STATE && sent[1] == CONTROL_OP_OTA_END);
    ota_frame(CONTROL_OP_OTA_ABORT, d0, 0);
    assert(sent[0] == CONTROL_STATUS_OK && sent_len == 2);
    ota_frame(CONTROL_OP_OTA_DATA, d0, sizeof(d0));
    assert(sent[0] == CONTROL_STATUS_ERR_OTA_STATE);

    pgp_ota_arm();
    ota_frame(CONTROL_OP_OTA_BEGIN, size, 4);
    assert(sent[0] == CONTROL_STATUS_OK);
    pgp_control_on_disconnect(0);
    ota_frame(CONTROL_OP_OTA_DATA, d0, sizeof(d0));
    assert(sent[0] == CONTROL_STATUS_ERR_OTA_STATE);
}

// 294-byte secrets payload behind the opcode byte: name[16] mac[6] dkey[16] blob[256]
static void provision(const uint8_t* payload, uint16_t payload_len) {
    uint8_t frame[1 + 300];
    frame[0] = CONTROL_OP_PROVISION_SECRETS;
    memcpy(frame + 1, payload, payload_len);
    assert(write_cmd(0, frame, 1 + payload_len));
}

static void test_provision_secrets(void) {
    reset();
    uint8_t p[294] = { 0 };
    memcpy(p, "MYCLONE", 8);
    const uint8_t mac[6] = { 0xe4, 0xcb, 0x0b, 0xc3, 0x59, 0x63 };
    memcpy(p + 16, mac, 6);
    p[22] = 0xaa;
    p[38] = 0xbb;
    p[293] = 0xcc;

    provision(p, 294);  // provisioned device: refused
    assert(sent_len == 2 && sent[0] == CONTROL_STATUS_ERR_NOT_PROVISIONING && sent[1] == CONTROL_OP_PROVISION_SECRETS);
    assert(write_secrets_calls == 0);

    pgp_valid = 0;
    uint8_t bad[294];
    const struct {
        uint16_t len;
        int off, val;
    } cases[] = {
        { 293, -1, 0 },    // short
        { 294, 6, 0 },     // no NUL in name: bytes 0..15 non-zero
        { 294, 0, 0 },     // empty name
        { 294, 1, 0x07 },  // control char in name
        { 294, 16, 0 },    // mac byte 0 cleared below
    };
    for (size_t k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
        memcpy(bad, p, 294);
        if (k == 1) {
            memset(bad, 'A', 16);
        } else if (k == 4) {
            memset(bad + 16, 0, 6);
        } else if (cases[k].off >= 0) {
            bad[cases[k].off] = (uint8_t)cases[k].val;
        }
        pgp_control_on_conf(0);
        provision(bad, cases[k].len);
        assert(sent_len == 2 && sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);
        assert(write_secrets_calls == 0);
    }

    pgp_control_on_conf(0);
    provision(p, 294);
    assert(sent_len == 2 && sent[0] == CONTROL_STATUS_OK && sent[1] == CONTROL_OP_PROVISION_SECRETS);
    assert(write_secrets_calls == 1 && strcmp(written_name, "MYCLONE") == 0);
    assert(memcmp(written_mac, mac, 6) == 0 && written_key[0] == 0xaa && written_blob[0] == 0xbb);
    assert(written_blob[255] == 0xcc);

    pgp_control_on_conf(0);
    write_secrets_ok = 0;
    provision(p, 294);
    assert(sent[0] == CONTROL_STATUS_ERR_INTERNAL);
}

static void test_toggle_device_payload(void) {
    reset();
    cmd1(CONTROL_OP_TOGGLE_AUTOSPIN, 0, 0);
    assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);
    pgp_control_on_conf(0);
    cmd1(CONTROL_OP_TOGGLE_AUTOCATCH, 4, 1);
    assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);

    pgp_control_on_conf(0);
    client_state_t* e = get_or_create_client_state_entry(1);
    e->has_settings = true;
    cmd1(CONTROL_OP_TOGGLE_AUTOSPIN, 0, 1);
    assert(sent[0] == CONTROL_STATUS_OK && sent_len == 3 && sent[2] == 1 && e->settings.autospin);
}

static void cmd2(uint8_t op, uint8_t a, uint8_t b) {
    uint8_t buf[3] = { op, a, b };
    assert(write_cmd(0, buf, 3));
}

static void test_set_device_payload(void) {
    reset();
    const uint8_t ops[] = { CONTROL_OP_SET_AUTOSPIN, CONTROL_OP_SET_AUTOCATCH };
    for (int k = 0; k < 2; k++) {
        bool* flag = NULL;
        client_state_t* e = get_or_create_client_state_entry(1 + k);  // conn 1, 2 -> slot 0, 1
        flag = k ? &e->settings.autocatch : &e->settings.autospin;

        cmd1(ops[k], 0, 0);  // payload too short
        assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);
        pgp_control_on_conf(0);
        cmd2(ops[k], 4, 1);  // slot out of range
        assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);
        pgp_control_on_conf(0);
        cmd2(ops[k], 3, 2);  // value out of range
        assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);
        pgp_control_on_conf(0);
        e->has_settings = false;
        cmd2(ops[k], (uint8_t)k, 1);  // no profile in slot
        assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD && !*flag);
        pgp_control_on_conf(0);

        e->has_settings = true;
        for (int i = 0; i < 2; i++) {  // a repeated frame must not flip the value back
            cmd2(ops[k], (uint8_t)k, 1);
            assert(sent_len == 3 && sent[0] == CONTROL_STATUS_OK && sent[1] == ops[k] && sent[2] == 1 && *flag);
            pgp_control_on_conf(0);
        }
        cmd2(ops[k], (uint8_t)k, 0);
        assert(sent[0] == CONTROL_STATUS_OK && sent[2] == 0 && !*flag);
        pgp_control_on_conf(0);
    }
}

static void test_set_log_level(void) {
    reset();
    cmd1(CONTROL_OP_SET_LOG_LEVEL, 0, 0);
    assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);
    pgp_control_on_conf(0);
    cmd1(CONTROL_OP_SET_LOG_LEVEL, 0, 1);
    assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);
    pgp_control_on_conf(0);
    cmd1(CONTROL_OP_SET_LOG_LEVEL, 4, 1);
    assert(sent[0] == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD && global_settings.log_level == 1);
    pgp_control_on_conf(0);
    for (uint8_t lvl = 3; lvl >= 1; lvl--) {
        for (int i = 0; i < 2; i++) {
            cmd1(CONTROL_OP_SET_LOG_LEVEL, lvl, 1);
            assert(sent_len == 3 && sent[0] == CONTROL_STATUS_OK && sent[2] == lvl && global_settings.log_level == lvl);
            pgp_control_on_conf(0);
        }
    }
}

static void test_set_persists_only_on_change(void) {
    reset();
    client_state_t* e = get_or_create_client_state_entry(1);
    e->has_settings = e->settings.autospin = e->settings.autocatch = true;
    const struct {
        uint8_t op, a, b;
        int len;
    } cmds[] = {
        { CONTROL_OP_SET_LOG_LEVEL, 2, 0, 1 },
        { CONTROL_OP_SET_MAX_CONNECTIONS, 3, 0, 1 },
        { CONTROL_OP_ADVERTISE_STOP, 0, 0, 0 },
    };
    for (size_t k = 0; k < sizeof(cmds) / sizeof(cmds[0]); k++) {
        for (int i = 0; i < 2; i++) {
            global_writes = 0;
            cmd1(cmds[k].op, cmds[k].a, cmds[k].len);
            assert(sent[0] == CONTROL_STATUS_OK);
            assert(global_writes == (i == 0 ? 1 : 0));
            pgp_control_on_conf(0);
        }
    }
    cmd1(CONTROL_OP_ADVERTISE_START, 0, 0);
    assert(global_writes == 1);
    pgp_control_on_conf(0);

    for (uint8_t op = CONTROL_OP_SET_AUTOSPIN; op <= CONTROL_OP_SET_AUTOCATCH; op++) {
        for (int i = 0; i < 2; i++) {
            device_writes = 0;
            uint8_t b[3] = { op, 0, 0 };  // both start on: 0 is a change
            assert(write_cmd(0, b, 3));
            assert(sent[0] == CONTROL_STATUS_OK && device_writes == (i == 0 ? 1 : 0) && device_write_slot == 0);
            pgp_control_on_conf(0);
        }
    }
}

static void test_set_reports_persist_failure(void) {
    reset();
    client_state_t* e = get_or_create_client_state_entry(1);
    e->has_settings = true;
    e->settings.autospin = false;
    save_ok = 0;
    cmd1(CONTROL_OP_SET_LOG_LEVEL, 3, 1);
    assert(sent_len == 2 && sent[0] == CONTROL_STATUS_ERR_INTERNAL && global_settings.log_level == 3);
    pgp_control_on_conf(0);
    cmd1(CONTROL_OP_SET_MAX_CONNECTIONS, 3, 1);
    assert(sent[0] == CONTROL_STATUS_ERR_INTERNAL && global_settings.target_active_connections == 3);
    pgp_control_on_conf(0);
    cmd1(CONTROL_OP_ADVERTISE_STOP, 0, 0);
    assert(sent[0] == CONTROL_STATUS_ERR_INTERNAL && !global_settings.advertising_enabled);
    pgp_control_on_conf(0);
    cmd2(CONTROL_OP_SET_AUTOSPIN, 0, 1);
    assert(sent_len == 2 && sent[0] == CONTROL_STATUS_ERR_INTERNAL && e->settings.autospin);
}

typedef struct {
    const char* name;
    int value;
} named_value_t;

static const named_value_t opcode_table[] = {
    { "HELP", CONTROL_OP_HELP },
    { "GET_GLOBAL_SETTINGS", CONTROL_OP_GET_GLOBAL_SETTINGS },
    { "SAVE_SETTINGS", CONTROL_OP_SAVE_SETTINGS },
    { "GET_SECRETS", CONTROL_OP_GET_SECRETS },
    { "RESET_SECRETS", CONTROL_OP_RESET_SECRETS },
    { "RESTART", CONTROL_OP_RESTART },
    { "GET_LED_STATE", CONTROL_OP_GET_LED_STATE },
    { "CYCLE_LOG_LEVEL", CONTROL_OP_CYCLE_LOG_LEVEL },
    { "GET_RUNTIME_STATS", CONTROL_OP_GET_RUNTIME_STATS },
    { "GET_TASK_LIST", CONTROL_OP_GET_TASK_LIST },
    { "ADVERTISE_START", CONTROL_OP_ADVERTISE_START },
    { "ADVERTISE_STOP", CONTROL_OP_ADVERTISE_STOP },
    { "GET_CLIENT_STATES", CONTROL_OP_GET_CLIENT_STATES },
    { "RESET_CLIENT_STATES", CONTROL_OP_RESET_CLIENT_STATES },
    { "SET_MAX_CONNECTIONS", CONTROL_OP_SET_MAX_CONNECTIONS },
    { "TOGGLE_AUTOSPIN", CONTROL_OP_TOGGLE_AUTOSPIN },
    { "TOGGLE_AUTOCATCH", CONTROL_OP_TOGGLE_AUTOCATCH },
    { "GET_CLIENT_SUMMARY", CONTROL_OP_GET_CLIENT_SUMMARY },
    { "GET_DEVICE_INFO", CONTROL_OP_GET_DEVICE_INFO },
    { "SET_AUTOSPIN", CONTROL_OP_SET_AUTOSPIN },
    { "SET_AUTOCATCH", CONTROL_OP_SET_AUTOCATCH },
    { "SET_LOG_LEVEL", CONTROL_OP_SET_LOG_LEVEL },
    { "PROVISION_SECRETS", CONTROL_OP_PROVISION_SECRETS },
    { "FORGET_PAIRINGS", CONTROL_OP_FORGET_PAIRINGS },
    { "OTA_BEGIN", CONTROL_OP_OTA_BEGIN },
    { "OTA_DATA", CONTROL_OP_OTA_DATA },
    { "OTA_END", CONTROL_OP_OTA_END },
    { "OTA_ABORT", CONTROL_OP_OTA_ABORT },
    { "LIST_KNOWN_DEVICES", CONTROL_OP_LIST_KNOWN_DEVICES },
    { "RENAME_KNOWN_DEVICE", CONTROL_OP_RENAME_KNOWN_DEVICE },
    { "FORGET_KNOWN_DEVICE", CONTROL_OP_FORGET_KNOWN_DEVICE },
};

static const named_value_t event_table[] = {
    { "GAME_CONNECTED", PGP_EVENT_GAME_CONNECTED },
    { "GAME_DISCONNECTED", PGP_EVENT_GAME_DISCONNECTED },
    { "POKEMON_IN_RANGE", PGP_EVENT_POKEMON_IN_RANGE },
    { "NEW_POKEMON_IN_RANGE", PGP_EVENT_NEW_POKEMON_IN_RANGE },
    { "POKESTOP_IN_RANGE", PGP_EVENT_POKESTOP_IN_RANGE },
    { "CAUGHT", PGP_EVENT_CAUGHT },
    { "FLED", PGP_EVENT_FLED },
    { "ITEMS_RECEIVED", PGP_EVENT_ITEMS_RECEIVED },
    { "BAG_FULL", PGP_EVENT_BAG_FULL },
    { "BOX_FULL", PGP_EVENT_BOX_FULL },
    { "BALLS_EMPTY", PGP_EVENT_BALLS_EMPTY },
};

static const named_value_t status_table[] = {
    { "OK", CONTROL_STATUS_OK },
    { "ERR_UNKNOWN_OPCODE", CONTROL_STATUS_ERR_UNKNOWN_OPCODE },
    { "ERR_MALFORMED_PAYLOAD", CONTROL_STATUS_ERR_MALFORMED_PAYLOAD },
    { "ERR_NOT_BONDED", CONTROL_STATUS_ERR_NOT_BONDED },
    { "ERR_BUSY", CONTROL_STATUS_ERR_BUSY },
    { "ERR_INTERNAL", CONTROL_STATUS_ERR_INTERNAL },
    { "ERR_NOT_PROVISIONING", CONTROL_STATUS_ERR_NOT_PROVISIONING },
    { "ERR_OTA_NOT_ARMED", CONTROL_STATUS_ERR_OTA_NOT_ARMED },
    { "ERR_OTA_STATE", CONTROL_STATUS_ERR_OTA_STATE },
    { "ERR_NOT_FOUND", CONTROL_STATUS_ERR_NOT_FOUND },
};

static const struct {
    const char* key;
    const uint8_t* le;
} uuid_table[] = {
    { "control_service", GATTS_SERVICE_UUID_CONTROL },
    { "command_char", GATTS_CHAR_UUID_CONTROL_COMMAND },
    { "response_char", GATTS_CHAR_UUID_CONTROL_RESPONSE },
    { "event_char", GATTS_CHAR_UUID_CONTROL_EVENT },
};

// run_tests.sh runs the binary from the repo root.
static FILE* open_fixture(void) {
    FILE* f = fopen("protocol/control_vectors.txt", "r");
    assert(f != NULL);
    return f;
}

static size_t hex_decode(const char* hex, uint8_t* out, size_t cap) {
    size_t n = strlen(hex) / 2;
    assert(n <= cap);
    for (size_t i = 0; i < n; i++) {
        unsigned v;
        assert(sscanf(hex + 2 * i, "%2x", &v) == 1);
        out[i] = (uint8_t)v;
    }
    return n;
}

static void check_named_values(FILE* f, const char* kind, const named_value_t* table, size_t count) {
    size_t kind_len = strlen(kind), seen = 0;
    char line[1024];
    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, kind, kind_len) != 0 || line[kind_len] != ' ') {
            continue;
        }
        char name[32], hex[8];
        assert(sscanf(line + kind_len, " %31s %7s", name, hex) == 2);
        uint8_t value;
        hex_decode(hex, &value, 1);
        bool found = false;
        for (size_t i = 0; i < count; i++) {
            if (strcmp(table[i].name, name) == 0) {
                assert(table[i].value == value);
                found = true;
            }
        }
        assert(found);
        seen++;
    }
    assert(seen == count);
}

static void uuid_string(const uint8_t* le, char* out) {
    sprintf(out,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        le[15],
        le[14],
        le[13],
        le[12],
        le[11],
        le[10],
        le[9],
        le[8],
        le[7],
        le[6],
        le[5],
        le[4],
        le[3],
        le[2],
        le[1],
        le[0]);
}

static void test_wire_constants(void) {
    FILE* f = open_fixture();
    check_named_values(f, "opcode", opcode_table, sizeof(opcode_table) / sizeof(opcode_table[0]));
    check_named_values(f, "event", event_table, sizeof(event_table) / sizeof(event_table[0]));
    check_named_values(f, "status", status_table, sizeof(status_table) / sizeof(status_table[0]));

    size_t seen = 0;
    char line[1024];
    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "uuid ", 5) != 0) {
            continue;
        }
        char key[32], text[64];
        assert(sscanf(line, "uuid %31s %63s", key, text) == 2);
        bool found = false;
        for (size_t i = 0; i < sizeof(uuid_table) / sizeof(uuid_table[0]); i++) {
            if (strcmp(uuid_table[i].key, key) == 0) {
                char actual[37];
                uuid_string(uuid_table[i].le, actual);
                assert(strcmp(actual, text) == 0);
                found = true;
            }
        }
        assert(found);
        seen++;
    }
    assert(seen == sizeof(uuid_table) / sizeof(uuid_table[0]));
    fclose(f);
}

static void run_setup(const char* name) {
    for (size_t i = 0; i < sizeof(setups) / sizeof(setups[0]); i++) {
        if (strcmp(setups[i].name, name) == 0) {
            setups[i].fn();
            return;
        }
    }
    assert(0 && "unknown setup name in control_vectors.txt");
}

static void test_wire_vectors(void) {
    FILE* f = open_fixture();
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "vector ", 7) != 0) {
            continue;
        }
        char name[32], setup[32], request_hex[64], response_hex[256], prefix[16] = "";
        assert(sscanf(line, "vector %31s %31s %63s %255s %15s", name, setup, request_hex, response_hex, prefix) >= 4);
        uint8_t request[64], want[256];
        size_t request_len = hex_decode(request_hex, request, sizeof(request));
        size_t want_len = hex_decode(response_hex, want, sizeof(want));

        reset();
        run_setup(setup);
        assert(write_cmd(0, request, (uint16_t)request_len));
        if (strcmp(prefix, "prefix") == 0) {
            assert(sent_len >= want_len);
        } else {
            assert(sent_len == want_len);
        }
        assert(memcmp(sent, want, want_len) == 0);
    }
    fclose(f);
}

// Game link conn 3 sits on slot 0 and conn 7 on slot 1; conn 0 is the subscribed companion link.
static void setup_events(void) {
    get_or_create_client_state_entry(3);
    get_or_create_client_state_entry(7);
    assert(write_event_cccd(0, 1) && sent_count == 0);
}

static void test_event_vectors(void) {
    FILE* f = open_fixture();
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "vector_event ", 13) != 0) {
            continue;
        }
        char name[32], type_name[32], frame_hex[16];
        assert(sscanf(line, "vector_event %31s %31s %15s", name, type_name, frame_hex) == 3);
        uint8_t want[3];
        assert(hex_decode(frame_hex, want, sizeof(want)) == sizeof(want));
        pgp_event_t type = 0;
        for (size_t i = 0; i < sizeof(event_table) / sizeof(event_table[0]); i++) {
            if (strcmp(event_table[i].name, type_name) == 0) {
                type = (pgp_event_t)event_table[i].value;
            }
        }
        assert(type != 0);

        reset();
        setup_events();
        pgp_control_emit_event(type, 7);
        assert(sent_count == 1 && sent_conn == 0 && sent_len == sizeof(want) && memcmp(sent, want, sizeof(want)) == 0);
        assert(!sent_confirm && sent_handle == 45);
    }
    fclose(f);
}

static void test_event_subscription(void) {
    reset();
    get_or_create_client_state_entry(7);
    pgp_control_emit_event(PGP_EVENT_CAUGHT, 7);
    assert(sent_count == 0);  // nobody subscribed

    assert(write_event_cccd(0, 1) && write_event_cccd(2, 1));
    pgp_control_emit_event(PGP_EVENT_CAUGHT, 7);
    assert(sent_count == 2);  // one per subscribed link
    pgp_control_emit_event(PGP_EVENT_FLED, 7);
    assert(sent_count == 4 && sent[0] == 1);  // seq counts per link

    write_event_cccd(0, 0);
    pgp_control_emit_event(PGP_EVENT_FLED, 7);
    assert(sent_count == 5 && sent_conn == 2);

    pgp_control_on_disconnect(2);
    pgp_control_emit_event(PGP_EVENT_FLED, 7);
    assert(sent_count == 5);

    assert(write_event_cccd(0, 1));  // resubscribing restarts seq
    pgp_control_emit_event(PGP_EVENT_FLED, 7);
    assert(sent_count == 6 && sent[0] == 0);

    pgp_control_emit_event(PGP_EVENT_FLED, 9);  // unknown game link: no frame
    assert(sent_count == 6);
}

static void test_event_does_not_touch_response_slot(void) {
    reset();
    setup_events();
    cmd1(CONTROL_OP_GET_LED_STATE, 0, 0);
    assert(sent_count == 1 && indicate_in_flight[0]);
    pgp_control_emit_event(PGP_EVENT_CAUGHT, 7);
    assert(sent_count == 2 && indicate_in_flight[0]);
    pgp_control_on_conf(0);
    cmd1(CONTROL_OP_GET_LED_STATE, 0, 0);
    assert(sent_count == 3 && sent_confirm);
}

int main(void) {
    test_wire_constants();
    test_wire_vectors();
    test_event_vectors();
    test_event_subscription();
    test_event_does_not_touch_response_slot();
    test_unknown_opcode_and_empty_write();
    test_foreign_handles();
    test_global_settings_frame();
    test_set_max_connections_validates_payload();
    test_advertise_and_save_and_restart();
    test_forget_pairings();
    test_indication_gate();
    test_oversized_payload_is_truncated();
    test_client_summary_layout();
    test_device_info_frame();
    test_provision_secrets();
    test_ota_frames();
    test_toggle_device_payload();
    test_set_device_payload();
    test_set_log_level();
    test_set_persists_only_on_change();
    test_set_reports_persist_failure();
    printf("test_pgp_control_protocol: all passed\n");
    return 0;
}

#endif

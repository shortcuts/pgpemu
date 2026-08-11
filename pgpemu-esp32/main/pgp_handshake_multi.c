#include "pgp_handshake_multi.h"

#include "buf_writer.h"
#include "config_storage.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "log_tags.h"
#include "mutex_helpers.h"
#include "pgp_autobutton.h"
#include "pgp_gap.h"

#include <stdlib.h>
#include <string.h>

static int active_connections = 0;
static SemaphoreHandle_t active_connections_mutex = NULL;

#define MAX_CONNECTIONS CONFIG_BT_ACL_CONNECTIONS

// map which cert_states index corresponds to which conn_id
static uint16_t conn_id_map[MAX_CONNECTIONS] = { 0 };

// keep track of handshake state per connection
static client_state_t client_states[MAX_CONNECTIONS] = { 0 };

void init_handshake_multi() {
    memset(conn_id_map, 0xff, sizeof(conn_id_map));
    if (active_connections_mutex == NULL) {
        active_connections_mutex = xSemaphoreCreateMutex();
    }
}

int get_active_connections() {
    int count = 0;
    if (mutex_acquire_timeout(active_connections_mutex, 100)) {
        count = active_connections;
        mutex_release(active_connections_mutex);
    }
    return count;
}

int get_max_connections() {
    return MAX_CONNECTIONS;
}

client_state_t* get_client_state_entry(uint16_t conn_id) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] == conn_id) {
            return &client_states[i];
        }
    }

    return NULL;
}

client_state_t* get_client_state_entry_by_idx(int i) {
    if (conn_id_map[i] != 0xffff) {
        return &client_states[i];
    }

    return NULL;
}

bool is_connection_active(uint16_t conn_id) {
    return get_client_state_entry(conn_id) != NULL;
}

client_state_t* get_client_state_entry_by_bda(esp_bd_addr_t bda) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] != 0xffff && memcmp(client_states[i].remote_bda, bda, sizeof(esp_bd_addr_t)) == 0) {
            return &client_states[i];
        }
    }

    return NULL;
}

client_state_t* get_or_create_client_state_entry(uint16_t conn_id) {
    // check if it exists
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] == conn_id) {
            return &client_states[i];
        }
    }

    // look for an empty slot
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] == 0xffff) {
            conn_id_map[i] = conn_id;

            // set default values
            memset(&client_states[i], 0, sizeof(client_state_t));
            client_states[i].conn_id = conn_id;
            client_states[i].handshake_start = xTaskGetTickCount();

            return &client_states[i];
        }
    }

    return NULL;
}

static void delete_client_state_entry(client_state_t* entry) {
    if (!entry) {
        return;
    }

    // Free allocated device settings
    if (entry->settings) {
        free(entry->settings);
        entry->settings = NULL;
    }

    // delete mapping
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] == entry->conn_id) {
            conn_id_map[i] = 0xffff;
        }
    }

    // zero out entry
    memset(entry, 0, sizeof(client_state_t));
}

int get_cert_state(uint16_t conn_id) {
    client_state_t* entry = get_client_state_entry(conn_id);
    if (!entry) {
        ESP_LOGE(HANDSHAKE_TAG, "get_cert_state: conn_id %d unknown", conn_id);
        return 0;
    }
    return entry->cert_state;
}

void set_remote_bda(uint16_t conn_id, esp_bd_addr_t remote_bda) {
    client_state_t* entry = get_or_create_client_state_entry(conn_id);
    if (!entry) {
        ESP_LOGE(HANDSHAKE_TAG, "set_remote_bda: conn_id %d unknown", conn_id);
        return;
    }
    memcpy(entry->remote_bda, remote_bda, sizeof(esp_bd_addr_t));
}

void connection_start(uint16_t conn_id) {
    // Safely increment active connections count
    if (mutex_acquire_blocking(active_connections_mutex)) {
        active_connections++;
        mutex_release(active_connections_mutex);
    }

    client_state_t* entry = get_client_state_entry(conn_id);
    if (!entry) {
        ESP_LOGE(HANDSHAKE_TAG, "connection_start: conn_id %d unknown", conn_id);
        return;
    }

    entry->conn_id = conn_id;
    entry->connection_start = xTaskGetTickCount();

    ESP_LOGI(HANDSHAKE_TAG,
        "[%d] connected, active_connections=%d, handshake_duration=%lu ms",
        conn_id,
        get_active_connections(),
        pdTICKS_TO_MS(entry->connection_start - entry->handshake_start));

    // After incrementing active_connections, stop advertising if we've reached target
    // This must happen AFTER the counter increment, not in ESP_GATTS_CONNECT_EVT
    // where active_connections hasn't been updated yet.
    int target = get_setting_uint8(&global_settings.target_active_connections);
    int current = get_active_connections();
    if (current >= target) {
        ESP_LOGI(
            HANDSHAKE_TAG, "[%d] reached target connections (%d/%d), stopping advertising", conn_id, current, target);
        pgp_advertise_stop();
    }
}

void connection_update(uint16_t conn_id) {
    client_state_t* entry = get_client_state_entry(conn_id);
    if (!entry) {
        ESP_LOGE(HANDSHAKE_TAG, "connection_update: conn_id %d unknown", conn_id);
        return;
    }
    entry->reconnection_at = xTaskGetTickCount();
}

void connection_stop(uint16_t conn_id, uint8_t reason) {
    // Safely decrement active connections count
    if (mutex_acquire_blocking(active_connections_mutex)) {
        active_connections--;
        if (active_connections < 0) {
            // I'm not entirely sure that we covered all paths so try to save something in case of
            // mistakes
            ESP_LOGE(HANDSHAKE_TAG, "we counted connections wrong!");
            active_connections = 0;
        }
        mutex_release(active_connections_mutex);
    }

    client_state_t* entry = get_client_state_entry(conn_id);
    if (!entry) {
        ESP_LOGE(HANDSHAKE_TAG, "connection_stop: conn_id %d unknown", conn_id);
        return;
    }

    entry->connection_end = xTaskGetTickCount();
    entry->cert_state = 0;

    // Detect early disconnect with cached session (potential stale cache)
    uint32_t conn_duration_ms = pdTICKS_TO_MS(entry->connection_end - entry->connection_start);
    if (conn_duration_ms < 5000 && entry->used_cached_session) {
        if (reason == ESP_GATT_CONN_TERMINATE_PEER_USER) {
            ESP_LOGW(HANDSHAKE_TAG, "[%d] Early disconnect detected with cached session, clearing cache", conn_id);
            clear_device_session(entry->remote_bda);
        }
    }

    ESP_LOGI(HANDSHAKE_TAG,
        "[%d] was connected for %lu ms",
        conn_id,
        pdTICKS_TO_MS(entry->connection_end - entry->connection_start));

    purge_button_queue_for_connection(conn_id);
    delete_client_state_entry(entry);
}

// Single source of truth for the client-state dump, shared by dump_client_states()
// (verbose debug log) and the Control Service's GET_CLIENT_STATES opcode
// (app-facing text). Both consumers get the same fields, so nothing can drift
// out of sync between a "log" version and a "buffer" version.
size_t dump_client_states_format(char* buf, size_t buf_len) {
    buf_writer_t writer;
    buf_writer_init(&writer, buf, buf_len);

    buf_writer_appendf(&writer, "active_connections: %d\nconn_id_map:\n", active_connections);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        buf_writer_appendf(&writer, "%d: %04x\n", i, conn_id_map[i]);
    }

    buf_writer_appendf(&writer, "client_states:\n");
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        client_state_t* entry = &client_states[i];
        buf_writer_appendf(&writer,
            "[%d] conn_id=%d cert_state=%d reconn_key=%d notify=%d\n"
            "  handshake=%lu reconnection=%lu conn_start=%lu conn_end=%lu\n",
            i,
            entry->conn_id,
            entry->cert_state,
            entry->has_reconnect_key,
            entry->notify,
            entry->handshake_start,
            entry->reconnection_at,
            entry->connection_start,
            entry->connection_end);
        if (entry->settings != NULL) {
            buf_writer_appendf(&writer,
                "  autospin=%s autocatch=%s\n",
                get_setting_log_value(&entry->settings->autospin),
                get_setting_log_value(&entry->settings->autocatch));
        }
        buf_writer_append_hex(&writer, "state_0_nonce", entry->state_0_nonce, sizeof(entry->state_0_nonce));
        buf_writer_append_hex(&writer, "the_challenge", entry->the_challenge, sizeof(entry->the_challenge));
        buf_writer_append_hex(&writer, "main_nonce", entry->main_nonce, sizeof(entry->main_nonce));
        buf_writer_append_hex(&writer, "outer_nonce", entry->outer_nonce, sizeof(entry->outer_nonce));
        buf_writer_append_hex(&writer, "session_key", entry->session_key, sizeof(entry->session_key));
        buf_writer_append_hex(
            &writer, "reconnect_challenge", entry->reconnect_challenge, sizeof(entry->reconnect_challenge));
    }
    return buf_writer_len(&writer);
}

void dump_client_states() {
    char buf[2048];
    dump_client_states_format(buf, sizeof(buf));
    ESP_LOGI(HANDSHAKE_TAG, "%s", buf);
}


// align with
// https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_security_server/main/example_ble_sec_gatts_demo.c
// instead
void reset_client_states() {
    ESP_LOGI(HANDSHAKE_TAG, "active_connections: %d", active_connections);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        // make sure it's not an empty slot
        if (conn_id_map[i] != 0xffff) {
            ESP_LOGI(HANDSHAKE_TAG, "disconnecting %d", i);
            esp_ble_gap_disconnect(client_states[i].remote_bda);
        }
    }
}

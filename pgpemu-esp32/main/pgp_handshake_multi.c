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
#include "stats.h"

#include <stdlib.h>
#include <string.h>

static int active_connections = 0;
static SemaphoreHandle_t active_connections_mutex = NULL;

#define MAX_CONNECTIONS CONFIG_BT_ACL_CONNECTIONS

// map which cert_states index corresponds to which conn_id
static uint16_t conn_id_map[MAX_CONNECTIONS] = { 0 };

// keep track of handshake state per connection
static client_state_t client_states[MAX_CONNECTIONS] = { 0 };

// Zero a slot but keep its settings mutex: FreeRTOS mutexes are created once per slot, never freed.
static void reset_client_state(client_state_t* entry) {
    SemaphoreHandle_t mutex = entry->settings.mutex;
    memset(entry, 0, sizeof(client_state_t));
    entry->settings.mutex = mutex;
}

void init_handshake_multi() {
    memset(conn_id_map, 0xff, sizeof(conn_id_map));
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (client_states[i].settings.mutex == NULL) {
            client_states[i].settings.mutex = xSemaphoreCreateMutex();
        }
    }
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
            reset_client_state(&client_states[i]);
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

    // delete mapping
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] == entry->conn_id) {
            conn_id_map[i] = 0xffff;
        }
    }

    reset_client_state(entry);
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
    client_state_t* entry = get_client_state_entry(conn_id);
    if (!entry) {
        ESP_LOGE(HANDSHAKE_TAG, "connection_start: conn_id %d unknown", conn_id);
        return;
    }

    // Safely increment active connections count
    if (mutex_acquire_blocking(active_connections_mutex)) {
        active_connections++;
        mutex_release(active_connections_mutex);
    }
    entry->counted_as_active = true;

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
        advertise_if_needed();
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
    client_state_t* entry = get_client_state_entry(conn_id);
    if (!entry) {
        ESP_LOGE(HANDSHAKE_TAG, "connection_stop: conn_id %d unknown", conn_id);
        return;
    }

    // Only decrement active_connections if this entry was actually counted, i.e.
    // connection_start() ran for it. A handshake that never completed never
    // incremented the counter, so stopping it must not decrement it either.
    if (entry->counted_as_active) {
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
        entry->counted_as_active = false;
    }

    entry->connection_end = xTaskGetTickCount();
    pgp_cert_state_t final_state = entry->cert_state;
    entry->cert_state = PGP_CERT_FRESH;
    entry->cert_len = 0;

    // Unproven against a capture: assumes the client drops the link when it rejects the cached keys.
    if (entry->used_cached_session && reason == ESP_GATT_CONN_TERMINATE_PEER_USER
        && final_state >= PGP_CERT_RECON_CHALLENGE_SENT && final_state <= PGP_CERT_RECON_ESTABLISHED_PENDING) {
        ESP_LOGW(HANDSHAKE_TAG,
            "[%d] disconnected in reconnect state %d with cached session, clearing cache",
            conn_id,
            final_state);
        clear_device_session(entry->remote_bda);
    }

    ESP_LOGI(HANDSHAKE_TAG,
        "[%d] was connected for %lu ms",
        conn_id,
        pdTICKS_TO_MS(entry->connection_end - entry->connection_start));

    purge_button_queue_for_connection(conn_id);
    // Bluedroid reuses conn_id values; stale counters would show on the next phone's profile
    stats_clear_for_conn(conn_id);
    delete_client_state_entry(entry);
}

// Client-state dump for the Control Service's GET_CLIENT_STATES opcode (app-facing text).
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
            (int)entry->cert_state,
            entry->has_reconnect_key,
            entry->notify,
            entry->handshake_start,
            entry->reconnection_at,
            entry->connection_start,
            entry->connection_end);
        if (entry->has_settings) {
            buf_writer_appendf(&writer,
                "  autospin=%s autocatch=%s\n",
                get_setting_log_value(&entry->settings.autospin),
                get_setting_log_value(&entry->settings.autocatch));
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

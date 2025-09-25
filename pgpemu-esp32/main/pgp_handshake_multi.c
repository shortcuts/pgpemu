#include "pgp_handshake_multi.h"

#include "esp_log.h"
#include "log_tags.h"

#include <string.h>

static int active_connections = 0;

#define MAX_CONNECTIONS CONFIG_BT_ACL_CONNECTIONS

// map which cert_states index corresponds to which conn_id
static uint16_t conn_id_map[MAX_CONNECTIONS] = { 0 };

// keep track of handshake state per connection
static client_state_t client_states[MAX_CONNECTIONS] = { 0 };

void init_handshake_multi() {
    memset(conn_id_map, 0xff, sizeof(conn_id_map));
}

int get_active_connections() {
    return active_connections;
}

client_state_t* get_client_state_entry(uint16_t conn_id) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] == conn_id) {
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

void connection_start(uint16_t conn_id) {
    active_connections++;

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
        active_connections,
        pdTICKS_TO_MS(entry->connection_start - entry->handshake_start));
}

void connection_update(uint16_t conn_id) {
    client_state_t* entry = get_client_state_entry(conn_id);
    if (!entry) {
        ESP_LOGE(HANDSHAKE_TAG, "connection_update: conn_id %d unknown", conn_id);
        return;
    }
    entry->reconnection_at = xTaskGetTickCount();
}

void connection_stop(uint16_t conn_id) {
    active_connections--;
    if (active_connections < 0) {
        // I'm not entirely sure that we covered all paths so try to save something in case of
        // mistakes
        ESP_LOGE(HANDSHAKE_TAG, "we counted connections wrong!");
        active_connections = 0;
    }

    client_state_t* entry = get_client_state_entry(conn_id);
    if (!entry) {
        ESP_LOGE(HANDSHAKE_TAG, "connection_stop: conn_id %d unknown", conn_id);
        return;
    }

    entry->connection_end = xTaskGetTickCount();
    entry->cert_state = 0;

    ESP_LOGI(HANDSHAKE_TAG,
        "[%d] was connected for %lu ms",
        conn_id,
        pdTICKS_TO_MS(entry->connection_end - entry->connection_start));

    delete_client_state_entry(entry);
}

static void dump_client_state(int idx, client_state_t* entry) {
    ESP_LOGI(HANDSHAKE_TAG,
        "[%d] %d cert_state=%d, recon_key=%d, notify=%d",
        entry->conn_id,
        idx,
        entry->cert_state,
        entry->has_reconnect_key,
        entry->notify);
    ESP_LOGI(HANDSHAKE_TAG,
        "timestamps: hs=%lu, rc=%lu, cs=%lu, ce=%lu",
        entry->handshake_start,
        entry->reconnection_at,
        entry->connection_start,
        entry->connection_end);

    ESP_LOGI(HANDSHAKE_TAG, "keys:");
    ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, entry->state_0_nonce, sizeof(entry->state_0_nonce));
    ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, entry->the_challenge, sizeof(entry->the_challenge));
    ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, entry->main_nonce, sizeof(entry->main_nonce));
    ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, entry->outer_nonce, sizeof(entry->outer_nonce));
    ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, entry->session_key, sizeof(entry->session_key));
    ESP_LOG_BUFFER_HEX(
        HANDSHAKE_TAG, entry->reconnect_challenge, sizeof(entry->reconnect_challenge));
}

void dump_client_states() {
    ESP_LOGI(HANDSHAKE_TAG, "active_connections: %d", active_connections);
    ESP_LOGI(HANDSHAKE_TAG, "conn_id_map:");
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        ESP_LOGI(HANDSHAKE_TAG, "%d: %04x", i, conn_id_map[i]);
    }

    ESP_LOGI(HANDSHAKE_TAG, "client_states:");
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        dump_client_state(i, &client_states[i]);
    }
}

#include "pgp_handshake.h"

#include "config_storage.h"
#include "esp_gatt_defs.h"
#include "esp_log.h"
#include "known_devices.h"
#include "log_tags.h"
#include "pgp_bluetooth.h"
#include "pgp_cert.h"
#include "pgp_control.h"
#include "pgp_gap.h"
#include "pgp_gatts.h"
#include "pgp_handshake_multi.h"

#include <stdint.h>
#include <string.h>

// disable using random values for the keys and nonces for debugging
static const bool use_debug_buffer_values = false;

uint16_t pgp_cert_read_slice(uint16_t conn_id, uint16_t offset, uint8_t* out, uint16_t out_cap) {
    client_state_t* client_state = get_client_state_entry(conn_id);
    if (!client_state || offset >= client_state->cert_len) {
        return 0;
    }
    // ATT payload is MTU - 1; 23 is the ATT default until ESP_GATTS_MTU_EVT arrives.
    uint16_t mtu = client_state->mtu ? client_state->mtu : 23;
    uint16_t len = client_state->cert_len - offset;
    if (len > mtu - 1) {
        len = mtu - 1;
    }
    if (len > out_cap) {
        len = out_cap;
    }
    memcpy(out, client_state->cert_buffer + offset, len);
    return len;
}

void handle_pgp_handshake_first(esp_gatt_if_t gatts_if, const uint8_t* value, uint16_t len, uint16_t conn_id) {
    if (len < 2) {
        ESP_LOGW(HANDSHAKE_TAG, "[%d] CCCD write too short: len=%d", conn_id, len);
        return;
    }
    uint16_t descr_value = value[1] << 8 | value[0];

    client_state_t* client_state = get_or_create_client_state_entry(conn_id);
    if (!client_state) {
        ESP_LOGE(HANDSHAKE_TAG, "[%d] couldn't get/create client state", conn_id);
        return;
    }

    if (descr_value == 0x0001) {
        client_state->notify = true;

        uint8_t notify_data[4];
        memset(notify_data, 0, 4);

        ESP_LOGI(HANDSHAKE_TAG,
            "[%d] Checking reconnection options: has_reconnect_key=%d, remote_bda=%02x:%02x:%02x:%02x:%02x:%02x",
            conn_id,
            client_state->has_reconnect_key,
            client_state->remote_bda[0],
            client_state->remote_bda[1],
            client_state->remote_bda[2],
            client_state->remote_bda[3],
            client_state->remote_bda[4],
            client_state->remote_bda[5]);

        if (client_state->has_reconnect_key) {
            // reconnect challenge
            ESP_LOGI(HANDSHAKE_TAG, "[%d] Using in-memory reconnect key", conn_id);
            notify_data[0] = 3;

            memset(client_state->cert_buffer, 0, 36);
            client_state->cert_buffer[0] = 3;
            memcpy(client_state->cert_buffer + 4, client_state->reconnect_challenge, 32);

            client_state->cert_len = 36;

            client_state->cert_state = PGP_CERT_RECON_CHALLENGE_SENT;
        } else if (has_cached_session(client_state->remote_bda)) {
            // Try to use cached session keys for faster reconnection
            if (retrieve_device_session_keys(
                    client_state->remote_bda, client_state->session_key, client_state->reconnect_challenge)) {
                client_state->has_reconnect_key = true;
                ESP_LOGI(HANDSHAKE_TAG, "[%d] Using cached session keys for reconnection", conn_id);
                client_state->used_cached_session = true;

                // Send reconnect challenge
                notify_data[0] = 3;
                memset(client_state->cert_buffer, 0, 36);
                client_state->cert_buffer[0] = 3;
                memcpy(client_state->cert_buffer + 4, client_state->reconnect_challenge, 32);

                client_state->cert_len = 36;

                client_state->cert_state = PGP_CERT_RECON_CHALLENGE_SENT;
            } else {
                // Cache retrieval failed, fall through to full handshake
                ESP_LOGW(
                    HANDSHAKE_TAG, "[%d] Failed to retrieve cached session keys, starting full handshake", conn_id);
                goto do_full_handshake;
            }
        } else {
        do_full_handshake:
            if (use_debug_buffer_values) {
                // use fixed key for easier debugging
                memset(client_state->the_challenge, 0x41, 16);
                memset(client_state->main_nonce, 0x42, 16);
                memset(client_state->session_key, 0x43, 16);
                memset(client_state->outer_nonce, 0x44, 16);
                ESP_LOGW(HANDSHAKE_TAG, "using static nonces");
            } else {
                randomize_buffer(client_state->the_challenge, 16);
                randomize_buffer(client_state->main_nonce, 16);
                randomize_buffer(client_state->session_key, 16);
                randomize_buffer(client_state->outer_nonce, 16);
            }

            generate_chal_0(bt_mac,
                client_state->the_challenge,
                client_state->main_nonce,
                client_state->session_key,
                client_state->outer_nonce,
                (struct challenge_data*)client_state->cert_buffer);

            client_state->cert_len = 378;
        }

        ESP_LOGD(HANDSHAKE_TAG, "[%d] start CERT PAIRING", conn_id);
        // the size of notify_data[] need less than MTU size
        esp_ble_gatts_send_indicate(gatts_if,
            conn_id,
            certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
            sizeof(notify_data),
            notify_data,
            false);
    } else if (descr_value == 0x0000) {
        client_state->notify = false;
        ESP_LOGD(HANDSHAKE_TAG, "[%d] notify disable", conn_id);
    } else {
        ESP_LOGE(HANDSHAKE_TAG, "unknown/indicate value");
    }
}

void handle_pgp_handshake_second(esp_gatt_if_t gatts_if, const uint8_t* prepare_buf, int datalen, uint16_t conn_id) {
    client_state_t* client_state = get_client_state_entry(conn_id);
    if (!client_state) {
        ESP_LOGE(HANDSHAKE_TAG, "[%d] couldn't get client state", conn_id);
        return;
    }

    if (client_state->cert_state >= PGP_CERT_FULL_CHALLENGE_SENT) {
        ESP_LOGD(
            HANDSHAKE_TAG, "[%d] Handshake state=%d, received %d b", conn_id, (int)client_state->cert_state, datalen);
    }

    // decrypt_next() reads a whole struct next_challenge; generate_reconnect_response() reads 32 bytes after a 4-byte
    // header
    int min_len = 0;
    switch (client_state->cert_state) {
    case PGP_CERT_FULL_CHALLENGE_SENT:
    case PGP_CERT_FULL_RESPONSE_PENDING:
        min_len = sizeof(struct next_challenge);
        break;
    case PGP_CERT_RECON_RESPONSE_PENDING:
        min_len = 4 + 32;
        break;
    default:
        break;
    }
    if (datalen < min_len) {
        ESP_LOGW(
            HANDSHAKE_TAG, "[%d] state %d write too short: len=%d", conn_id, (int)client_state->cert_state, datalen);
        return;
    }

    switch (client_state->cert_state) {
    case PGP_CERT_FRESH:  // normal challenge+response entry point
    {
        if (datalen == 20) {
            // just assume server responds correctly
            uint8_t notify_data[4];
            memset(notify_data, 0, 4);
            notify_data[0] = 0x01;

            if (use_debug_buffer_values) {
                memset(client_state->state_0_nonce, 0x42, 16);
            } else {
                randomize_buffer(client_state->state_0_nonce, 16);
            }

            uint8_t temp[52];
            memset(temp, 0, sizeof(temp));

            struct next_challenge* chal = (struct next_challenge*)temp;
            generate_next_chal(0, client_state->session_key, client_state->state_0_nonce, chal);

            temp[0] = 0x01;
            memcpy(client_state->cert_buffer, temp, 52);

            // ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, temp, sizeof(temp));
            // ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, cert_buffer, 52);

            client_state->cert_len = 52;
            esp_ble_gatts_send_indicate(gatts_if,
                conn_id,
                certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
                sizeof(notify_data),
                notify_data,
                false);

            client_state->cert_state = PGP_CERT_FULL_CHALLENGE_SENT;
        } else {
            ESP_LOGE(HANDSHAKE_TAG, "App sends incorrect len=%d", datalen);
        }
        break;
    }
    case PGP_CERT_FULL_CHALLENGE_SENT: {
        // we need to decrypt and send challenge data from APP
        uint8_t temp[20];
        memset(temp, 0, sizeof(temp));
        decrypt_next(prepare_buf, client_state->session_key, temp + 4);
        temp[0] = 0x02;

        uint8_t notify_data[4];
        memset(notify_data, 0, 4);
        notify_data[0] = 0x02;

        ESP_LOGD(HANDSHAKE_TAG, "Sending response");
        if (esp_log_level_get(HANDSHAKE_TAG) >= ESP_LOG_DEBUG) {
            ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, temp, sizeof(temp));
        }

        memcpy(client_state->cert_buffer, temp, 20);

        client_state->cert_len = 20;
        esp_ble_gatts_send_indicate(gatts_if,
            conn_id,
            certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
            sizeof(notify_data),
            notify_data,
            false);

        client_state->cert_state = PGP_CERT_FULL_RESPONSE_PENDING;
        break;
    }
    case PGP_CERT_FULL_RESPONSE_PENDING: {
        ESP_LOGD(HANDSHAKE_TAG, "OK");

        uint8_t temp[20];
        memset(temp, 0, sizeof(temp));
        decrypt_next(prepare_buf, client_state->session_key, temp + 4);

        if (esp_log_level_get(HANDSHAKE_TAG) >= ESP_LOG_DEBUG) {
            ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, temp, sizeof(temp));
        }

        // generate reconnect key
        client_state->has_reconnect_key = true;
        if (use_debug_buffer_values) {
            memset(client_state->reconnect_challenge, 0x46, 32);
        } else {
            randomize_buffer(client_state->reconnect_challenge, 32);
        }

        // Persist session keys for reconnection
        persist_device_session_keys(
            client_state->remote_bda, client_state->session_key, client_state->reconnect_challenge);

        uint8_t notify_data[4] = { 0x04, 0x00, 0x23, 0x00 };
        esp_ble_gatts_send_indicate(gatts_if,
            conn_id,
            certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
            sizeof(notify_data),
            notify_data,
            false);

        client_state->cert_state = PGP_CERT_DONE;
        pgp_control_emit_event(PGP_EVENT_GAME_CONNECTED, conn_id);
        known_devices_touch(client_state->remote_bda);
        connection_start(conn_id);
        advertise_if_needed();
        break;
    }
    case PGP_CERT_RECON_CHALLENGE_SENT:  // reconnection #1: entry point
    {
        if (datalen == 20) {
            // just assume server responds correctly
            ESP_LOGI(HANDSHAKE_TAG, "[%d] reconnection challenge received (state 3->4)", conn_id);
            if (esp_log_level_get(HANDSHAKE_TAG) >= ESP_LOG_DEBUG) {
                ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, prepare_buf, datalen);
            }

            uint8_t notify_data[4] = { 0x04, 0x00, 0x01, 0x00 };
            esp_ble_gatts_send_indicate(gatts_if,
                conn_id,
                certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
                sizeof(notify_data),
                notify_data,
                false);

            client_state->cert_state = PGP_CERT_RECON_RESPONSE_PENDING;
        } else {
            ESP_LOGW(HANDSHAKE_TAG, "[%d] reconnection #1 unexpected datalen=%d", conn_id, datalen);
        }
        break;
    }
    case PGP_CERT_RECON_RESPONSE_PENDING:  // reconnection #2
    {
        ESP_LOGI(HANDSHAKE_TAG, "[%d] reconnection response received (state 4->5)", conn_id);

        memset(client_state->cert_buffer, 0, 4);
        generate_reconnect_response(client_state->session_key, prepare_buf + 4, client_state->cert_buffer + 4);
        client_state->cert_buffer[0] = 5;

        if (esp_log_level_get(HANDSHAKE_TAG) >= ESP_LOG_DEBUG) {
            ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, client_state->cert_buffer, 20);
        }

        uint8_t notify_data[4];
        memset(notify_data, 0, 4);
        notify_data[0] = 0x05;

        client_state->cert_len = 20;
        esp_ble_gatts_send_indicate(gatts_if,
            conn_id,
            certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
            sizeof(notify_data),
            notify_data,
            false);

        client_state->cert_state = PGP_CERT_RECON_ESTABLISHED_PENDING;
        break;
    }
    case PGP_CERT_RECON_ESTABLISHED_PENDING:  // reconnection #3: established
    {
        if (datalen == 5) {
            // just assume server responds correctly
            ESP_LOGI(HANDSHAKE_TAG, "[%d] reconnection complete (state 5->6)", conn_id);

            uint8_t notify_data[4] = { 0x04, 0x00, 0x02, 0x00 };
            esp_ble_gatts_send_indicate(gatts_if,
                conn_id,
                certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
                sizeof(notify_data),
                notify_data,
                false);

            client_state->cert_state = PGP_CERT_DONE;
            pgp_control_emit_event(PGP_EVENT_GAME_CONNECTED, conn_id);
            known_devices_touch(client_state->remote_bda);
            // For reconnections on a fresh entry (connection_start == 0), increment the counter.
            // For reconnections on an existing entry (connection_start != 0), just update timestamp.
            // This handles both scenarios: fresh slots vs reconnections within same slot.
            if (client_state->connection_start == 0) {
                connection_start(conn_id);
            } else {
                connection_update(conn_id);
            }
            advertise_if_needed();
        } else {
            ESP_LOGW(HANDSHAKE_TAG, "[%d] reconnection #3 unexpected datalen=%d", conn_id, datalen);
        }
        break;
    }
    default:
        ESP_LOGE(HANDSHAKE_TAG, "Unhandled state: %d", (int)client_state->cert_state);
        break;
    }
}

void pgp_handshake_disconnect(uint16_t conn_id, uint8_t reason) {
    // this deletes the client state entry
    connection_stop(conn_id, reason);
}

int pgp_get_handshake_state(uint16_t conn_id) {
    return get_cert_state(conn_id);
}

#include "pgp_handshake.h"

#include <stdint.h>
#include <string.h>

#include "esp_gatt_defs.h"
#include "esp_log.h"
#include "log_tags.h"
#include "pgp_bluetooth.h"
#include "pgp_cert.h"
#include "pgp_gap.h"
#include "pgp_gatts.h"
#include "pgp_handshake_multi.h"

// disable using random values for the keys and nonces for debugging
static const bool use_debug_buffer_values = false;

void handle_pgp_handshake_first(esp_gatt_if_t gatts_if, uint16_t descr_value, uint16_t conn_id) {
    client_state_t* client_state = get_or_create_client_state_entry(conn_id);
    if (!client_state) {
        ESP_LOGE(HANDSHAKE_TAG, "couldn't get/create client state, conn_id=%d", conn_id);
        return;
    }

    if (descr_value == 0x0001) {
        client_state->notify = true;

        uint8_t notify_data[4];
        memset(notify_data, 0, 4);

        if (client_state->has_reconnect_key) {
            // reconnect challenge
            notify_data[0] = 3;

            memset(client_state->cert_buffer, 0, 36);
            client_state->cert_buffer[0] = 3;
            memcpy(client_state->cert_buffer + 4, client_state->reconnect_challenge, 32);

            esp_ble_gatts_set_attr_value(certificate_handle_table[IDX_CHAR_SFIDA_TO_CENTRAL_VAL],
                                         36, client_state->cert_buffer);

            client_state->cert_state = 3;
        } else {
            // first challenge
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

            generate_chal_0(bt_mac, client_state->the_challenge, client_state->main_nonce,
                            client_state->session_key, client_state->outer_nonce,
                            (struct challenge_data*)client_state->cert_buffer);

            esp_ble_gatts_set_attr_value(certificate_handle_table[IDX_CHAR_SFIDA_TO_CENTRAL_VAL],
                                         378, client_state->cert_buffer);
        }

        ESP_LOGD(HANDSHAKE_TAG, "start CERT PAIRING, conn_id=%d", conn_id);
        // the size of notify_data[] need less than MTU size
        esp_ble_gatts_send_indicate(gatts_if, conn_id,
                                    certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
                                    sizeof(notify_data), notify_data, false);
    } else if (descr_value == 0x0000) {
        client_state->notify = false;
        ESP_LOGD(HANDSHAKE_TAG, "notify disable, conn_id=%d", conn_id);
    } else {
        ESP_LOGE(HANDSHAKE_TAG, "unknown/indicate value");
    }
}

void handle_pgp_handshake_second(esp_gatt_if_t gatts_if, const uint8_t* prepare_buf, int datalen,
                                 uint16_t conn_id) {
    client_state_t* client_state = get_client_state_entry(conn_id);
    if (!client_state) {
        ESP_LOGE(HANDSHAKE_TAG, "couldn't get client state, conn_id=%d", conn_id);
        return;
    }

    if (client_state->cert_state >= 1) {
        ESP_LOGD(HANDSHAKE_TAG, "Handshake state=%d, received %d b, conn_id=%d",
                 client_state->cert_state, datalen, conn_id);
    }

    switch (client_state->cert_state) {
        case 0:  // normal challenge+response entry point
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

                esp_ble_gatts_set_attr_value(
                    certificate_handle_table[IDX_CHAR_SFIDA_TO_CENTRAL_VAL], 52,
                    client_state->cert_buffer);
                esp_ble_gatts_send_indicate(gatts_if, conn_id,
                                            certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
                                            sizeof(notify_data), notify_data, false);

                client_state->cert_state = 1;
            } else {
                ESP_LOGE(HANDSHAKE_TAG, "App sends incorrect len=%d", datalen);
            }
            break;
        }
        case 1: {
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

            esp_ble_gatts_set_attr_value(certificate_handle_table[IDX_CHAR_SFIDA_TO_CENTRAL_VAL],
                                         20, client_state->cert_buffer);
            esp_ble_gatts_send_indicate(gatts_if, conn_id,
                                        certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
                                        sizeof(notify_data), notify_data, false);

            client_state->cert_state = 2;
            break;
        }
        case 2: {
            // TODO: what do we use the data for?
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

            uint8_t notify_data[4] = {0x04, 0x00, 0x23, 0x00};
            esp_ble_gatts_send_indicate(gatts_if, conn_id,
                                        certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
                                        sizeof(notify_data), notify_data, false);

            client_state->cert_state = 6;
            connection_start(conn_id);
            advertise_if_needed();
            break;
        }
        case 3:  // reconnection #1: entry point
        {
            if (datalen == 20) {
                // just assume server responds correctly
                ESP_LOGD(HANDSHAKE_TAG, "OK");
                if (esp_log_level_get(HANDSHAKE_TAG) >= ESP_LOG_DEBUG) {
                    ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, prepare_buf, datalen);
                }

                uint8_t notify_data[4] = {0x04, 0x00, 0x01, 0x00};
                esp_ble_gatts_send_indicate(gatts_if, conn_id,
                                            certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
                                            sizeof(notify_data), notify_data, false);

                client_state->cert_state = 4;
            }
            break;
        }
        case 4:  // reconnection #2
        {
            ESP_LOGD(HANDSHAKE_TAG, "OK");

            memset(client_state->cert_buffer, 0, 4);
            generate_reconnect_response(client_state->session_key, prepare_buf + 4,
                                        client_state->cert_buffer + 4);
            client_state->cert_buffer[0] = 5;

            if (esp_log_level_get(HANDSHAKE_TAG) >= ESP_LOG_DEBUG) {
                ESP_LOG_BUFFER_HEX(HANDSHAKE_TAG, client_state->cert_buffer, 20);
            }

            uint8_t notify_data[4];
            memset(notify_data, 0, 4);
            notify_data[0] = 0x05;

            esp_ble_gatts_set_attr_value(certificate_handle_table[IDX_CHAR_SFIDA_TO_CENTRAL_VAL],
                                         20, client_state->cert_buffer);
            esp_ble_gatts_send_indicate(gatts_if, conn_id,
                                        certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
                                        sizeof(notify_data), notify_data, false);

            client_state->cert_state = 5;
            break;
        }
        case 5:  // reconnection #3: established
        {
            if (datalen == 5) {
                // just assume server responds correctly
                ESP_LOGD(HANDSHAKE_TAG, "OK");

                uint8_t notify_data[4] = {0x04, 0x00, 0x02, 0x00};
                esp_ble_gatts_send_indicate(gatts_if, conn_id,
                                            certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL],
                                            sizeof(notify_data), notify_data, false);

                client_state->cert_state = 6;
                connection_update(conn_id);
            }
            break;
        }
        default:
            ESP_LOGE(HANDSHAKE_TAG, "Unhandled state: %d", client_state->cert_state);
            break;
    }
}

void pgp_handshake_disconnect(uint16_t conn_id) {
    // this deletes the client state entry
    connection_stop(conn_id);
}

int pgp_get_handshake_state(uint16_t conn_id) { return get_cert_state(conn_id); }

#ifndef PGP_HANDSHAKE_MULTI_H
#define PGP_HANDSHAKE_MULTI_H

#include "esp_bt_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/task.h"

#include <portmacro.h>
#include <stdbool.h>
#include <stdint.h>

static const size_t CERT_BUFFER_LEN = 378;

typedef struct {
    // esp bt connection id
    uint16_t conn_id;
    esp_bd_addr_t remote_bda;
    int cert_state;
    // TODO: we probably need to save the remote mac address so that we associate a reconnecting
    // client with its previous client state
    bool has_reconnect_key;
    bool notify;

    uint8_t cert_buffer[378];

    uint8_t state_0_nonce[16];

    uint8_t the_challenge[16];
    uint8_t main_nonce[16];
    uint8_t outer_nonce[16];

    uint8_t session_key[16];
    uint8_t reconnect_challenge[32];

    TickType_t handshake_start, reconnection_at, connection_start, connection_end;
} client_state_t;

void init_handshake_multi();

int get_active_connections();

// returns NULL when conn_id unknown
client_state_t* get_client_state_entry(uint16_t conn_id);
// returns NULL only if conn_id unknown and max connections reached
client_state_t* get_or_create_client_state_entry(uint16_t conn_id);

int get_cert_state(uint16_t conn_id);
void set_remote_bda(uint16_t conn_id, esp_bd_addr_t remote_bda);

void dump_client_states();
void reset_client_states();


void connection_start(uint16_t conn_id);
void connection_update(uint16_t conn_id);
void connection_stop(uint16_t conn_id);

#endif /* PGP_HANDSHAKE_MULTI_H */

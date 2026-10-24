#ifndef PGP_HANDSHAKE_MULTI_H
#define PGP_HANDSHAKE_MULTI_H

#include "esp_bt_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/task.h"
#include "settings.h"

#include <portmacro.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static const size_t CERT_BUFFER_LEN = 378;

// Values are the numbers the logs and diagnostics dump print; keep them stable.
typedef enum {
    PGP_CERT_FRESH = 0,
    PGP_CERT_FULL_CHALLENGE_SENT = 1,
    PGP_CERT_FULL_RESPONSE_PENDING = 2,
    PGP_CERT_RECON_CHALLENGE_SENT = 3,
    PGP_CERT_RECON_RESPONSE_PENDING = 4,
    PGP_CERT_RECON_ESTABLISHED_PENDING = 5,
    PGP_CERT_DONE = 6,
} pgp_cert_state_t;

typedef struct {
    // connection informations
    uint16_t conn_id;
    esp_bd_addr_t remote_bda;
    // Embedded so connect/disconnect never allocate; its mutex lives as long as the slot.
    DeviceSettings settings;
    // Set once the connect path has loaded settings into this slot.
    bool has_settings;

    // cert informations
    pgp_cert_state_t cert_state;
    // TODO: we probably need to save the remote mac address so that we associate a reconnecting
    // client with its previous client state
    bool has_reconnect_key;
    bool notify;

    uint8_t cert_buffer[378];
    // valid bytes in cert_buffer; a read of the shared attribute serves this many
    uint16_t cert_len;
    // negotiated ATT MTU, 0 until ESP_GATTS_MTU_EVT
    uint16_t mtu;

    uint8_t state_0_nonce[16];

    uint8_t the_challenge[16];
    uint8_t main_nonce[16];
    uint8_t outer_nonce[16];

    uint8_t session_key[16];
    uint8_t reconnect_challenge[32];

    TickType_t handshake_start, reconnection_at, connection_start, connection_end;
    bool used_cached_session;
    // Set by connection_start() once this entry has been counted in the global
    // active_connections total. connection_stop() only decrements when this is
    // set, so stopping a handshake that never completed can't undercount.
    bool counted_as_active;
    // Set once BLE auth (ESP_GAP_BLE_AUTH_CMPL_EVT) succeeds on this connection instance.
    // Lets pgp_gap.c tell a fresh pairing's auth failure apart from a transient auth
    // hiccup on a link that already authenticated once and is still live.
    bool auth_succeeded;
} client_state_t;

void init_handshake_multi();

int get_active_connections();
int get_max_connections();

// returns NULL when conn_id unknown
client_state_t* get_client_state_entry(uint16_t conn_id);
// returns NULL only if conn_id unknown and max connections reached
client_state_t* get_or_create_client_state_entry(uint16_t conn_id);
// returns NULL when idx has no associated connection
client_state_t* get_client_state_entry_by_idx(int i);
// returns the client_states index of conn_id, or -1 when unknown
int get_client_slot(uint16_t conn_id);
// returns NULL when no currently-connected client matches bda
client_state_t* get_client_state_entry_by_bda(esp_bd_addr_t bda);

// returns true if conn_id exists and is currently connected
bool is_connection_active(uint16_t conn_id);

int get_cert_state(uint16_t conn_id);
void set_remote_bda(uint16_t conn_id, esp_bd_addr_t remote_bda);

// Formats the client-state dump into buf.
// Returns the number of bytes written (excluding the null terminator).
size_t dump_client_states_format(char* buf, size_t buf_len);
void reset_client_states();


void connection_start(uint16_t conn_id);
void connection_update(uint16_t conn_id);
void connection_stop(uint16_t conn_id, uint8_t reason);

#endif /* PGP_HANDSHAKE_MULTI_H */

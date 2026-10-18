#ifndef PGP_HANDSHAKE_H
#define PGP_HANDSHAKE_H

#include "esp_gatt_defs.h"

#include <stdint.h>

void handle_pgp_handshake_first(esp_gatt_if_t gatts_if, const uint8_t* value, uint16_t len, uint16_t conn_id);
void handle_pgp_handshake_second(esp_gatt_if_t gatts_if, const uint8_t* prepare_buf, int datalen, uint16_t conn_id);

// Copies the part of conn_id's own cert buffer a read at offset gets; returns bytes copied.
uint16_t pgp_cert_read_slice(uint16_t conn_id, uint16_t offset, uint8_t* out, uint16_t out_cap);

void pgp_handshake_disconnect(uint16_t conn_id, uint8_t reason);

int pgp_get_handshake_state(uint16_t conn_id);

#endif /* PGP_HANDSHAKE_H */

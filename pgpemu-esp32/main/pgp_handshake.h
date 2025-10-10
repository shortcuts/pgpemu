#ifndef PGP_HANDSHAKE_H
#define PGP_HANDSHAKE_H

#include "esp_gatt_defs.h"

#include <stdint.h>

void handle_pgp_handshake_first(esp_gatt_if_t gatts_if, uint16_t descr_value, uint16_t conn_id);
void handle_pgp_handshake_second(esp_gatt_if_t gatts_if, const uint8_t* prepare_buf, int datalen, uint16_t conn_id);

void pgp_handshake_disconnect(uint16_t conn_id);

int pgp_get_handshake_state(uint16_t conn_id);

#endif /* PGP_HANDSHAKE_H */

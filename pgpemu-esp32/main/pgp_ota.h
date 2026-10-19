#ifndef PGP_OTA_H
#define PGP_OTA_H

#include "pgp_control.h"

#include <stddef.h>
#include <stdint.h>

// Starts the 60 s timer that confirms a freshly booted image; call once after Bluetooth is up.
void pgp_ota_init(void);

// Opens the OTA window for OTA_ARM_WINDOW_MS; called from the board button task.
void pgp_ota_arm(void);
bool pgp_ota_is_armed(void);

// Starts a session on the inactive slot. mtu is the negotiated ATT MTU of conn_id; chunk_out is the
// largest OTA_DATA payload the app may send.
control_status_t pgp_ota_begin(uint16_t conn_id, uint32_t total, uint16_t mtu, uint16_t* chunk_out);
control_status_t pgp_ota_data(uint16_t conn_id, uint32_t offset, const uint8_t* data, size_t len, uint32_t* next_out);
// Validates the image and selects it for the next boot.
control_status_t pgp_ota_end(uint16_t conn_id);
// Only the session owner may abort; another bonded phone must not kill a running update.
control_status_t pgp_ota_abort(uint16_t conn_id);
void pgp_ota_on_disconnect(uint16_t conn_id);

#endif /* PGP_OTA_H */

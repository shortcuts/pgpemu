#ifndef PGP_AUTOBUTTON_H
#define PGP_AUTOBUTTON_H

#include "esp_gatt_defs.h"

#include <stdbool.h>
#include <stdint.h>

bool init_autobutton();

// Presses the button on conn_id after delay_ms. Never blocks; returns false (press dropped) when no slot is free.
bool schedule_button_press(esp_gatt_if_t gatts_if, uint16_t conn_id, int delay_ms);

void purge_button_queue_for_connection(uint16_t conn_id);

#endif /* PGP_AUTOBUTTON_H */

#ifndef PGP_LED_HANDLER_H
#define PGP_LED_HANDLER_H

#include <stdint.h>

#include "esp_gatt_defs.h"

void handle_led_notify_from_app(esp_gatt_if_t gatts_if, uint16_t conn_id, const uint8_t* buffer);

#endif /* PGP_LED_HANDLER_H */

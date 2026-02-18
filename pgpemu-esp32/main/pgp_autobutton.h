#ifndef PGP_AUTOBUTTON_H
#define PGP_AUTOBUTTON_H

#include "esp_gatt_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    // which session does this belong to
    esp_gatt_if_t gatts_if;
    uint16_t conn_id;

    // delay after which button is pressed
    int delay;
} button_queue_item_t;

extern QueueHandle_t button_queue;

bool init_autobutton();
void purge_button_queue_for_connection(uint16_t conn_id);

#endif /* PGP_AUTOBUTTON_H */

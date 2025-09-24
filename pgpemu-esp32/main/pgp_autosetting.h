#ifndef PGP_AUTOSETTING_H
#define PGP_AUTOSETTING_H

#include "esp_gatt_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    // which session does this belong to
    esp_gatt_if_t gatts_if;
    uint16_t conn_id;

    // delay after which setting is pressed
    int delay;

    char setting;
} setting_queue_item_t;

extern QueueHandle_t setting_queue;

bool init_autosetting();

#endif /* PGP_AUTOSETTING_H */

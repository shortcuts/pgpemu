#ifndef PGP_AUTOSETTING_H
#define PGP_AUTOSETTING_H

#include "esp_gatt_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

typedef struct {
    // which session does this belong to
    esp_gatt_if_t gatts_if;
    uint16_t conn_id;

    // NEW: Session identifier to match device
    uint32_t session_id;

    // delay after which setting is pressed
    int delay;

    char setting;
} setting_queue_item_t;

typedef struct {
    uint32_t session_id;
    char setting;
} retoggle_timer_data_t;

extern QueueHandle_t setting_queue;

bool init_autosetting();

#endif /* PGP_AUTOSETTING_H */

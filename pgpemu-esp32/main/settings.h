#ifndef SETTINGS_H
#define SETTINGS_H

#include "esp_gatt_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/semphr.h"

#include <stdbool.h>

typedef struct {
    // any read/write must lock this
    SemaphoreHandle_t mutex;

    // set which PGP device presets stored in NVS should be cloned
    uint8_t chosen_device;

    // set how many client connections are allowed at the same time
    uint8_t target_active_connections;

    // gotcha functions
    bool autocatch, autospin;

    // do you have an input button? only checked on boot
    bool use_button;

    // 1 = debug, 2 = info, 3 = verbose
    uint8_t log_level;
} Settings;

extern Settings settings;

void init_settings();
void settings_ready();
bool toggle_setting(bool* var);
bool cycle_setting(uint8_t* var, uint8_t min, uint8_t max);
bool get_setting(bool* var);
char* get_setting_log_value(bool* var);
uint8_t get_setting_uint8(uint8_t* var);
bool set_setting_uint8(uint8_t* var, const uint8_t val);
bool set_chosen_device(uint8_t id);

#endif /* SETTINGS_H */

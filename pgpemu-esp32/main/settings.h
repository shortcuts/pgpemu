#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/semphr.h"

typedef struct {
    // any read/write must lock this
    SemaphoreHandle_t mutex;

    // set which PGP device presets stored in NVS should be cloned
    uint8_t chosen_device;

    // set how many client connections are allowed at the same time
    uint8_t target_active_connections;

    // gotcha functions
    bool autocatch, autospin;

    // waste a bit of power to keep your powerbank from turning us off
    bool powerbank_ping;

    // do you have an input button? only checked on boot
    bool use_button;

    // do you have an LED? only checked on boot
    bool use_led;

    // show pokestop/pokemon interactions using the LED (errors like "bag full" are always shown)
    bool led_interactions;

    // 1 = debug, 2 = info, 3 = verbose
    uint8_t log_level;

    // very verbose log messages
    bool very_verbose;
} Settings;

extern Settings settings;

void init_settings();
void settings_ready();
bool toggle_setting(bool* var);
bool cycle_setting(uint8_t* var, uint8_t min, uint8_t max);
bool get_setting(bool* var);
uint8_t get_setting_uint8(uint8_t* var);
bool set_setting_bool(bool* var, bool val);
bool set_setting_uint8(uint8_t* var, const uint8_t val);
bool set_chosen_device(uint8_t id);

#endif /* SETTINGS_H */

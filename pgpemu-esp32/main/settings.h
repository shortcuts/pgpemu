#ifndef SETTINGS_H
#define SETTINGS_H

#include "esp_gatt_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/semphr.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    // any read/write must lock this
    SemaphoreHandle_t mutex;

    // set how many client connections are allowed at the same time
    uint8_t target_active_connections;

    // 1 = debug, 2 = info, 3 = verbose
    uint8_t log_level;
} GlobalSettings;

extern GlobalSettings global_settings;

typedef struct {
    // any read/write must lock this
    SemaphoreHandle_t mutex;

    esp_bd_addr_t bda;

    // gotcha functions
    bool autocatch, autospin;

    // 0 = spin everything, 1 to 9 = N/10
    uint8_t autospin_probability;

    // Retoggle state tracking (for bag full/box full scenarios)
    bool autospin_retoggle_pending;
    bool autocatch_retoggle_pending;
    TickType_t autospin_retoggle_time;    // when to restore autospin
    TickType_t autocatch_retoggle_time;   // when to restore autocatch
} DeviceSettings;

void init_global_settings();
void global_settings_ready();
bool toggle_setting(bool* var);
bool toggle_device_autospin(uint8_t c);
bool toggle_device_autocatch(uint8_t c);
uint8_t set_device_autospin_probability(uint8_t c, uint8_t autospin_probability);
bool cycle_log_level(uint8_t* var);
bool get_setting(bool* var);
char* get_setting_log_value(bool* var);
uint8_t get_setting_uint8(uint8_t* var);
bool set_setting_uint8(uint8_t* var, const uint8_t val);

#endif /* SETTINGS_H */

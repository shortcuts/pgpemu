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

    // enable/disable BLE advertising to save power
    bool advertising_enabled;
} GlobalSettings;

extern GlobalSettings global_settings;

typedef enum { PGP_CATCH_OFF = 0, PGP_CATCH_ALL = 1, PGP_CATCH_NEW_ONLY = 2 } pgp_catch_mode_t;

typedef struct {
    // any read/write must lock this
    SemaphoreHandle_t mutex;

    esp_bd_addr_t bda;

    // gotcha functions
    bool autospin;
    uint8_t catch_mode;  // pgp_catch_mode_t
} DeviceSettings;

void init_global_settings();
void global_settings_ready();
bool toggle_setting(bool* var);
bool toggle_device_autospin(uint8_t c);
bool toggle_device_autocatch(uint8_t c);
bool set_device_autospin(uint8_t c, bool val);
bool set_device_autocatch(uint8_t c, bool val);
bool set_device_catch_mode(uint8_t c, uint8_t mode);
bool cycle_log_level(uint8_t* var);
bool get_setting(bool* var);
char* get_setting_log_value(bool* var);
uint8_t get_setting_uint8(uint8_t* var);
bool set_setting_bool(bool* var, bool val);
bool set_setting_uint8(uint8_t* var, const uint8_t val);

#endif /* SETTINGS_H */

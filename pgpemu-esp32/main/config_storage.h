#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include "esp_bt_defs.h"
#include "settings.h"

#include <stdbool.h>

void init_settings_nvs_partition();

// read the global settings from nvs and set it to the settings singleton, use_mutex is only meant to be false on
// app_main startup, when the mutex doesn't exists yet.
void read_stored_global_settings(bool use_mutex);
// read the device settings from nvs by trying to match the bda.
// Populates the provided DeviceSettings struct pointer with loaded values
// Returns true if settings were loaded, false on error
bool read_stored_device_settings(esp_bd_addr_t bda, DeviceSettings* out_settings);

bool write_global_settings_to_nvs();
bool write_device_settings_to_nvs(uint8_t slot);
bool write_devices_settings_to_nvs();

// Session key persistence for device reconnection
// After successful handshake, persist the session keys to NVS
bool persist_device_session_keys(esp_bd_addr_t bda, const uint8_t* session_key, const uint8_t* reconnect_challenge);

// On reconnection attempt, retrieve cached keys from NVS
bool retrieve_device_session_keys(esp_bd_addr_t bda, uint8_t* session_key_out, uint8_t* reconnect_challenge_out);

// Check if device has cached session keys
bool has_cached_session(esp_bd_addr_t bda);

// Clear session keys on manual user request (optional)
bool clear_device_session(esp_bd_addr_t bda);

// Erase the session keys and the device settings of one address.
bool erase_device_data(esp_bd_addr_t bda);

// Lifetime counters of one address: six u32 LE, see lifetime_stats.h. False when no blob is stored.
#define LIFETIME_STATS_BLOB_LEN 24
bool read_lifetime_stats(esp_bd_addr_t bda, uint8_t blob[LIFETIME_STATS_BLOB_LEN]);
bool write_lifetime_stats(esp_bd_addr_t bda, const uint8_t blob[LIFETIME_STATS_BLOB_LEN]);

#endif /* CONFIG_STORAGE_H */

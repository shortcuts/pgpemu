#ifndef NVS_HELPER_H
#define NVS_HELPER_H

#include <stdbool.h>

#include "esp_system.h"

/**
 * @brief Check result of NVS read operation and log warning/error if needed.
 * @param tag Log tag for ESP-IDF logging
 * @param err Error code from NVS API
 * @param name Name of the NVS key
 * @return true if read was successful, false otherwise
 */
bool nvs_read_check(const char* tag, esp_err_t err, const char* name);

/**
 * @brief Check result of NVS write operation and log error if needed.
 * @param tag Log tag for ESP-IDF logging
 * @param err Error code from NVS API
 * @param name Name of the NVS key
 * @return true if write was successful, false otherwise
 */
bool nvs_write_check(const char* tag, esp_err_t err, const char* name);

#endif /* NVS_HELPER_H */
#ifndef NVS_HELPER_H
#define NVS_HELPER_H

#include "esp_system.h"
#include "nvs.h"

#include <stdbool.h>
#include <stddef.h>

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

/**
 * @brief Safely open NVS partition in readonly mode.
 * @param tag Log tag for ESP-IDF logging
 * @param namespace Namespace name
 * @param out_handle Pointer to handle to receive the opened handle
 * @return true if opened successfully, false otherwise
 *
 * Handles ESP_ERR_NVS_NOT_FOUND specially (logs warning instead of error).
 * For other errors, panics with ESP_ERROR_CHECK.
 */
bool nvs_open_readonly(const char* tag, const char* namespace, nvs_handle_t* out_handle);

/**
 * @brief Safely open NVS partition in readwrite mode.
 * @param tag Log tag for ESP-IDF logging
 * @param namespace Namespace name
 * @param out_handle Pointer to handle to receive the opened handle
 * @return true if opened successfully, false otherwise
 *
 * Panics on any error with ESP_ERROR_CHECK.
 */
bool nvs_open_readwrite(const char* tag, const char* namespace, nvs_handle_t* out_handle);

/**
 * @brief Safely close an NVS handle (no-op if handle is 0/invalid).
 * @param handle Handle to close
 */
void nvs_safe_close(nvs_handle_t handle);

/**
 * @brief Read a blob from NVS with size validation.
 * @param tag Log tag for ESP-IDF logging
 * @param handle NVS handle
 * @param key NVS key
 * @param out_buf Buffer to receive blob data
 * @param expected_size Expected size of blob in bytes
 * @return true if read successfully and size matches, false otherwise
 *
 * First queries the blob size, validates it matches expected_size, then reads.
 * Logs appropriate warnings/errors if read fails or size doesn't match.
 */
bool nvs_read_blob_checked(const char* tag,
    nvs_handle_t handle,
    const char* key,
    uint8_t* out_buf,
    size_t expected_size);

/**
 * @brief Commit NVS changes and close handle atomically.
 * @param tag Log tag for ESP-IDF logging
 * @param handle NVS handle to commit and close
 * @param key_name Human-readable name of what was written (for logging)
 * @return true if committed successfully, false otherwise
 *
 * Always closes the handle, even if commit fails. Logs errors appropriately.
 */
bool nvs_commit_and_close(const char* tag, nvs_handle_t handle, const char* key_name);

#endif /* NVS_HELPER_H */
/**
 * @file mutex_helpers.h
 * @brief Mutex helper functions for FreeRTOS mutual exclusion
 *
 * Provides simplified and consistent mutex operations with proper NULL checking
 * and timeout handling. This reduces code duplication and improves safety.
 */

#ifndef MUTEX_HELPERS_H
#define MUTEX_HELPERS_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * @brief Acquire mutex with blocking (indefinite) timeout
 *
 * Safely acquires a mutex, blocking until available. Handles NULL mutexes
 * gracefully by returning false immediately.
 *
 * @param mutex The SemaphoreHandle_t mutex to acquire
 * @return true if mutex was successfully acquired, false if NULL or error
 *
 * Example:
 *   if (mutex_acquire_blocking(my_mutex)) {
 *       // Critical section
 *       mutex_release(my_mutex);
 *   }
 */
inline static bool mutex_acquire_blocking(SemaphoreHandle_t mutex) {
    if (!mutex) {
        return false;
    }
    return xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE;
}

/**
 * @brief Acquire mutex with timeout (milliseconds)
 *
 * Attempts to acquire a mutex with a specified timeout. Converts milliseconds
 * to FreeRTOS tick periods automatically.
 *
 * @param mutex The SemaphoreHandle_t mutex to acquire
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return true if acquired, false if timeout or NULL
 *
 * Example:
 *   if (mutex_acquire_timeout(my_mutex, 100)) {
 *       // Critical section
 *       mutex_release(my_mutex);
 *   } else {
 *       ESP_LOGW(TAG, "mutex timeout");
 *   }
 */
inline static bool mutex_acquire_timeout(SemaphoreHandle_t mutex, uint32_t timeout_ms) {
    if (!mutex) {
        return false;
    }
    TickType_t timeout_ticks = (timeout_ms == 0) ? 0 : (timeout_ms / portTICK_PERIOD_MS);
    return xSemaphoreTake(mutex, timeout_ticks) == pdTRUE;
}

/**
 * @brief Release previously acquired mutex
 *
 * Safely releases a mutex. Handles NULL mutexes gracefully (no-op).
 *
 * @param mutex The SemaphoreHandle_t mutex to release
 *
 * Example:
 *   mutex_release(my_mutex);
 */
inline static void mutex_release(SemaphoreHandle_t mutex) {
    if (mutex) {
        xSemaphoreGive(mutex);
    }
}

/**
 * @brief Guard scope with mutex - blocking acquisition
 *
 * Convenience macro for automatic mutex acquisition and release using a for-loop guard.
 * Mutex is held throughout the block and released on exit (including early returns).
 *
 * @param mutex The SemaphoreHandle_t mutex to acquire
 *
 * Usage:
 *   WITH_MUTEX_LOCK(my_mutex) {
 *       // Critical section - mutex is held
 *       shared_variable++;
 *       if (condition) {
 *           return;  // Mutex is safely released
 *       }
 *   }  // Mutex released here
 *
 * Note: The body only executes if mutex is acquired. Do not use in situations
 * where you need to detect timeout - use manual acquire/release instead.
 */
#define WITH_MUTEX_LOCK(mutex) \
    for (bool _acquired = mutex_acquire_blocking(mutex); _acquired; mutex_release(mutex), (_acquired = false))

/**
 * @brief Guard scope with mutex - timeout-based acquisition
 *
 * Convenience macro for automatic mutex acquisition and release with timeout.
 * Mutex is held throughout the block and released on exit.
 *
 * @param mutex The SemaphoreHandle_t mutex to acquire
 * @param timeout_ms Timeout in milliseconds
 *
 * Usage:
 *   WITH_MUTEX_TIMEOUT(my_mutex, 100) {
 *       // Critical section - only executes if acquired within timeout
 *       shared_variable++;
 *   }  // Mutex released here
 *
 * Note: Body does not execute if timeout occurs.
 */
#define WITH_MUTEX_TIMEOUT(mutex, timeout_ms) \
    for (bool _acquired = mutex_acquire_timeout(mutex, timeout_ms); \
         _acquired; \
         mutex_release(mutex), (_acquired = false))

#endif  // MUTEX_HELPERS_H

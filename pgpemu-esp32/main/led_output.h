#ifndef LED_OUTPUT_H
#define LED_OUTPUT_H

#include <stdbool.h>

/**
 * Initialize the status LED on GPIO 8 (active-high) and start its 50 ms pattern timer.
 * The LED starts OFF. Call this during system initialization.
 * @param provisioning true when no secrets are installed
 * @param crashed true when the last reset was a panic or watchdog
 */
void init_led_output(bool provisioning, bool crashed);

/**
 * Enable or disable the status LED patterns. Disabled keeps the LED dark (stealth).
 * Safe to call before init_led_output.
 */
void led_output_set_enabled(bool enabled);

/**
 * Get current LED pin level.
 * @return true if LED is ON, false if LED is OFF
 */
bool get_led_advertising(void);

#endif /* LED_OUTPUT_H */

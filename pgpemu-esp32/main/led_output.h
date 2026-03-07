#ifndef LED_OUTPUT_H
#define LED_OUTPUT_H

#include <stdbool.h>

/**
 * Initialize LED output on GPIO 2 (active-high).
 * Sets GPIO 2 as output and initializes LED to ON (HIGH).
 * Call this during system initialization.
 * NOTE: Changed from GPIO 8 due to potential USB-Serial-JTAG conflict.
 */
void init_led_output(void);

/**
 * Set LED advertising state.
 * @param enabled true to turn LED ON (GPIO 2 = HIGH), false to turn LED OFF (GPIO 2 = LOW)
 */
void set_led_advertising(bool enabled);

/**
 * Get current LED advertising state.
 * @return true if LED is ON, false if LED is OFF
 */
bool get_led_advertising(void);

#endif /* LED_OUTPUT_H */

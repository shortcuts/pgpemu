#ifndef LED_OUTPUT_H
#define LED_OUTPUT_H

#include <stdbool.h>
void init_led_output();

// leds turn off after given duration. set duration to 0 to keep leds on until next event.
void show_rgb_event(bool red, bool green, bool blue, int duration_ms);

#endif /* LED_OUTPUT_H */

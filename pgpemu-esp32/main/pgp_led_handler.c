#include "esp_gatt_defs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "log_tags.h"
#include "pgp_autobutton.h"
#include "pgp_control.h"
#include "pgp_handshake_multi.h"
#include "settings.h"
#include "stats.h"

#include <stdint.h>
#include <stdlib.h>

// Helper macro to get device settings safely
#define GET_DEVICE_SETTINGS(conn_id)                              \
    ({                                                            \
        client_state_t* state = get_client_state_entry(conn_id);  \
        (state && state->has_settings) ? &state->settings : NULL; \
    })

void handle_led_notify_from_app(esp_gatt_if_t gatts_if, uint16_t conn_id, const uint8_t* buffer, uint16_t len) {
    if (len < 4) {
        ESP_LOGW(LEDHANDLER_TAG, "[%d] LED write too short: len=%d", conn_id, len);
        return;
    }
    int number_of_patterns = buffer[3] & 0x1f;
    if (len < 4 + 3 * number_of_patterns) {
        ESP_LOGW(LEDHANDLER_TAG, "[%d] LED write truncated: len=%d, patterns=%d", conn_id, len, number_of_patterns);
        return;
    }
    int priority = (buffer[3] >> 5) & 0x7;

    ESP_LOGD(LEDHANDLER_TAG, "[%d] LED: Pattern count=%d, priority=%d", conn_id, number_of_patterns, priority);

    // total duration / 50 ms
    int pattern_duration = 0;
    // number of shakes when catching
    int count_ballshake = 0;
    // how many times does each color occur in the pattern?
    int count_red = 0;
    int count_green = 0;
    int count_blue = 0;
    int count_yellow = 0;
    int count_white = 0;
    int count_off = 0, count_notoff = 0;

    // 1 pattern = 3 bytes
    for (int i = 0; i < number_of_patterns; i++) {
        int p = 4 + 3 * i;
        const uint8_t* pat = &buffer[p];

        uint8_t duration = pat[0];
        uint8_t red = pat[1] & 0xf;
        uint8_t green = (pat[1] >> 4) & 0xf;
        uint8_t blue = pat[2] & 0xf;
        bool interpolate = (pat[2] & 0x80) != 0;
        char inter_ch = interpolate ? 'i' : ' ';
        bool vibration = (pat[2] & 0x70) != 0;
        char vib_ch = vibration ? 'v' : ' ';
        ESP_LOGD(LEDHANDLER_TAG, "*(%3d) #%x%x%x %c%c", duration, red, green, blue, vib_ch, inter_ch);

        pattern_duration += duration;

        // parse colors roughly
        if (!red && !green && !blue) {
            count_off++;
        } else {
            count_notoff++;

            // special hack to detect blinking white at the start (beginning of catch animation)
            // this pattern is up to 3 times:
            // D (629037) PGPEMU: *(3) #888
            // D (629037) PGPEMU: *(9) #000
            // D (629047) PGPEMU: *(16) #000
            if (i <= 3 * 3) {
                if (red && green && blue) {
                    count_ballshake++;
                }
            }

            if (red && !green && !blue) {
                count_red++;
            } else if (!red && green && !blue) {
                count_green++;
            } else if (!red && !green && blue) {
                count_blue++;
            } else if (red && green && !blue) {
                count_yellow++;
            } else if (red && green && blue) {
                count_white++;
            }
        }
    }

    ESP_LOGD(LEDHANDLER_TAG, "[%d] LED pattern total duration: %d ms", conn_id, pattern_duration * 50);

    bool press_button = false;

    // Get device settings for this connection
    DeviceSettings* device_settings = GET_DEVICE_SETTINGS(conn_id);
    client_state_t* client = get_client_state_entry(conn_id);
    // Off and unrecognised patterns keep the last warning; every other pattern clears it.
    uint8_t warning = client ? client->warning : PGP_WARNING_NONE;

    if (count_off && !count_notoff) {
        ESP_LOGD(LEDHANDLER_TAG, "[%d] Turn LEDs off.", conn_id);
    } else if (count_white && count_white == count_notoff) {
        // only white - bag is full
        warning = PGP_WARNING_BAG_FULL;
        increment_bag_full(conn_id);
        pgp_control_emit_event(PGP_EVENT_BAG_FULL, conn_id);
        if (device_settings) {
            ESP_LOGW(LEDHANDLER_TAG, "[%d] Bag is full", conn_id);
        }
    } else if (count_red && count_off && count_red == count_notoff) {
        // blinking just red - pokeballs empty or stop out of range
        warning = PGP_WARNING_BALLS_EMPTY;
        increment_balls_empty(conn_id);
        pgp_control_emit_event(PGP_EVENT_BALLS_EMPTY, conn_id);
        if (device_settings) {
            ESP_LOGW(LEDHANDLER_TAG, "[%d] Pokeballs are empty or Pokestop went out of range", conn_id);
        }
    } else if (count_red && !count_off && count_red == count_notoff) {
        // only red - box is full
        warning = PGP_WARNING_BOX_FULL;
        increment_box_full(conn_id);
        pgp_control_emit_event(PGP_EVENT_BOX_FULL, conn_id);
        if (device_settings) {
            ESP_LOGW(LEDHANDLER_TAG, "[%d] Box is full", conn_id);
        }
    } else if (count_green && count_green == count_notoff) {
        // blinking green
        warning = PGP_WARNING_NONE;
        pgp_control_emit_event(PGP_EVENT_POKEMON_IN_RANGE, conn_id);
        if (device_settings) {
            ESP_LOGI(LEDHANDLER_TAG, "[%d] Pokemon in range", conn_id);
            if (device_settings->autocatch) {
                press_button = true;
            }
        }
    } else if (count_yellow && count_yellow == count_notoff) {
        // blinking yellow
        warning = PGP_WARNING_NONE;
        pgp_control_emit_event(PGP_EVENT_NEW_POKEMON_IN_RANGE, conn_id);
        if (device_settings) {
            ESP_LOGI(LEDHANDLER_TAG, "[%d] New pokemon in range", conn_id);
            if (device_settings->autocatch) {
                press_button = true;
            }
        }
    } else if (count_blue && count_blue == count_notoff) {
        // blinking blue - pokestop in range
        warning = PGP_WARNING_NONE;
        pgp_control_emit_event(PGP_EVENT_POKESTOP_IN_RANGE, conn_id);
        if (device_settings && device_settings->autospin) {
            ESP_LOGI(LEDHANDLER_TAG, "[%d] Pokestop in range: pressing button", conn_id);
            press_button = true;
        }
    } else if (count_ballshake) {
        warning = PGP_WARNING_NONE;
        if (count_blue && count_green) {
            increment_caught(conn_id);
            pgp_control_emit_event(PGP_EVENT_CAUGHT, conn_id);
            ESP_LOGI(LEDHANDLER_TAG, "[%d] Caught Pokemon after %d ball shakes.", conn_id, count_ballshake);
        } else if (count_red) {
            increment_fled(conn_id);
            pgp_control_emit_event(PGP_EVENT_FLED, conn_id);
            ESP_LOGW(LEDHANDLER_TAG, "[%d] Pokemon fled after %d ball shakes.", conn_id, count_ballshake);
        } else {
            ESP_LOGE(LEDHANDLER_TAG,
                "[%d] I don't know what the Pokemon did after %d ball shakes.",
                conn_id,
                count_ballshake);
        }
    } else if (count_red && count_green && count_blue && !count_off) {
        // blinking grb-grb...
        warning = PGP_WARNING_NONE;
        increment_spin(conn_id);
        pgp_control_emit_event(PGP_EVENT_ITEMS_RECEIVED, conn_id);
        ESP_LOGI(LEDHANDLER_TAG, "[%d] Got items from Pokestop.", conn_id);
    } else {
        if (device_settings && (device_settings->autospin || device_settings->autocatch)) {
            ESP_LOGE(LEDHANDLER_TAG, "[%d] Unhandled Color pattern, pushing button in any case", conn_id);
            press_button = true;
        } else {
            ESP_LOGE(LEDHANDLER_TAG, "[%d] Unhandled Color pattern", conn_id);
        }
    }

    if (client) {
        client->warning = warning;
    }

    if (press_button) {
        int pattern_ms = pattern_duration * 50;

        // random button press delay between 1000 and 2500 ms
        int delay = 1000 + esp_random() % 1501;
        if (delay < pattern_ms) {
            ESP_LOGD(LEDHANDLER_TAG, "[%d] queueing push button after %d ms", conn_id, delay);

            schedule_button_press(gatts_if, conn_id, delay);
        }
    }
}

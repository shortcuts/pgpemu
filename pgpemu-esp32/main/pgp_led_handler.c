#include "esp_gatt_defs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "led_output.h"
#include "log_tags.h"
#include "pgp_autobutton.h"
#include "settings.h"

static const int led_duration_ms = 200;

void handle_led_notify_from_app(esp_gatt_if_t gatts_if, uint16_t conn_id, const uint8_t* buffer) {
    int number_of_patterns = buffer[3] & 0x1f;
    int priority = (buffer[3] >> 5) & 0x7;

    ESP_LOGD(LEDHANDLER_TAG, "LED: Pattern count=%d, priority=%d", number_of_patterns, priority);

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
    int count_other = 0;  // colors like pink which aren't used
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
        ESP_LOGD(LEDHANDLER_TAG, "*(%3d) #%x%x%x %c%c", duration, red, green, blue, vib_ch,
                 inter_ch);

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
            } else {
                count_other++;
            }
        }
    }

    ESP_LOGI(LEDHANDLER_TAG,
             "LED pattern total duration: %d ms, conn_id=%d, Event:", pattern_duration * 50,
             conn_id);

    const bool show_interactions = get_setting(&settings.led_interactions);
    bool press_button = false;

    if (count_off && !count_notoff) {
        ESP_LOGD(LEDHANDLER_TAG, "Turn LEDs off.");
    } else if (count_white && count_white == count_notoff) {
        // only white
        ESP_LOGW(LEDHANDLER_TAG, "Can't spin Pokestop. Bag is full.");
        show_rgb_event(true, false, false, 3 * led_duration_ms);
    } else if (count_red && count_off && count_red == count_notoff) {
        // blinking just red
        ESP_LOGW(LEDHANDLER_TAG, "Pokeballs are empty or Pokestop went out of range.");
        show_rgb_event(true, false, false, 1 * led_duration_ms);
    } else if (count_red && !count_off && count_red == count_notoff) {
        // only red
        ESP_LOGW(LEDHANDLER_TAG, "Can't catch Pokemon. Box is full.");
        show_rgb_event(true, false, false, 3 * led_duration_ms);
    } else if (count_green && count_green == count_notoff) {
        // blinking green
        ESP_LOGI(LEDHANDLER_TAG, "Pokemon in range!");
        if (get_setting(&settings.autocatch)) {
            press_button = true;
        }
    } else if (count_yellow && count_yellow == count_notoff) {
        // blinking yellow
        ESP_LOGI(LEDHANDLER_TAG, "New Pokemon in range!");
        press_button = true;
    } else if (count_blue && count_blue == count_notoff) {
        // blinking blue
        ESP_LOGI(LEDHANDLER_TAG, "Pokestop in range!");
        if (get_setting(&settings.autospin)) {
            press_button = true;
        }
    } else if (count_ballshake) {
        if (count_blue && count_green) {
            if (show_interactions) {
                show_rgb_event(false, true, false, led_duration_ms);  // green
            }
            ESP_LOGI(LEDHANDLER_TAG, "Caught Pokemon after %d ball shakes.", count_ballshake);
        } else if (count_red) {
            if (show_interactions) {
                show_rgb_event(true, false, true, led_duration_ms);  // pink
            }
            ESP_LOGI(LEDHANDLER_TAG, "Pokemon fled after %d ball shakes.", count_ballshake);
        } else {
            ESP_LOGE(LEDHANDLER_TAG, "I don't know what the Pokemon did after %d ball shakes.",
                     count_ballshake);
        }
    } else if (count_red && count_green && count_blue && !count_off) {
        if (show_interactions) {
            show_rgb_event(false, false, true, led_duration_ms);  // blue
        }
        // blinking grb-grb...
        ESP_LOGI(LEDHANDLER_TAG, "Got items from Pokestop.");
    } else {
        if (get_setting(&settings.autospin) || get_setting(&settings.autocatch)) {
            ESP_LOGE(LEDHANDLER_TAG, "Unhandled Color pattern, pushing button in any case");
            press_button = true;
        } else {
            ESP_LOGE(LEDHANDLER_TAG, "Unhandled Color pattern");
        }
    }

    if (press_button) {
        int pattern_ms = pattern_duration * 50;

        // random button press delay between 1000 and 2500 ms
        int delay = 1000 + esp_random() % 1501;
        if (delay < pattern_ms) {
            ESP_LOGD(LEDHANDLER_TAG, "queueing push button after %d ms, conn_id=%d", delay,
                     conn_id);

            button_queue_item_t item;
            item.gatts_if = gatts_if;
            item.conn_id = conn_id;
            item.delay = delay;
            xQueueSend(button_queue, &item, portMAX_DELAY);
        }
    }
}
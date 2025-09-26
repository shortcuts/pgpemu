#include "esp_gatt_defs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "log_tags.h"
#include "pgp_autobutton.h"
#include "pgp_autosetting.h"
#include "settings.h"
#include "stats.h"
#include "uart.h"

const int retoggle_delay = 300000;

void handle_led_notify_from_app(esp_gatt_if_t gatts_if, uint16_t conn_id, const uint8_t* buffer) {
    int number_of_patterns = buffer[3] & 0x1f;
    int priority = (buffer[3] >> 5) & 0x7;

    ESP_LOGD(LEDHANDLER_TAG,
        "[%d] LED: Pattern count=%d, priority=%d",
        conn_id,
        number_of_patterns,
        priority);

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
        ESP_LOGD(
            LEDHANDLER_TAG, "*(%3d) #%x%x%x %c%c", duration, red, green, blue, vib_ch, inter_ch);

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

    ESP_LOGD(
        LEDHANDLER_TAG, "[%d] LED pattern total duration: %d ms", conn_id, pattern_duration * 50);

    bool press_button = false;
    char retoggle_setting = 0;

    if (count_off && !count_notoff) {
        ESP_LOGD(LEDHANDLER_TAG, "[%d] Turn LEDs off.", conn_id);
    } else if (count_white && count_white == count_notoff) {
        // only white
        process_char('s');
        retoggle_setting = 's';
        ESP_LOGW(LEDHANDLER_TAG,
            "[%d] Bag is full: press button %s",
            conn_id,
            get_setting_log_value(&settings.autospin));
    } else if (count_red && count_off && count_red == count_notoff) {
        // blinking just red
        process_char('c');
        retoggle_setting = 'c';
        ESP_LOGW(LEDHANDLER_TAG,
            "[%d] Pokeballs are empty or Pokestop went out of range: press button %s",
            conn_id,
            get_setting_log_value(&settings.autocatch));
    } else if (count_red && !count_off && count_red == count_notoff) {
        // only red
        process_char('c');
        retoggle_setting = 'c';
        ESP_LOGW(LEDHANDLER_TAG,
            "[%d] Box is full: press button %s",
            conn_id,
            get_setting_log_value(&settings.autocatch));
    } else if (count_green && count_green == count_notoff) {
        // blinking green
        ESP_LOGI(LEDHANDLER_TAG,
            "[%d] Pokemon in range: press button %s",
            conn_id,
            get_setting_log_value(&settings.autocatch));
        if (get_setting(&settings.autocatch)) {
            press_button = true;
        }
    } else if (count_yellow && count_yellow == count_notoff) {
        // blinking yellow
        ESP_LOGI(LEDHANDLER_TAG,
            "[%d] New pokemon in range: press button %s",
            conn_id,
            get_setting_log_value(&settings.autocatch));
        if (get_setting(&settings.autocatch)) {
            press_button = true;
        }
    } else if (count_blue && count_blue == count_notoff) {
        // blinking blue
        ESP_LOGI(LEDHANDLER_TAG,
            "[%d] Pokestop in range: press button %s",
            conn_id,
            get_setting_log_value(&settings.autospin));
        if (get_setting(&settings.autospin)) {
            press_button = true;
        }
    } else if (count_ballshake) {
        if (count_blue && count_green) {
            increment_caught(conn_id);
            ESP_LOGI(LEDHANDLER_TAG,
                "[%d] Caught Pokemon after %d ball shakes.",
                conn_id,
                count_ballshake);
        } else if (count_red) {
            increment_fled(conn_id);
            ESP_LOGW(LEDHANDLER_TAG,
                "[%d] Pokemon fled after %d ball shakes.",
                conn_id,
                count_ballshake);
        } else {
            ESP_LOGE(LEDHANDLER_TAG,
                "[%d] I don't know what the Pokemon did after %d ball shakes.",
                conn_id,
                count_ballshake);
        }
    } else if (count_red && count_green && count_blue && !count_off) {
        // blinking grb-grb...
        increment_spin(conn_id);
        ESP_LOGI(LEDHANDLER_TAG, "[%d] Got items from Pokestop.", conn_id);
    } else {
        if (get_setting(&settings.autospin) || get_setting(&settings.autocatch)) {
            ESP_LOGE(LEDHANDLER_TAG,
                "[%d] Unhandled Color pattern, pushing button in any case",
                conn_id);
            press_button = true;
        } else {
            ESP_LOGE(LEDHANDLER_TAG, "[%d] Unhandled Color pattern", conn_id);
        }
    }

    if (press_button) {
        int pattern_ms = pattern_duration * 50;

        // random button press delay between 1000 and 2500 ms
        int delay = 1000 + esp_random() % 1501;
        if (delay < pattern_ms) {
            ESP_LOGD(LEDHANDLER_TAG, "[%d] queueing push button after %d ms", conn_id, delay);

            button_queue_item_t item;
            item.gatts_if = gatts_if;
            item.conn_id = conn_id;
            item.delay = delay;
            xQueueSend(button_queue, &item, portMAX_DELAY);
        }
    }

    if (retoggle_setting != 0) {
        ESP_LOGD(LEDHANDLER_TAG,
            "[%d] queueing setting toggle for %c after %d ms",
            conn_id,
            retoggle_setting,
            retoggle_delay);
        setting_queue_item_t item = {
            .gatts_if = gatts_if,
            .conn_id = conn_id,
            .delay = retoggle_delay,
            .setting = retoggle_setting,
        };
        xQueueSend(setting_queue, &item, portMAX_DELAY);
    }
}

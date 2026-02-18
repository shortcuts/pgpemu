#include "pgp_autobutton.h"

#include "esp_bt.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "log_tags.h"
#include "pgp_gatts.h"
#include "pgp_handshake_multi.h"

QueueHandle_t button_queue;

static void autobutton_task(void* pvParameters);

bool init_autobutton() {
    button_queue = xQueueCreate(10, sizeof(button_queue_item_t));
    if (!button_queue) {
        ESP_LOGE(BUTTON_TASK_TAG, "%s creating button queue failed", __func__);
        return false;
    }

    BaseType_t ret = xTaskCreate(autobutton_task, "autobutton_task", 3072, NULL, 11, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(BUTTON_TASK_TAG, "%s creating task failed", __func__);
        vQueueDelete(button_queue);
        return false;
    }

    return true;
}

static void autobutton_task(void* __attribute__((unused)) pvParameters) {
    button_queue_item_t item;

    ESP_LOGI(BUTTON_TASK_TAG, "task start");

    while (1) {
        if (xQueueReceive(button_queue, &item, portMAX_DELAY)) {
            // according to u/EeveesGalore's docs (https://i.imgur.com/7oWjMNu.png) button is
            // sampled every 50 ms byte 0 = samples0,1 (2=LSBit) byte 1 = samples2-9 (10=LSBit)
            // randomize at which sample the button press starts and ends (min. diff 200 ms)
            int press_start = esp_random() % 6;  // start at sample 0-5
            int press_last = press_start + 4 + esp_random() % (10 - press_start - 4);
            //               ^--min value--^                  ^-min distance to 10-^
            int press_duration = press_last - press_start + 1;

            // set bits where the button is pressed
            uint16_t button_pattern = 0;
            for (int i = 0; i < 10; i++) {
                button_pattern <<= 1;  // this gets shifted 10 times total
                if (i >= press_start && i <= press_last) {
                    button_pattern |= 1;  // button is pressed
                }
            }
            button_pattern &= 0x03ff;  // just to be safe

            // make little endian byte array for sending
            uint8_t notify_data[2] = { (button_pattern >> 8) & 0x03, button_pattern & 0xff };

            ESP_LOGD(BUTTON_TASK_TAG,
                "[%d] pressing button delay=%d ms, duration=%d ms",
                item.conn_id,
                item.delay,
                press_duration * 50);
            vTaskDelay(item.delay / portTICK_PERIOD_MS);

            if (!is_connection_active(item.conn_id)) {
                ESP_LOGW(BUTTON_TASK_TAG, "Connection %d no longer active, skipping button press", item.conn_id);
                continue;
            }

            esp_ble_gatts_send_indicate(item.gatts_if,
                item.conn_id,
                led_button_handle_table[IDX_CHAR_BUTTON_VAL],
                sizeof(notify_data),
                notify_data,
                false);
        }
    }

    vTaskDelete(NULL);
}

void purge_button_queue_for_connection(uint16_t conn_id) {
    button_queue_item_t temp_item;
    button_queue_item_t items_to_keep[10];
    int keep_count = 0;

    while (xQueueReceive(button_queue, &temp_item, 0) == pdTRUE) {
        if (temp_item.conn_id != conn_id) {
            if (keep_count < 10) {
                items_to_keep[keep_count++] = temp_item;
            }
        }
    }

    for (int i = 0; i < keep_count; i++) {
        xQueueSendToBack(button_queue, &items_to_keep[i], 0);
    }

    ESP_LOGI(BUTTON_TASK_TAG, "Purged button queue for connection %d", conn_id);
}

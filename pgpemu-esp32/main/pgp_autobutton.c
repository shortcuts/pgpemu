#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gatts_api.h"

#include "pgp_autobutton.h"

#include "log_tags.h"
#include "pgp_gatts.h"

QueueHandle_t button_queue;

static void autobutton_task(void *pvParameters);

bool init_autobutton()
{
    button_queue = xQueueCreate(10, sizeof(button_queue_item_t));
    if (button_queue == 0)
    {
        ESP_LOGE(BUTTON_TASK_TAG, "%s creating button queue failed", __func__);
        return false;
    }

    xTaskCreate(autobutton_task, "autobutton_task", 3072, NULL, 11, NULL);

    return true;
}

static void autobutton_task(void *pvParameters)
{
    button_queue_item_t item;

    ESP_LOGI(BUTTON_TASK_TAG, "task start");

    while (1)
    {
        if (xQueueReceive(button_queue, &item, portMAX_DELAY))
        {
            // according to u/EeveesGalore's docs (https://i.imgur.com/7oWjMNu.png) button is sampled every 50 ms
            // byte 0 = samples0,1 (2=LSBit)
            // byte 1 = samples2-9 (10=LSBit)
            // randomize at which sample the button press starts and ends (min. diff 200 ms)
            int press_start = esp_random() % 6; // start at sample 0-5
            int press_last = press_start + 4 + esp_random() % (10 - press_start - 4);
            //               ^--min value--^                  ^-min distance to 10-^
            int press_duration = press_last - press_start + 1;

            // set bits where the button is pressed
            uint16_t button_pattern = 0;
            for (int i = 0; i < 10; i++)
            {
                button_pattern <<= 1; // this gets shifted 10 times total
                if (i >= press_start && i <= press_last)
                {
                    button_pattern |= 1; // button is pressed
                }
            }
            button_pattern &= 0x03ff; // just to be safe

            // make little endian byte array for sending
            uint8_t notify_data[2] = {
                (button_pattern >> 8) & 0x03,
                button_pattern & 0xff};

            ESP_LOGI(BUTTON_TASK_TAG, "pressing button delay=%d ms, duration=%d ms, conn_id=%d", item.delay, press_duration * 50, item.conn_id);
            vTaskDelay(item.delay / portTICK_PERIOD_MS);

            esp_ble_gatts_send_indicate(item.gatts_if,
                                        item.conn_id,
                                        led_button_handle_table[IDX_CHAR_BUTTON_VAL],
                                        sizeof(notify_data), notify_data, false);
        }
    }

    vTaskDelete(NULL);
}
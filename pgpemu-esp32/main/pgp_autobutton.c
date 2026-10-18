#include "pgp_autobutton.h"

#include "esp_bt.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "log_tags.h"
#include "pgp_gatts.h"
#include "pgp_handshake_multi.h"

#define BUTTON_SLOTS 10

// One timer per pending press, so a press never waits behind another connection's delay.
typedef struct {
    bool in_use;
    esp_gatt_if_t gatts_if;
    uint16_t conn_id;
    int64_t due_us;
    esp_timer_handle_t timer;
} button_slot_t;

static button_slot_t slots[BUTTON_SLOTS];
static portMUX_TYPE slots_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t dropped_presses;

static void press_button_cb(void* arg);

bool init_autobutton() {
    for (int i = 0; i < BUTTON_SLOTS; i++) {
        esp_timer_create_args_t args = {
            .callback = press_button_cb,
            .arg = (void*)(intptr_t)i,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "autobutton",
        };
        if (esp_timer_create(&args, &slots[i].timer) != ESP_OK) {
            ESP_LOGE(BUTTON_TASK_TAG, "%s creating timer %d failed", __func__, i);
            while (--i >= 0) {
                esp_timer_delete(slots[i].timer);
            }
            return false;
        }
    }
    return true;
}

bool schedule_button_press(esp_gatt_if_t gatts_if, uint16_t conn_id, int delay_ms) {
    int64_t due_us = esp_timer_get_time() + (int64_t)delay_ms * 1000;
    int i;

    portENTER_CRITICAL(&slots_lock);
    for (i = 0; i < BUTTON_SLOTS && slots[i].in_use; i++) {}
    if (i < BUTTON_SLOTS) {
        slots[i].in_use = true;
        slots[i].gatts_if = gatts_if;
        slots[i].conn_id = conn_id;
        slots[i].due_us = due_us;
    }
    portEXIT_CRITICAL(&slots_lock);

    // esp_timer_* stays outside the lock
    if (i < BUTTON_SLOTS && esp_timer_start_once(slots[i].timer, (uint64_t)delay_ms * 1000) != ESP_OK) {
        portENTER_CRITICAL(&slots_lock);
        slots[i].in_use = false;
        portEXIT_CRITICAL(&slots_lock);
        i = BUTTON_SLOTS;
    }
    if (i == BUTTON_SLOTS) {
        dropped_presses++;
        ESP_LOGW(BUTTON_TASK_TAG, "[%d] press dropped, %lu total", conn_id, (unsigned long)dropped_presses);
        return false;
    }
    return true;
}

static void press_button_cb(void* arg) {
    button_slot_t* slot = &slots[(intptr_t)arg];
    esp_gatt_if_t gatts_if;
    uint16_t conn_id;

    portENTER_CRITICAL(&slots_lock);
    // a callback that raced with purge and slot reuse sees the new, later due time and does nothing
    bool stale = !slot->in_use || esp_timer_get_time() < slot->due_us;
    gatts_if = slot->gatts_if;
    conn_id = slot->conn_id;
    if (!stale) {
        slot->in_use = false;
    }
    portEXIT_CRITICAL(&slots_lock);
    if (stale) {
        return;
    }

    if (!is_connection_active(conn_id)) {
        ESP_LOGW(BUTTON_TASK_TAG, "Connection %d no longer active, skipping button press", conn_id);
        return;
    }

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

    ESP_LOGD(BUTTON_TASK_TAG, "[%d] pressing button, duration=%d ms", conn_id, press_duration * 50);
    esp_ble_gatts_send_indicate(
        gatts_if, conn_id, led_button_handle_table[IDX_CHAR_BUTTON_VAL], sizeof(notify_data), notify_data, false);
}

void purge_button_queue_for_connection(uint16_t conn_id) {
    for (int i = 0; i < BUTTON_SLOTS; i++) {
        bool hit;
        portENTER_CRITICAL(&slots_lock);
        hit = slots[i].in_use && slots[i].conn_id == conn_id;
        if (hit) {
            slots[i].in_use = false;
        }
        portEXIT_CRITICAL(&slots_lock);
        if (hit) {
            esp_timer_stop(slots[i].timer);  // may already have fired
        }
    }

    ESP_LOGI(BUTTON_TASK_TAG, "Purged button queue for connection %d", conn_id);
}

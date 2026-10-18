// Unit tests for autobutton press scheduling (PC build)
// Each press fires at its own deadline; mocked esp_timer drives the real pgp_autobutton.c
#ifndef ESP_PLATFORM

#include "../pgp_autobutton.c"

#include <assert.h>

// ---- mock esp_timer: a table of timers, run in due-time order by advance_to() ----
#define MAX_TIMERS 16
typedef struct {
    esp_timer_cb_t cb;
    void* arg;
    bool armed;
    int64_t due;
} mock_timer_t;
static mock_timer_t timers[MAX_TIMERS];
static int timer_count, start_fail;
static int64_t now_us;

esp_err_t esp_timer_create(const esp_timer_create_args_t* a, esp_timer_handle_t* out) {
    timers[timer_count].cb = a->callback;
    timers[timer_count].arg = a->arg;
    *out = &timers[timer_count++];
    return ESP_OK;
}
esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t us) {
    if (start_fail) {
        return ESP_FAIL;
    }
    ((mock_timer_t*)t)->armed = true;
    ((mock_timer_t*)t)->due = now_us + (int64_t)us;
    return ESP_OK;
}
esp_err_t esp_timer_stop(esp_timer_handle_t t) {
    ((mock_timer_t*)t)->armed = false;
    return ESP_OK;
}
esp_err_t esp_timer_delete(esp_timer_handle_t t) {
    (void)t;
    return ESP_OK;
}
int64_t esp_timer_get_time(void) {
    return now_us;
}

static void advance_to(int64_t t_us) {
    for (;;) {
        int best = -1;
        for (int i = 0; i < timer_count; i++) {
            if (timers[i].armed && timers[i].due <= t_us && (best < 0 || timers[i].due < timers[best].due)) {
                best = i;
            }
        }
        if (best < 0) {
            break;
        }
        now_us = timers[best].due;
        timers[best].armed = false;
        timers[best].cb(timers[best].arg);
    }
    now_us = t_us;
}

// ---- mocks of what the callback calls ----
uint16_t led_button_handle_table[LED_BUTTON_LAST_IDX];
static int dead_conn = -1;
bool is_connection_active(uint16_t conn_id) {
    return (int)conn_id != dead_conn;
}
static int fired[16], fired_n;
static int64_t fired_at[16];
int esp_ble_gatts_send_indicate(esp_gatt_if_t gatts_if,
    uint16_t conn_id,
    uint16_t handle,
    uint16_t len,
    uint8_t* value,
    bool need_confirm) {
    (void)gatts_if, (void)handle, (void)len, (void)value, (void)need_confirm;
    fired[fired_n] = conn_id;
    fired_at[fired_n++] = now_us;
    return 0;
}

#define MS 1000
static void reset(void) {
    advance_to(now_us + 10000 * MS);  // drain
    fired_n = 0;
    dead_conn = -1;
    start_fail = 0;
}

static void test_presses_fire_at_own_deadlines(void) {
    assert(schedule_button_press(0, 0, 2500));
    assert(schedule_button_press(0, 1, 1000));
    int64_t t0 = now_us;
    advance_to(t0 + 3000 * MS);
    assert(fired_n == 2);
    assert(fired[0] == 1 && fired_at[0] == t0 + 1000 * MS);
    assert(fired[1] == 0 && fired_at[1] == t0 + 2500 * MS);
}

static void test_full_table_drops_press(void) {
    for (int i = 0; i < BUTTON_SLOTS; i++) {
        assert(schedule_button_press(0, 0, 1000));
    }
    assert(!schedule_button_press(0, 0, 1000));
    advance_to(now_us + 1000 * MS);
    assert(fired_n == BUTTON_SLOTS);
}

static void test_purge_cancels_only_that_connection(void) {
    assert(schedule_button_press(0, 0, 1000));
    assert(schedule_button_press(0, 1, 1000));
    purge_button_queue_for_connection(0);
    advance_to(now_us + 1000 * MS);
    assert(fired_n == 1 && fired[0] == 1);
    for (int i = 0; i < BUTTON_SLOTS; i++) {
        assert(schedule_button_press(0, 0, 1000));  // purged slot is reusable
    }
}

static void test_stale_callback_after_reuse_is_noop(void) {
    assert(schedule_button_press(0, 0, 1000));
    esp_timer_cb_t cb = timers[0].cb;
    void* arg = timers[0].arg;
    purge_button_queue_for_connection(0);
    assert(schedule_button_press(0, 1, 2000));  // reuses slot 0
    cb(arg);                                    // old timer fires late, before the new deadline
    assert(fired_n == 0);
    advance_to(now_us + 2000 * MS);
    assert(fired_n == 1 && fired[0] == 1);
}

static void test_inactive_connection_skipped(void) {
    assert(schedule_button_press(0, 3, 1000));
    dead_conn = 3;
    advance_to(now_us + 1000 * MS);
    assert(fired_n == 0);
}

static void test_timer_start_failure(void) {
    start_fail = 1;
    for (int i = 0; i < BUTTON_SLOTS + 1; i++) {
        assert(!schedule_button_press(0, 0, 1000));  // never leaks a slot
    }
    start_fail = 0;
    assert(schedule_button_press(0, 0, 1000));
}

int main(void) {
    assert(init_autobutton());
    assert(timer_count == BUTTON_SLOTS);
    test_presses_fire_at_own_deadlines();
    reset();
    test_full_table_drops_press();
    reset();
    test_purge_cancels_only_that_connection();
    reset();
    test_stale_callback_after_reuse_is_noop();
    reset();
    test_inactive_connection_skipped();
    reset();
    test_timer_start_failure();
    printf("test_autobutton: all passed\n");
    return 0;
}

#endif

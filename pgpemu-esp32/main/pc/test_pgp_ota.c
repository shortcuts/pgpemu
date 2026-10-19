// Unit tests for the real pgp_ota.c: arm window, session state machine, validation (PC build)
#ifndef ESP_PLATFORM

#include "../log_tags.h"
#include "esp_timer.h"

#include <assert.h>
#include <stdio.h>

static int64_t now_us;
static esp_timer_cb_t timer_cb;
static int timer_started;
int64_t esp_timer_get_time(void) {
    return now_us;
}
esp_err_t esp_timer_create(const esp_timer_create_args_t* args, esp_timer_handle_t* out) {
    timer_cb = args->callback;
    *out = (void*)1;
    return ESP_OK;
}
esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t us) {
    (void)t;
    timer_started = (int)(us / 1000000);
    return ESP_OK;
}

#include "../pgp_ota.c"

#define IMAGE 1000
static const uint8_t blob[IMAGE];

static void reset(void) {
    pgp_ota_abort(1);
    ota_begin_calls = ota_write_calls = ota_end_calls = ota_abort_calls = ota_boot_calls = 0;
    ota_written = 0;
    ota_write_result = ota_end_result = ESP_OK;
    strcpy(ota_stub_project, "pgpemu");
    now_us = 10000000;
    armed_until_ms = 0;
}

// Arms and opens a session of `total` bytes for conn 1.
static void begin(uint32_t total) {
    uint16_t chunk;
    pgp_ota_arm();
    assert(pgp_ota_begin(1, total, 517, &chunk) == CONTROL_STATUS_OK);
}

static void test_not_armed_rejects_begin(void) {
    reset();
    uint16_t chunk;
    assert(pgp_ota_begin(1, IMAGE, 517, &chunk) == CONTROL_STATUS_ERR_OTA_NOT_ARMED);
    assert(ota_begin_calls == 0);
}

static void test_arm_expires_after_120_s(void) {
    reset();
    pgp_ota_arm();
    assert(pgp_ota_is_armed());
    now_us += 119 * 1000000LL;
    assert(pgp_ota_is_armed());
    now_us += 2 * 1000000LL;
    assert(!pgp_ota_is_armed());
}

static void test_begin_clears_arm_and_sizes_chunk(void) {
    reset();
    uint16_t chunk = 0;
    pgp_ota_arm();
    assert(pgp_ota_begin(1, IMAGE, 517, &chunk) == CONTROL_STATUS_OK);
    assert(chunk == 480 && !pgp_ota_is_armed() && ota_begin_calls == 1);
    pgp_ota_abort(1);
    pgp_ota_arm();
    assert(pgp_ota_begin(1, IMAGE, 100, &chunk) == CONTROL_STATUS_OK && chunk == 92);
}

static void test_begin_rejects_bad_input(void) {
    reset();
    uint16_t chunk;
    pgp_ota_arm();
    assert(pgp_ota_begin(1, IMAGE, 23, &chunk) == CONTROL_STATUS_ERR_OTA_STATE);
    assert(pgp_ota_begin(1, 0, 517, &chunk) == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);
    assert(pgp_ota_begin(1, 0x1c0001, 517, &chunk) == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD);
    assert(ota_begin_calls == 0 && pgp_ota_is_armed());
}

static void test_second_begin_while_receiving_is_rejected(void) {
    reset();
    begin(IMAGE);
    uint16_t chunk;
    pgp_ota_arm();
    assert(pgp_ota_begin(2, IMAGE, 517, &chunk) == CONTROL_STATUS_ERR_OTA_STATE && ota_begin_calls == 1);
}

static void test_data_checks_state_conn_and_offset(void) {
    reset();
    uint32_t next = 0;
    assert(pgp_ota_data(1, 0, blob, 10, &next) == CONTROL_STATUS_ERR_OTA_STATE);  // idle
    begin(IMAGE);
    assert(pgp_ota_data(2, 0, blob, 10, &next) == CONTROL_STATUS_ERR_OTA_STATE);         // other link
    assert(pgp_ota_data(1, 5, blob, 10, &next) == CONTROL_STATUS_ERR_OTA_STATE);         // gap
    assert(pgp_ota_data(1, 0, blob, IMAGE + 1, &next) == CONTROL_STATUS_ERR_OTA_STATE);  // past the end
    assert(ota_write_calls == 0);
    assert(pgp_ota_data(1, 0, blob, 400, &next) == CONTROL_STATUS_OK && next == 400);
    assert(pgp_ota_data(1, 0, blob, 400, &next) == CONTROL_STATUS_ERR_OTA_STATE);  // repeated chunk
    assert(pgp_ota_data(1, 400, blob, 600, &next) == CONTROL_STATUS_OK && next == IMAGE);
    assert(ota_written == IMAGE);
}

static void test_end_needs_every_byte(void) {
    reset();
    uint32_t next;
    begin(IMAGE);
    pgp_ota_data(1, 0, blob, 500, &next);
    assert(pgp_ota_end(1) == CONTROL_STATUS_ERR_OTA_STATE && ota_end_calls == 0);
    pgp_ota_data(1, 500, blob, 500, &next);
    assert(pgp_ota_end(2) == CONTROL_STATUS_ERR_OTA_STATE);
    assert(pgp_ota_end(1) == CONTROL_STATUS_OK);
    assert(ota_end_calls == 1 && ota_boot_calls == 1);
    assert(pgp_ota_end(1) == CONTROL_STATUS_ERR_OTA_STATE && ota_boot_calls == 1);  // back to idle
}

static void test_wrong_project_name_keeps_boot_partition(void) {
    reset();
    uint32_t next;
    begin(IMAGE);
    pgp_ota_data(1, 0, blob, IMAGE, &next);
    strcpy(ota_stub_project, "other");
    assert(pgp_ota_end(1) == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD && ota_boot_calls == 0);
}

static void test_failed_end_keeps_boot_partition(void) {
    reset();
    uint32_t next;
    begin(IMAGE);
    pgp_ota_data(1, 0, blob, IMAGE, &next);
    ota_end_result = ESP_FAIL;
    assert(pgp_ota_end(1) == CONTROL_STATUS_ERR_MALFORMED_PAYLOAD && ota_boot_calls == 0);
    assert(ota_abort_calls == 0);  // esp_ota_end already released the handle
}

static void test_failed_write_aborts(void) {
    reset();
    uint32_t next;
    begin(IMAGE);
    ota_write_result = ESP_FAIL;
    assert(pgp_ota_data(1, 0, blob, 10, &next) == CONTROL_STATUS_ERR_INTERNAL);
    assert(ota_abort_calls == 1 && pgp_ota_data(1, 0, blob, 10, &next) == CONTROL_STATUS_ERR_OTA_STATE);
}

static void test_abort_and_owner_disconnect_return_to_idle(void) {
    reset();
    uint32_t next;
    begin(IMAGE);
    pgp_ota_on_disconnect(2);  // not the owner
    assert(pgp_ota_abort(2) == CONTROL_STATUS_ERR_OTA_STATE && ota_abort_calls == 0);
    assert(pgp_ota_data(1, 0, blob, 10, &next) == CONTROL_STATUS_OK);
    pgp_ota_on_disconnect(1);
    assert(ota_abort_calls == 1 && pgp_ota_data(1, 10, blob, 10, &next) == CONTROL_STATUS_ERR_OTA_STATE);
    begin(IMAGE);
    assert(pgp_ota_abort(1) == CONTROL_STATUS_OK && ota_abort_calls == 2);
    assert(pgp_ota_abort(1) == CONTROL_STATUS_OK && ota_abort_calls == 2);  // idle: nothing to abort
}

static void test_confirm_timer(void) {
    reset();
    pgp_ota_init();
    assert(timer_started == 60 && timer_cb != NULL);
    ota_valid_result = ESP_ERR_NOT_FOUND;
    timer_cb(NULL);
    assert(ota_valid_calls == 1);
}

int main(void) {
    test_not_armed_rejects_begin();
    test_arm_expires_after_120_s();
    test_begin_clears_arm_and_sizes_chunk();
    test_begin_rejects_bad_input();
    test_second_begin_while_receiving_is_rejected();
    test_data_checks_state_conn_and_offset();
    test_end_needs_every_byte();
    test_wrong_project_name_keeps_boot_partition();
    test_failed_end_keeps_boot_partition();
    test_failed_write_aborts();
    test_abort_and_owner_disconnect_return_to_idle();
    test_confirm_timer();
    printf("test_pgp_ota: all passed\n");
    return 0;
}

#endif

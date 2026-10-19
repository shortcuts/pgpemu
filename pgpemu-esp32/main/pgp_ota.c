#include "pgp_ota.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "log_tags.h"

#include <string.h>

#define OTA_ARM_WINDOW_MS 120000
#define OTA_CONFIRM_DELAY_US (60 * 1000000LL)
// ATT header (3) + OTA_DATA offset field (4) + opcode (1) are the per-write overhead.
#define OTA_WRITE_OVERHEAD (3 + 5)
// Keeps opcode + offset + chunk within MAX_VALUE_LENGTH (500).
#define OTA_MAX_CHUNK 480
#define OTA_MIN_CHUNK 64
// CMake project(pgpemu); a foreign ESP32-C3 image passes esp_ota_end but fails this check.
#define OTA_PROJECT_NAME "pgpemu"

typedef enum { OTA_IDLE, OTA_RECEIVING } ota_state_t;

static struct {
    ota_state_t state;
    uint16_t conn_id;
    uint32_t total, offset;
    esp_ota_handle_t handle;
    const esp_partition_t* partition;
} session;

// Written by the button task, read by the BTC task: a 32-bit store is atomic on RISC-V, an int64_t is not.
static volatile uint32_t armed_until_ms;

static uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void pgp_ota_arm(void) {
    armed_until_ms = now_ms() + OTA_ARM_WINDOW_MS;
}

// The signed difference survives the 32-bit wrap.
bool pgp_ota_is_armed(void) {
    return (int32_t)(armed_until_ms - now_ms()) > 0;
}

static void confirm_image(void* arg) {
    (void)arg;
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    // NOT_FOUND: the image was flashed over USB, so there is nothing to confirm.
    if (err == ESP_OK) {
        ESP_LOGI(OTA_TAG, "image confirmed");
    } else if (err != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(OTA_TAG, "confirm failed: %s", esp_err_to_name(err));
    }
}

void pgp_ota_init(void) {
    static esp_timer_handle_t timer;
    const esp_timer_create_args_t args = { .callback = confirm_image, .name = "ota_confirm" };
    if (esp_timer_create(&args, &timer) != ESP_OK || esp_timer_start_once(timer, OTA_CONFIRM_DELAY_US) != ESP_OK) {
        ESP_LOGE(OTA_TAG, "confirm timer failed");
        return;
    }
    ESP_LOGI(OTA_TAG, "confirm timer started (60 s)");
}

static void end_session(bool abort_write) {
    if (abort_write && session.state == OTA_RECEIVING) {
        esp_ota_abort(session.handle);
    }
    session.state = OTA_IDLE;
}

control_status_t pgp_ota_begin(uint16_t conn_id, uint32_t total, uint16_t mtu, uint16_t* chunk_out) {
    if (!pgp_ota_is_armed()) {
        return CONTROL_STATUS_ERR_OTA_NOT_ARMED;
    }
    if (session.state != OTA_IDLE) {
        return CONTROL_STATUS_ERR_OTA_STATE;
    }
    // A link stuck at the default MTU 23 would need tiny chunks: fail instead of crawling.
    int chunk = (int)mtu - OTA_WRITE_OVERHEAD;
    if (chunk > OTA_MAX_CHUNK) {
        chunk = OTA_MAX_CHUNK;
    }
    if (chunk < OTA_MIN_CHUNK) {
        return CONTROL_STATUS_ERR_OTA_STATE;
    }
    const esp_partition_t* part = esp_ota_get_next_update_partition(NULL);
    if (part == NULL) {
        return CONTROL_STATUS_ERR_INTERNAL;
    }
    if (total == 0 || total > part->size) {
        return CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
    }
    // Sequential mode erases per sector, so this call does not stall BTC_TASK past the supervision timeout.
    if (esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &session.handle) != ESP_OK) {
        return CONTROL_STATUS_ERR_INTERNAL;
    }
    session.state = OTA_RECEIVING;
    session.conn_id = conn_id;
    session.total = total;
    session.offset = 0;
    session.partition = part;
    armed_until_ms = now_ms();  // one arm, one update
    *chunk_out = (uint16_t)chunk;
    ESP_LOGI(OTA_TAG, "begin size=%u chunk=%d", (unsigned)total, chunk);
    return CONTROL_STATUS_OK;
}

control_status_t pgp_ota_data(uint16_t conn_id, uint32_t offset, const uint8_t* data, size_t len, uint32_t* next_out) {
    if (session.state != OTA_RECEIVING || session.conn_id != conn_id || offset != session.offset || len == 0
        || len > session.total - session.offset) {
        return CONTROL_STATUS_ERR_OTA_STATE;
    }
    if (esp_ota_write(session.handle, data, len) != ESP_OK) {
        end_session(true);
        return CONTROL_STATUS_ERR_INTERNAL;
    }
    session.offset += (uint32_t)len;
    *next_out = session.offset;
    return CONTROL_STATUS_OK;
}

control_status_t pgp_ota_end(uint16_t conn_id) {
    if (session.state != OTA_RECEIVING || session.conn_id != conn_id || session.offset != session.total) {
        return CONTROL_STATUS_ERR_OTA_STATE;
    }
    // esp_ota_end releases the handle on success and on failure, so no abort follows.
    session.state = OTA_IDLE;
    if (esp_ota_end(session.handle) != ESP_OK) {
        ESP_LOGW(OTA_TAG, "image rejected by esp_ota_end");
        return CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
    }
    esp_app_desc_t desc;
    if (esp_ota_get_partition_description(session.partition, &desc) != ESP_OK
        || strncmp(desc.project_name, OTA_PROJECT_NAME, sizeof(desc.project_name)) != 0) {
        ESP_LOGW(OTA_TAG, "image is not a pgpemu build");
        return CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
    }
    if (esp_ota_set_boot_partition(session.partition) != ESP_OK) {
        return CONTROL_STATUS_ERR_INTERNAL;
    }
    ESP_LOGI(OTA_TAG, "done, boot slot switched");
    return CONTROL_STATUS_OK;
}

control_status_t pgp_ota_abort(uint16_t conn_id) {
    if (session.state == OTA_RECEIVING && session.conn_id != conn_id) {
        return CONTROL_STATUS_ERR_OTA_STATE;
    }
    end_session(true);
    return CONTROL_STATUS_OK;
}

void pgp_ota_on_disconnect(uint16_t conn_id) {
    if (session.state == OTA_RECEIVING && session.conn_id == conn_id) {
        ESP_LOGW(OTA_TAG, "owner disconnected, aborting");
        end_session(true);
    }
}

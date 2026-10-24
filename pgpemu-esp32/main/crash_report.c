#include "crash_report.h"

#include "esp_core_dump.h"
#include "esp_log.h"
#include "log_tags.h"

#include <stdlib.h>
#include <string.h>

// ESP32-C3 code ranges (SOC_IROM_*, SOC_IRAM_*): a stack word inside one may be a return address.
#define FLASH_CODE_LOW 0x42000000u
#define FLASH_CODE_HIGH 0x42800000u
#define IRAM_CODE_LOW 0x4037C000u
#define IRAM_CODE_HIGH 0x403E0000u

typedef struct {
    uint32_t mcause, mtval, pc, ra, sp;
    char task[CRASH_REPORT_TASK_LEN];
    uint8_t elf_sha[CRASH_REPORT_SHA_LEN];
    uint8_t pc_count;
    uint32_t pcs[CRASH_REPORT_MAX_PCS];
} crash_report_t;

// No mutex: init runs before Bluetooth starts, the Control Service handlers all run on BTC_TASK.
static crash_report_t report;
static bool present;

static bool in_code(uint32_t w) {
    return (w >= FLASH_CODE_LOW && w < FLASH_CODE_HIGH) || (w >= IRAM_CODE_LOW && w < IRAM_CODE_HIGH);
}

// The device cannot walk frames without DWARF, so keep the code addresses the host resolves with addr2line.
static uint8_t scan_stack(const uint8_t* stack, size_t len, uint32_t* pcs) {
    uint8_t n = 0;
    for (size_t i = 0; i + 4 <= len && n < CRASH_REPORT_MAX_PCS; i += 4) {
        uint32_t w;
        memcpy(&w, stack + i, 4);
        if (in_code(w)) {
            pcs[n++] = w;
        }
    }
    return n;
}

static void put_le32(uint8_t* out, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        out[i] = (uint8_t)(v >> (8 * i));
    }
}

void crash_report_init(void) {
    memset(&report, 0, sizeof(report));
    present = false;

    esp_err_t err = esp_core_dump_image_check();
    if (err == ESP_ERR_NOT_FOUND) {
        return;
    }
    if (err == ESP_ERR_INVALID_CRC || err == ESP_ERR_INVALID_SIZE) {
        // A dump cut short by a power loss holds nothing useful and would be re-checked on every boot.
        ESP_LOGW(CRASH_TAG, "core dump invalid (%d), erasing", err);
        esp_core_dump_image_erase();
        return;
    }
    if (err != ESP_OK) {
        ESP_LOGW(CRASH_TAG, "core dump check failed (%d)", err);
        return;
    }

    // Heap, not stack: the summary holds a 1 KB stack dump, too big for the main task's margin.
    esp_core_dump_summary_t* s = malloc(sizeof(*s));
    if (s == NULL) {
        return;
    }
    if (esp_core_dump_get_summary(s) == ESP_OK) {
        size_t len = s->exc_bt_info.dump_size < sizeof(s->exc_bt_info.stackdump) ? s->exc_bt_info.dump_size
                                                                                 : sizeof(s->exc_bt_info.stackdump);
        report.mcause = s->ex_info.mcause;
        report.mtval = s->ex_info.mtval;
        report.pc = s->exc_pc;
        report.ra = s->ex_info.ra;
        report.sp = s->ex_info.sp;
        memcpy(report.task, s->exc_task, sizeof(report.task));
        memcpy(report.elf_sha, s->app_elf_sha256, sizeof(report.elf_sha));
        report.pc_count = scan_stack(s->exc_bt_info.stackdump, len, report.pcs);
        present = true;
        ESP_LOGW(CRASH_TAG, "crash report: pc=0x%08x task=%.15s", (unsigned)report.pc, report.task);
    }
    free(s);
}

size_t crash_report_encode(uint8_t* out, size_t cap) {
    if (!present) {
        if (cap < 1) {
            return 0;
        }
        out[0] = 0;
        return 1;
    }
    size_t len = CRASH_REPORT_FIXED_LEN + 4 * (size_t)report.pc_count;
    if (cap < len) {
        return 0;
    }
    out[0] = 1;
    put_le32(out + 1, report.mcause);
    put_le32(out + 5, report.mtval);
    put_le32(out + 9, report.pc);
    put_le32(out + 13, report.ra);
    put_le32(out + 17, report.sp);
    memcpy(out + 21, report.task, sizeof(report.task));
    memcpy(out + 37, report.elf_sha, sizeof(report.elf_sha));
    out[46] = report.pc_count;
    for (int i = 0; i < report.pc_count; i++) {
        put_le32(out + CRASH_REPORT_FIXED_LEN + 4 * i, report.pcs[i]);
    }
    return len;
}

bool crash_report_clear(void) {
    if (esp_core_dump_image_erase() != ESP_OK) {
        return false;
    }
    memset(&report, 0, sizeof(report));
    present = false;
    return true;
}

// Unit tests for the real crash_report.c over stubbed core dump calls (PC build)
#ifndef ESP_PLATFORM

#include "../crash_report.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put_word(int i, uint32_t w) {
    memcpy(core_dump_stub_summary.exc_bt_info.stackdump + 4 * i, &w, 4);
}

static void reset(void) {
    memset(&core_dump_stub_summary, 0, sizeof(core_dump_stub_summary));
    core_dump_check_result = ESP_OK;
    core_dump_erase_result = ESP_OK;
    core_dump_erase_calls = 0;
}

static size_t encode(uint8_t* out) {
    return crash_report_encode(out, CRASH_REPORT_MAX_LEN);
}

static void test_no_dump(void) {
    uint8_t out[CRASH_REPORT_MAX_LEN];
    reset();
    core_dump_check_result = ESP_ERR_NOT_FOUND;
    crash_report_init();
    assert(encode(out) == 1 && out[0] == 0);
    assert(core_dump_erase_calls == 0);
}

static void test_valid_dump_layout(void) {
    uint8_t out[CRASH_REPORT_MAX_LEN];
    reset();
    core_dump_stub_summary.ex_info.mcause = 0x00000007;
    core_dump_stub_summary.ex_info.mtval = 0x11223344;
    core_dump_stub_summary.exc_pc = 0x42001234;
    core_dump_stub_summary.ex_info.ra = 0x42005678;
    core_dump_stub_summary.ex_info.sp = 0x3fc90000;
    strcpy(core_dump_stub_summary.exc_task, "BTC_TASK");
    memcpy(core_dump_stub_summary.app_elf_sha256, "abcdef012", 9);
    core_dump_stub_summary.exc_bt_info.dump_size = 20;
    put_word(0, 0x3fc90010);  // data address
    put_word(1, 0x42000100);  // flash code
    put_word(2, 0x00000000);
    put_word(3, 0x403e0000);  // one past IRAM code
    put_word(4, 0x4037c000);  // IRAM code
    crash_report_init();

    const uint8_t want[] = { 1,
        0x07,
        0,
        0,
        0,
        0x44,
        0x33,
        0x22,
        0x11,
        0x34,
        0x12,
        0x00,
        0x42,
        0x78,
        0x56,
        0x00,
        0x42,
        0x00,
        0x00,
        0xc9,
        0x3f,
        'B',
        'T',
        'C',
        '_',
        'T',
        'A',
        'S',
        'K',
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        'a',
        'b',
        'c',
        'd',
        'e',
        'f',
        '0',
        '1',
        '2',
        2,
        0x00,
        0x01,
        0x00,
        0x42,
        0x00,
        0xc0,
        0x37,
        0x40 };
    assert(encode(out) == sizeof(want) && memcmp(out, want, sizeof(want)) == 0);
    assert(core_dump_erase_calls == 0);
}

static void test_candidates_stop_at_16(void) {
    uint8_t out[CRASH_REPORT_MAX_LEN];
    reset();
    core_dump_stub_summary.exc_bt_info.dump_size = 1024;
    for (int i = 0; i < 256; i++) {
        put_word(i, 0x42000000 + i);
    }
    crash_report_init();
    assert(encode(out) == CRASH_REPORT_MAX_LEN && out[46] == 16);
    assert(out[47] == 0x00 && out[47 + 4 * 15] == 0x0f);
}

static void test_empty_stack_gives_no_candidates(void) {
    uint8_t out[CRASH_REPORT_MAX_LEN];
    reset();
    put_word(0, 0x42000000);
    crash_report_init();
    assert(encode(out) == CRASH_REPORT_FIXED_LEN && out[46] == 0);
}

static void test_dump_size_beyond_buffer_is_clamped(void) {
    uint8_t out[CRASH_REPORT_MAX_LEN];
    reset();
    core_dump_stub_summary.exc_bt_info.dump_size = 0xFFFFFFFF;
    crash_report_init();
    assert(encode(out) == CRASH_REPORT_FIXED_LEN);
}

static void test_encode_needs_room(void) {
    uint8_t out[CRASH_REPORT_MAX_LEN];
    reset();
    crash_report_init();
    assert(crash_report_encode(out, CRASH_REPORT_FIXED_LEN - 1) == 0);
    assert(crash_report_encode(out, 0) == 0);
}

static void test_invalid_dump_is_erased(void) {
    uint8_t out[CRASH_REPORT_MAX_LEN];
    reset();
    core_dump_check_result = ESP_ERR_INVALID_CRC;
    crash_report_init();
    assert(core_dump_erase_calls == 1 && encode(out) == 1 && out[0] == 0);
    reset();
    core_dump_check_result = ESP_ERR_INVALID_SIZE;
    crash_report_init();
    assert(core_dump_erase_calls == 1 && encode(out) == 1 && out[0] == 0);
}

static void test_clear(void) {
    uint8_t out[CRASH_REPORT_MAX_LEN];
    reset();
    crash_report_init();
    core_dump_erase_result = ESP_FAIL;
    assert(!crash_report_clear() && encode(out) == CRASH_REPORT_FIXED_LEN);
    core_dump_erase_result = ESP_OK;
    assert(crash_report_clear() && core_dump_erase_calls == 2);
    assert(encode(out) == 1 && out[0] == 0);
}

int main(void) {
    test_no_dump();
    test_valid_dump_layout();
    test_candidates_stop_at_16();
    test_empty_stack_gives_no_candidates();
    test_dump_size_beyond_buffer_is_clamped();
    test_encode_needs_room();
    test_invalid_dump_is_erased();
    test_clear();
    printf("crash_report: all passed\n");
    return 0;
}

#endif

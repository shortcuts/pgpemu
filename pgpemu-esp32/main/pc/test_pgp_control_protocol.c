// Unit tests for the Control Service wire-format protocol (PC build)
// Tests opcode/status constants and the [status][opcode][payload] framing
#ifndef ESP_PLATFORM

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Mirrors pgp_control.h's opcode table
typedef enum {
    CONTROL_OP_HELP = 0x01,
    CONTROL_OP_GET_GLOBAL_SETTINGS = 0x02,
    CONTROL_OP_SAVE_SETTINGS = 0x03,
    CONTROL_OP_GET_SECRETS = 0x04,
    CONTROL_OP_RESET_SECRETS = 0x05,
    CONTROL_OP_RESTART = 0x06,
    CONTROL_OP_GET_LED_STATE = 0x07,
    CONTROL_OP_CYCLE_LOG_LEVEL = 0x08,
    CONTROL_OP_GET_RUNTIME_STATS = 0x09,
    CONTROL_OP_GET_TASK_LIST = 0x0A,
    CONTROL_OP_ADVERTISE_START = 0x0B,
    CONTROL_OP_ADVERTISE_STOP = 0x0C,
    CONTROL_OP_GET_CLIENT_STATES = 0x0D,
    CONTROL_OP_RESET_CLIENT_STATES = 0x0E,
    CONTROL_OP_SET_MAX_CONNECTIONS = 0x0F,
    CONTROL_OP_TOGGLE_AUTOSPIN = 0x10,
    CONTROL_OP_TOGGLE_AUTOCATCH = 0x11,
} control_opcode_t;

// Mirrors pgp_control.h's status table
typedef enum {
    CONTROL_STATUS_OK = 0x00,
    CONTROL_STATUS_ERR_UNKNOWN_OPCODE = 0x01,
    CONTROL_STATUS_ERR_MALFORMED_PAYLOAD = 0x02,
    CONTROL_STATUS_ERR_NOT_BONDED = 0x03,
    CONTROL_STATUS_ERR_BUSY = 0x04,
    CONTROL_STATUS_ERR_INTERNAL = 0x05,
} control_status_t;

// Mirrors pgp_control.h's CONTROL_MAX_RESPONSE_PAYLOAD (500 - 2)
#define CONTROL_MAX_RESPONSE_PAYLOAD (500 - 2)

// Mirrors pgp_control_send_response's [status][opcode][payload] framing and truncation rule.
// Returns the total frame length written to out.
static size_t build_frame(control_status_t status,
    uint8_t opcode,
    const uint8_t* payload,
    size_t payload_len,
    uint8_t* out) {
    if (payload_len > CONTROL_MAX_RESPONSE_PAYLOAD) {
        payload_len = CONTROL_MAX_RESPONSE_PAYLOAD;
    }
    out[0] = (uint8_t)status;
    out[1] = opcode;
    if (payload_len > 0) {
        memcpy(out + 2, payload, payload_len);
    }
    return 2 + payload_len;
}

// Mirrors pgp_control_handle_command_write's split of value[0] / value+1.
// Returns false (and leaves outputs unset) when len == 0.
static bool parse_request(const uint8_t* value,
    uint16_t len,
    uint8_t* opcode,
    const uint8_t** payload,
    uint16_t* payload_len) {
    if (len < 1) {
        return false;
    }
    *opcode = value[0];
    *payload = value + 1;
    *payload_len = len - 1;
    return true;
}

static void test_opcode_constants() {
    printf("\n=== Test: Opcode Constants ===\n");

    control_opcode_t opcodes[] = { CONTROL_OP_HELP,
        CONTROL_OP_GET_GLOBAL_SETTINGS,
        CONTROL_OP_SAVE_SETTINGS,
        CONTROL_OP_GET_SECRETS,
        CONTROL_OP_RESET_SECRETS,
        CONTROL_OP_RESTART,
        CONTROL_OP_GET_LED_STATE,
        CONTROL_OP_CYCLE_LOG_LEVEL,
        CONTROL_OP_GET_RUNTIME_STATS,
        CONTROL_OP_GET_TASK_LIST,
        CONTROL_OP_ADVERTISE_START,
        CONTROL_OP_ADVERTISE_STOP,
        CONTROL_OP_GET_CLIENT_STATES,
        CONTROL_OP_RESET_CLIENT_STATES,
        CONTROL_OP_SET_MAX_CONNECTIONS,
        CONTROL_OP_TOGGLE_AUTOSPIN,
        CONTROL_OP_TOGGLE_AUTOCATCH };
    size_t count = sizeof(opcodes) / sizeof(opcodes[0]);
    assert(count == 0x11);
    printf("✓ Table has 17 opcodes (0x01-0x11)\n");

    for (size_t i = 0; i < count; i++) {
        assert((uint8_t)opcodes[i] == (uint8_t)(i + 1));
    }
    printf("✓ Opcodes are 0x01..0x11, no gaps\n");

    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            assert(opcodes[i] != opcodes[j]);
        }
    }
    printf("✓ No duplicate opcodes\n");
}

static void test_status_constants() {
    printf("\n=== Test: Status Constants ===\n");

    assert(CONTROL_STATUS_OK == 0x00);
    assert(CONTROL_STATUS_ERR_UNKNOWN_OPCODE == 0x01);
    assert(CONTROL_STATUS_ERR_MALFORMED_PAYLOAD == 0x02);
    assert(CONTROL_STATUS_ERR_NOT_BONDED == 0x03);
    assert(CONTROL_STATUS_ERR_BUSY == 0x04);
    assert(CONTROL_STATUS_ERR_INTERNAL == 0x05);
    printf("✓ Status codes are OK=0x00..ERR_INTERNAL=0x05\n");
}

static void test_build_frame_layout() {
    printf("\n=== Test: Response Frame Layout ===\n");

    uint8_t out[2 + CONTROL_MAX_RESPONSE_PAYLOAD];

    // 0-length payload
    size_t frame_len = build_frame(CONTROL_STATUS_OK, CONTROL_OP_HELP, NULL, 0, out);
    assert(frame_len == 2);
    assert(out[0] == CONTROL_STATUS_OK);
    assert(out[1] == CONTROL_OP_HELP);
    printf("✓ 0-length payload produces a 2-byte frame\n");

    // payload at exactly the cap
    uint8_t payload[CONTROL_MAX_RESPONSE_PAYLOAD];
    for (size_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (uint8_t)(i & 0xff);
    }
    frame_len = build_frame(CONTROL_STATUS_ERR_INTERNAL, CONTROL_OP_GET_SECRETS, payload, sizeof(payload), out);
    assert(frame_len == 2 + CONTROL_MAX_RESPONSE_PAYLOAD);
    assert(out[0] == CONTROL_STATUS_ERR_INTERNAL);
    assert(out[1] == CONTROL_OP_GET_SECRETS);
    assert(memcmp(out + 2, payload, sizeof(payload)) == 0);
    printf("✓ Payload at exactly CONTROL_MAX_RESPONSE_PAYLOAD (498) bytes is preserved\n");
}

static void test_build_frame_truncation() {
    printf("\n=== Test: Response Frame Truncation ===\n");

    uint8_t oversized[CONTROL_MAX_RESPONSE_PAYLOAD + 50];
    for (size_t i = 0; i < sizeof(oversized); i++) {
        oversized[i] = (uint8_t)(i & 0xff);
    }

    uint8_t out[2 + CONTROL_MAX_RESPONSE_PAYLOAD];
    size_t frame_len = build_frame(CONTROL_STATUS_OK, CONTROL_OP_GET_TASK_LIST, oversized, sizeof(oversized), out);

    assert(frame_len == 2 + CONTROL_MAX_RESPONSE_PAYLOAD);
    printf("✓ Truncated frame is exactly 2 + CONTROL_MAX_RESPONSE_PAYLOAD bytes\n");

    assert(frame_len <= 500);
    printf("✓ Frame never exceeds MAX_VALUE_LENGTH (500)\n");

    assert(memcmp(out + 2, oversized, CONTROL_MAX_RESPONSE_PAYLOAD) == 0);
    printf("✓ Truncated payload matches the first CONTROL_MAX_RESPONSE_PAYLOAD bytes\n");
}

static void test_parse_request() {
    printf("\n=== Test: Request Parsing ===\n");

    uint8_t opcode = 0;
    const uint8_t* payload = NULL;
    uint16_t payload_len = 0xffff;

    // len == 0 is rejected
    bool ok = parse_request(NULL, 0, &opcode, &payload, &payload_len);
    assert(!ok);
    printf("✓ len == 0 is rejected\n");

    // opcode-only write (len == 1)
    uint8_t value1[] = { CONTROL_OP_ADVERTISE_START };
    ok = parse_request(value1, sizeof(value1), &opcode, &payload, &payload_len);
    assert(ok);
    assert(opcode == CONTROL_OP_ADVERTISE_START);
    assert(payload_len == 0);
    printf("✓ len == 1 splits into opcode + empty payload\n");

    // opcode + payload
    uint8_t value2[] = { CONTROL_OP_SET_MAX_CONNECTIONS, 0x03 };
    ok = parse_request(value2, sizeof(value2), &opcode, &payload, &payload_len);
    assert(ok);
    assert(opcode == CONTROL_OP_SET_MAX_CONNECTIONS);
    assert(payload_len == 1);
    assert(payload[0] == 0x03);
    printf("✓ len >= 1 correctly splits value[0] / value+1\n");
}

int main() {
    printf("========================================\n");
    printf("Control Service Protocol Unit Tests\n");
    printf("========================================\n");

    test_opcode_constants();
    test_status_constants();
    test_build_frame_layout();
    test_build_frame_truncation();
    test_parse_request();

    printf("\n========================================\n");
    printf("✓ All control protocol tests passed!\n");
    printf("========================================\n");

    return 0;
}

#endif

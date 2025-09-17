#include "driver/uart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "config_secrets.h"
#include "config_storage.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "log_tags.h"
#include "mbedtls/base64.h"
#include "pgp_gap.h"
#include "pgp_gatts.h"
#include "pgp_handshake_multi.h"
#include "secrets.h"
#include "settings.h"
#include "stats.h"
#include "uart.h"

// scratchpad for reading/writing to nvs
static char tmp_clone_name[sizeof(PGP_CLONE_NAME)];
static uint8_t tmp_mac[sizeof(PGP_MAC)];
static uint8_t tmp_device_key[sizeof(PGP_DEVICE_KEY)];
static uint8_t tmp_blob[sizeof(PGP_BLOB)];

#if CONFIG_IDF_TARGET_ESP32C3 /* ----------- C3 : USB-Serial-JTAG ------ */

static int console_read(uint8_t* dst, size_t len, TickType_t to) {
    return usb_serial_jtag_read_bytes(dst, len, to);
}
static int console_write(const void* src, size_t len, TickType_t to) {
    return usb_serial_jtag_write_bytes(src, len, to);
}

#else /* -------- ESP32 & ESP32-S3 : UART0 ---- */

static const uart_port_t CONSOLE_UART = UART_NUM_0;
static QueueHandle_t uart_evt_q = NULL;

static int console_read(uint8_t* dst, size_t len, TickType_t to) {
    return uart_read_bytes(CONSOLE_UART, dst, len, to);
}
static int console_write(const void* src, size_t len, TickType_t to) {
    return uart_write_bytes(CONSOLE_UART, src, len);
}
#endif

/* convenience helpers (blocking, timeout 10 s) */
#define CONSOLE_GETCHAR(c) (console_read((uint8_t*)&(c), 1, pdMS_TO_TICKS(10000)) == 1)
#define CONSOLE_PUTS(str) console_write(str, strlen(str), portMAX_DELAY)

#if CONFIG_IDF_TARGET_ESP32C3
static void usb_console_task(void*); /* USB task   (c3)               */
#else
static void console_task(void*); /* UART task  (esp32/s3)         */
#endif
// static void uart_event_task(void *pvParameters);
static void uart_secrets_handler();
static void uart_target_connections_handler();
static bool decode_to_buf(char targetType, uint8_t* inBuf, int inBytes);
static void uart_restart_command();

void init_uart() {
#if CONFIG_IDF_TARGET_ESP32C3
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.tx_buffer_size = 256;
    cfg.rx_buffer_size = 256;
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    esp_vfs_usb_serial_jtag_use_driver();
#pragma GCC diagnostic pop

    xTaskCreate(usb_console_task, "usb_console", 4096, NULL, 12, NULL);
#else
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
#if CONFIG_IDF_TARGET_ESP32S3
        .tx_fifo_watermark = 0,
#endif
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(EX_UART_NUM, &uart_config);

    // Set UART pins (using UART0 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);

    // Install UART driver, and get the queue.
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0);

    xTaskCreate(console_task, "uart_console", 4096, NULL, 12, NULL);
#endif
}

static void process_char(uint8_t c) {
    if (c == 't') {
        // show connection status
        dump_client_connection_times();
    } else if (c == 'C') {
        // show full client details
        dump_client_states();
    } else if (c == 's') {
        // toggle autospin
        if (!toggle_setting(&settings.autospin)) {
            ESP_LOGE(UART_TAG, "failed!");
        }
        ESP_LOGI(UART_TAG, "autospin %s", get_setting(&settings.autospin) ? "on" : "off");
    } else if (c == 'c') {
        // toggle autocatch
        if (!toggle_setting(&settings.autocatch)) {
            ESP_LOGE(UART_TAG, "failed!");
        }
        ESP_LOGI(UART_TAG, "autocatch %s", get_setting(&settings.autocatch) ? "on" : "off");
    } else if (c == 'p') {
        // toggle ping
        if (!toggle_setting(&settings.powerbank_ping)) {
            ESP_LOGE(UART_TAG, "failed!");
        }
        ESP_LOGI(UART_TAG, "powerbank ping %s",
                 get_setting(&settings.powerbank_ping) ? "on" : "off");
    } else if (c == 'i') {
        // toggle show led interactions
        if (!toggle_setting(&settings.led_interactions)) {
            ESP_LOGE(UART_TAG, "failed!");
        }
        ESP_LOGI(UART_TAG, "show led interactions %s",
                 get_setting(&settings.led_interactions) ? "on" : "off");
    } else if (c == 'B') {
        // toggle input button
        if (!toggle_setting(&settings.use_button)) {
            ESP_LOGE(UART_TAG, "failed!");
        }
        ESP_LOGI(UART_TAG, "input button %s",
                 get_setting(&settings.use_button) ? "available" : "not available");
    } else if (c == 'O') {
        // toggle output led
        if (!toggle_setting(&settings.use_led)) {
            ESP_LOGE(UART_TAG, "failed!");
        }
        ESP_LOGI(UART_TAG, "output led %s",
                 get_setting(&settings.use_led) ? "available" : "not available");
    } else if (c == 'l') {
        // toggle verbose logging
        if (!toggle_setting(&settings.verbose)) {
            ESP_LOGE(UART_TAG, "failed!");
        }
        bool new_state = get_setting(&settings.verbose);
        if (new_state) {
            log_levels_max();
        } else {
            log_levels_min();
        }
        ESP_LOGI(UART_TAG, "verbose %s", new_state ? "on" : "off");
    } else if (c == 'A') {
        ESP_LOGI(UART_TAG, "starting advertising");
        pgp_advertise();
    } else if (c == 'a') {
        ESP_LOGI(UART_TAG, "stopping advertising");
        pgp_advertise_stop();
    } else if (c == 'S') {
        ESP_LOGI(UART_TAG, "saving configuration to nvs");
        bool ok = write_config_storage();
        if (ok) {
            ESP_LOGI(UART_TAG, "success!");
        }
    } else if (c == 'r') {
        ESP_LOGI(UART_TAG, "runtime: %lu min", stats_get_runtime());
    } else if (c == 'R') {
        // restart esp32
        uart_restart_command();
    } else if (c == 'T') {
        // show task list
        char buf[1024];  // "min. 40 bytes per task"
        vTaskList(buf);

        ESP_LOGI(UART_TAG, "Task List:\nTask Name\tStatus\tPrio\tHWM\tTask\tAffinity\n%s", buf);
        ESP_LOGI(UART_TAG, "Heap free: %lu bytes", esp_get_free_heap_size());
    } else if (c == 'X') {
        // enter secrets mode
        uart_secrets_handler();
    } else if (c == 'm') {
        uart_target_connections_handler();
    } else if (c == 'h' || c == '?') {
        ESP_LOGI(UART_TAG, "Device: %s", PGP_CLONE_NAME);
        ESP_LOGI(UART_TAG, "User Settings (lost on restart unless saved):");
        ESP_LOGI(UART_TAG, "- s - toggle PGP autospin");
        ESP_LOGI(UART_TAG, "- c - toggle PGP autocatch");
        ESP_LOGI(UART_TAG, "- p - toggle powerbank ping");
        ESP_LOGI(UART_TAG, "- i - toggle showing autospin/catch actions on LED");
        ESP_LOGI(UART_TAG, "- l - toggle verbose logging");
        ESP_LOGI(UART_TAG,
                 "- m... - set maximum client connections (eg. 3 clients max. with 'm3', up to %d)",
                 CONFIG_BT_ACL_CONNECTIONS);
        ESP_LOGI(UART_TAG, "- S - save user settings permanently");
        ESP_LOGI(UART_TAG, "Hardware Settings (only read at boot time, use 'S' to save):");
        ESP_LOGI(UART_TAG, "- B - toggle input button available");
        ESP_LOGI(UART_TAG, "- O - toggle output RGB LED available");
        ESP_LOGI(UART_TAG, "Commands:");
        ESP_LOGI(UART_TAG, "- h,? - help");
        ESP_LOGI(UART_TAG, "- X... - edit secrets mode (select eg. slot 2 with 'X2!')");
        ESP_LOGI(UART_TAG, "- A - start BT advertising");
        ESP_LOGI(UART_TAG, "- a - stop BT advertising");
        ESP_LOGI(UART_TAG, "- t - show BT connection times");
        ESP_LOGI(UART_TAG, "- C - show BT client states");
        ESP_LOGI(UART_TAG, "- r - show runtime counter");
        ESP_LOGI(UART_TAG, "- T - show FreeRTOS task list");
        ESP_LOGI(UART_TAG, "- f - show all configuration values");
        ESP_LOGI(UART_TAG, "- R - restart");
    } else if (c == 'f') {
        ESP_LOGI(UART_TAG, "Autospin: %s", get_setting(&settings.autospin) ? "on" : "off");
        ESP_LOGI(UART_TAG, "Autocatch: %s", get_setting(&settings.autocatch) ? "on" : "off");
        ESP_LOGI(UART_TAG, "Powerbank ping: %s",
                 get_setting(&settings.powerbank_ping) ? "on" : "off");
        ESP_LOGI(UART_TAG, "Show led interactions: %s",
                 get_setting(&settings.led_interactions) ? "on" : "off");
        ESP_LOGI(UART_TAG, "Input button: %s",
                 get_setting(&settings.use_button) ? "available" : "not available");
        ESP_LOGI(UART_TAG, "Output led: %s",
                 get_setting(&settings.use_led) ? "available" : "not available");
        ESP_LOGI(UART_TAG, "Verbose log: %s", get_setting(&settings.verbose) ? "on" : "off");
        ESP_LOGI(UART_TAG, "Chosen device #: %d - Name: %s",
                 get_setting_uint8(&settings.chosen_device), PGP_CLONE_NAME);
        ESP_LOGI(UART_TAG, "Connections: %d / Target: %d", get_active_connections(),
                 get_setting_uint8(&settings.target_active_connections));
    }
}

/* polling version (USB-Serial-JTAG, ESP32-C3) */
#if CONFIG_IDF_TARGET_ESP32C3
static void usb_console_task(void* pv) {
    uint8_t c;
    for (;;) {
        if (console_read(&c, 1, pdMS_TO_TICKS(20)) == 1) {
            process_char(c);
        }
    }
}
#else
/* UART queue version (ESP32 / S3) */
static void console_task(void* pv) {
    uart_event_t evt;
    uint8_t tmp[64];

    while (true) {
        if (xQueueReceive(uart_evt_q, &evt, portMAX_DELAY) && evt.type == UART_DATA) {
            int n = console_read(tmp, evt.size, 0);
            for (int i = 0; i < n; i++) process_char(tmp[i]);
        }
    }
}
#endif

static void uart_secrets_handler() {
    // uart rx buffer
    uint8_t buf[512];
    // pos in that buffer
    int pos = 0;

    // our signal that we're ready
    CONSOLE_PUTS("\n!\n");
    fflush(stdout);

    // secrets slot id
    uint8_t chosen_slot = 9;
    // indicate if a slot was explicitly chosen
    bool slot_chosen = false;

    char state = 0;
    bool isLineBreak = false;
    bool running = true;
    while (running) {
        // read one byte at a time to the current buffer position
        int size = console_read(buf + pos, 1, 10000 / portTICK_PERIOD_MS);
        if (size != 1) {
            // timeout, leave secrets mode
            break;
        }
        isLineBreak = (*(buf + pos) == '\n' || *(buf + pos) == '\r');

        switch (state) {
            case 0:
                // we received a command byte

                // reset buf pos
                pos = 0;
                switch (buf[0]) {
                    case '\n':
                    case '\r':
                    case 'X':
                        // ignore
                        break;

                    case '?':
                    case 'h':
                        // help/list slots
                        ESP_LOGW(UART_TAG, "Secrets Mode");
                        ESP_LOGI(UART_TAG, "User Commands:");
                        ESP_LOGI(UART_TAG, "- h,? - help");
                        ESP_LOGI(UART_TAG, "- q - leave secrets mode");
                        ESP_LOGI(UART_TAG, "- 0-9 - select secrets slot");
                        ESP_LOGI(UART_TAG, "- ! - activate selected slot and restart");
                        ESP_LOGI(UART_TAG, "- l - list slots");
                        ESP_LOGI(UART_TAG, "- C - clear slot");
                        ESP_LOGI(UART_TAG,
                                 "- there are additional commands but they are for use in "
                                 "secrets_upload.py only!");
                        break;

                    case 'l':
                        show_secrets_slots();
                        break;

                    case '!':
                        // activate selected slot and restart
                        if (!slot_chosen) {
                            ESP_LOGE(UART_TAG, "no slot chosen!");
                        } else {
                            ESP_LOGW(UART_TAG, "SETTING SLOT=%d AND RESTARTING", chosen_slot);
                            if (set_chosen_device(chosen_slot)) {
                                if (write_config_storage()) {
                                    uart_restart_command();
                                } else {
                                    ESP_LOGE(UART_TAG, "failed writing to nvs");
                                }
                            } else {
                                ESP_LOGE(UART_TAG, "failed editing settings");
                            }
                        }
                        break;

                    case 's': {
                        // get CRC of scratch buffer
                        uint32_t crc = get_secrets_crc32(tmp_mac, tmp_device_key, tmp_blob);
                        ESP_LOGI(UART_TAG, "slot=tmp crc=%08lx", crc);
                        break;
                    }

                    case 'S':
                        // read nvs slot to scratch buffer and get CRC of that
                        if (slot_chosen) {
                            if (read_secrets_id(chosen_slot, tmp_clone_name, tmp_mac,
                                                tmp_device_key, tmp_blob)) {
                                uint32_t crc = get_secrets_crc32(tmp_mac, tmp_device_key, tmp_blob);
                                ESP_LOGI(UART_TAG, "slot=%d crc=%08lx", chosen_slot, crc);
                            } else {
                                ESP_LOGE(UART_TAG, "failed reading slot=%d", chosen_slot);
                            }
                        }
                        break;

                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        // select secrets slot
                        chosen_slot = buf[0] - '0';
                        slot_chosen = true;
                        ESP_LOGW(UART_TAG, "slot=%d", chosen_slot);
                        break;

                    case 'N':
                        // set name
                    case 'M':
                        // set mac
                    case 'K':
                        // set device key
                    case 'B':
                        // set blob
                        state = buf[0];
                        ESP_LOGW(UART_TAG, "set=%c", state);
                        break;

                    case 'C':
                        // clear secret
                        if (slot_chosen) {
                            ESP_LOGW(UART_TAG, "clear=%d", delete_secrets_id(chosen_slot));
                        }
                        break;

                    case 'W':
                        // write secret
                        if (slot_chosen) {
                            ESP_LOGW(UART_TAG, "write=%d",
                                     write_secrets_id(chosen_slot, tmp_clone_name, tmp_mac,
                                                      tmp_device_key, tmp_blob));
                        }
                        break;

                    case 'q':
                        // quit
                    default:
                        if (buf[0] != 'q') {
                            ESP_LOGE(UART_TAG, "invalid command '%c'", buf[0]);
                        }
                        running = false;
                }
                fflush(stdout);
                break;

            case 'N':
                // set name
                if (pos >= (sizeof(PGP_CLONE_NAME) - 1)) {
                    ESP_LOGE(UART_TAG, "name too long");
                    running = false;
                } else if (isLineBreak) {
                    // copy name to global var
                    ESP_LOGW(UART_TAG, "N=[OK]");
                    // overwrite \n with termination byte
                    buf[pos] = 0;
                    memcpy(tmp_clone_name, buf, pos + 1);
                    // back to command mode
                    state = 0;
                    pos = 0;
                } else {
                    pos++;
                }
                break;

            case 'M':
                // set mac
            case 'K':
                // set device key
            case 'B':
                // set blob
                if (pos >= (sizeof(buf) - 1)) {
                    // this shouldn't happen normally because our buf is generously oversized
                    ESP_LOGE(UART_TAG, "data too long");
                    running = false;
                } else if (isLineBreak) {
                    // base64 decode and write to the correct global variable from secrets.c
                    if (decode_to_buf(state, buf, pos)) {
                        ESP_LOGW(UART_TAG, "%c=[OK]", state);
                    } else {
                        ESP_LOGW(UART_TAG, "%c=[FAIL]", state);
                    }
                    // back to command mode
                    state = 0;
                    pos = 0;
                } else {
                    pos++;
                }
                break;

            default:
                ESP_LOGE(UART_TAG, "invalid state");
                running = false;
        }
    }

    // our signal that we're done
    CONSOLE_PUTS("\nX\n");
    fflush(stdout);
}

// get a base64 encoded buffer (inBuf) with inBytes and write it to the target secret
static bool decode_to_buf(char targetType, uint8_t* inBuf, int inBytes) {
    int len;
    uint8_t* outBuf;

    switch (targetType) {
        case 'M':
            // set mac
            outBuf = tmp_mac;
            len = sizeof(tmp_mac);
            break;

        case 'K':
            // set device key
            outBuf = tmp_device_key;
            len = sizeof(tmp_device_key);
            break;

        case 'B':
            // set blob
            outBuf = tmp_blob;
            len = sizeof(tmp_blob);
            break;

        default:
            ESP_LOGE(UART_TAG, "invalid targetType=%c", targetType);
            return false;
    }

    size_t writtenLen = 0;
    int res = mbedtls_base64_decode(outBuf, len, &writtenLen, inBuf, inBytes);

    if (res != 0) {
        ESP_LOGE(UART_TAG, "mbedtls_base64_decode returned %d", res);
        return false;
    }

    if (writtenLen != len) {
        ESP_LOGE(UART_TAG, "wrong writtenLen! inBytes=%d -base64-> wanted=%d, got=%d", inBytes, len,
                 writtenLen);
        return false;
    }

    return true;
}

static void uart_restart_command() {
    ESP_LOGI(UART_TAG, "restarting");
    fflush(stdout);
    esp_restart();
}

static void uart_target_connections_handler() {
    uint8_t buf;
    int size = console_read(&buf, 1, 10000 / portTICK_PERIOD_MS);
    if (size != 1) {
        // timeout
        ESP_LOGE(UART_TAG, "[m]aximum active connections setting timeout");
        return;
    } else if (buf < '1' || buf > '9') {
        ESP_LOGE(UART_TAG, "only ascii numbers 1-%d are allowed", CONFIG_BT_ACL_CONNECTIONS);
        return;
    }

    int new_value = buf - '0';
    if (new_value < 1 || new_value > CONFIG_BT_ACL_CONNECTIONS) {
        ESP_LOGE(UART_TAG, "out of range: 1-%d", CONFIG_BT_ACL_CONNECTIONS);
        return;
    }

    if (set_setting_uint8(&settings.target_active_connections, new_value)) {
        ESP_LOGI(UART_TAG, "target_active_connections now %d (press 'S' to save permanently)",
                 new_value);
    } else {
        ESP_LOGE(UART_TAG, "failed editing setting");
    }
}

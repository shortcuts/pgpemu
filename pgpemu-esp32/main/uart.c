#include "driver/uart.h"

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

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int console_read(uint8_t* dst, size_t len, TickType_t to) {
    return usb_serial_jtag_read_bytes(dst, len, to);
}

/* convenience helpers (blocking, timeout 10 s) */
#define CONSOLE_GETCHAR(c) (console_read((uint8_t*)&(c), 1, pdMS_TO_TICKS(10000)) == 1)

static void usb_console_task(void*);
static void uart_secrets_handler();
static void uart_auto_handler(uint8_t c);
static void uart_bluetooth_handler();
static void uart_restart_command();

void init_uart() {
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.tx_buffer_size = 256;
    cfg.rx_buffer_size = 256;
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    esp_vfs_usb_serial_jtag_use_driver();
#pragma GCC diagnostic pop

    xTaskCreate(usb_console_task, "usb_console", 4096, NULL, 12, NULL);
}

void process_char(uint8_t c) {
    switch (c) {
    case '?':
        ESP_LOGI(UART_TAG,
            "---HELP---\n"
            "Secrets: %s\n"
            "Commands:\n"
            "- ? - help\n"
            "- l - cycle through log levels\n"
            "- r - show runtime counter\n"
            "- t - show FreeRTOS task list\n"
            "- s - show global settings values\n"
            "- S - save settings permanently\n"
            "- R - restart\n"
            "Secrets:\n"
            "- xs - show loaded secrets\n"
            "- xr - reset loaded secrets\n"
            "Bluetooth:\n"
            "- bA - start advertising\n"
            "- ba - stop advertising\n"
            "- bs - show client states\n"
            "- br - clear connections\n"
            "- b[1,4] - set maximum client connections (e.g. 3 clients max. with 'b3', up to %d, currently %d)\n"
            "Device Settings:\n"
            "- [1,4]s - toggle autospin\n"
            "- [1,4][0,9] - autospin probability\n"
            "- [1,4]c - toggle autocatch\n",
            PGP_CLONE_NAME,
            CONFIG_BT_ACL_CONNECTIONS,
            get_setting_uint8(&global_settings.target_active_connections));
        break;
    case 's':
        ESP_LOGI(UART_TAG,
            "---GLOBAL SETTINGS---\n"
            "- Log level: %d\n"
            "- Connections: %d / %d",
            get_setting_uint8(&global_settings.log_level),
            get_active_connections(),
            get_setting_uint8(&global_settings.target_active_connections));
        break;
    case 'S':
        ESP_LOGI(UART_TAG, "saving global configuration to nvs");
        if (write_global_settings_to_nvs()) {
            ESP_LOGI(UART_TAG, "success!");
        }
        ESP_LOGI(UART_TAG, "saving devices configuration to nvs");
        if (write_devices_settings_to_nvs()) {
            ESP_LOGI(UART_TAG, "success!");
        }
        break;
    case 'x':
        uart_secrets_handler();
        break;
    case 'b':
        uart_bluetooth_handler();
        break;
    case 'l':
        if (!cycle_log_level(&global_settings.log_level)) {
            ESP_LOGE(UART_TAG, "failed!");
        }
        uint8_t log_level = get_setting_uint8(&global_settings.log_level);
        if (log_level == 3) {
            ESP_LOGI(UART_TAG, "log level 3: verbose");
            log_levels_verbose();
        } else if (log_level == 2) {
            ESP_LOGI(UART_TAG, "log level 2: info");
            log_levels_info();
        } else {
            ESP_LOGI(UART_TAG, "log level 1: debug");
            log_levels_debug();
        }
        break;
    case 'r':
        stats_get_runtime();
        break;
    case 'R':
        uart_restart_command();
        break;
    case 't':
        char buf[1024];  // "min. 40 bytes per task"
        vTaskList(buf);

        ESP_LOGI(UART_TAG, "Task List:\nTask Name\tStatus\tPrio\tHWM\tTask\tAffinity\n%s", buf);
        ESP_LOGI(UART_TAG, "Heap free: %lu bytes", esp_get_free_heap_size());
        break;
    case '0':
    case '1':
    case '2':
    case '3':
        uart_auto_handler(c - '0');
        break;
    default:
        ESP_LOGE(UART_TAG, "unhandled input: %c", c);
    }
}

/* polling version (USB-Serial-JTAG, ESP32-C3) */
static void usb_console_task(void* pv) {
    uint8_t c;
    for (;;) {
        if (console_read(&c, 1, pdMS_TO_TICKS(20)) == 1) {
            process_char(c);
        }
    }
}

static void uart_auto_handler(uint8_t c) {
    uint8_t buf;

    int size = console_read(&buf, 1, 10000 / portTICK_PERIOD_MS);
    if (size != 1) {
        ESP_LOGE(UART_TAG, "auto setting timeout");
        return;
    }

    switch (buf) {
    case 's':
        ESP_LOGI(UART_TAG, "autospin: %d", toggle_device_autospin(c));
        break;
    case 'c':
        ESP_LOGI(UART_TAG, "autocatch: %d", toggle_device_autocatch(c));
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
        ESP_LOGI(UART_TAG, "autospin_probability: %d", set_device_autospin_probability(c, buf - '0'));
        break;
    default:
        ESP_LOGE(UART_TAG, "unknown auto handler case: a%c", buf);
    }
}

static void uart_secrets_handler() {
    uint8_t buf;

    int size = console_read(&buf, 1, 10000 / portTICK_PERIOD_MS);
    if (size != 1) {
        ESP_LOGE(UART_TAG, "secrets setting timeout");
        return;
    }

    switch (buf) {
    case 's':
        show_secrets();
        break;
    case 'r':
        if (reset_secrets()) {
            ESP_LOGW(UART_TAG, "secrets cleared");
        } else {
            ESP_LOGE(UART_TAG, "unable to clear secrets");
        }
        break;
    default:
        ESP_LOGE(UART_TAG, "unknown secret handler case: x%c", buf);
    }
}

static void uart_restart_command() {
    ESP_LOGI(UART_TAG, "restarting");
    fflush(stdout);
    esp_restart();
}

static void uart_bluetooth_handler() {
    uint8_t buf;

    int size = console_read(&buf, 1, 10000 / portTICK_PERIOD_MS);
    if (size != 1) {
        // timeout
        ESP_LOGE(UART_TAG, "bluetooth setting timeout");
        return;
    }

    switch (buf) {
    case 'A':
        pgp_advertise();
        break;
    case 'a':
        pgp_advertise_stop();
        break;
    case 's':
        dump_client_states();
        break;
    case 'r':
        reset_client_states();
        break;
    case '1':
    case '2':
    case '3':
    case '4':
        if (set_setting_uint8(&settings.target_active_connections, buf - '0')) {
            ESP_LOGI(UART_TAG, "target_active_connections now %c (press 'S' to save permanently)", buf);
        } else {
            ESP_LOGE(UART_TAG, "failed editing setting");
        }
        break;
    default:
        ESP_LOGE(UART_TAG, "unknown bluetooth handler case: b%c", buf);
    }
}

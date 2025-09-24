#ifndef LOG_TAGS_H
#define LOG_TAGS_H

// before initialization
void log_levels_debug();

// everything verbose
void log_levels_verbose();

// more manageable outputs
void log_levels_info();

static const char BT_GAP_TAG[] = "pgp_bt_gap";
static const char BT_GATTS_TAG[] = "pgp_bt_gatts";
static const char BT_TAG[] = "pgp_bluetooth";
static const char BUTTON_INPUT_TAG[] = "button_input";
static const char BUTTON_TASK_TAG[] = "pgp_autobutton";
static const char CERT_TAG[] = "pgp_cert";
static const char CONFIG_SECRETS_TAG[] = "config_secrets";
static const char CONFIG_STORAGE_TAG[] = "config_storage";
static const char HANDSHAKE_TAG[] = "pgp_handshake";
static const char LEDHANDLER_TAG[] = "pgp_led";
static const char PGPEMU_TAG[] = "PGPEMU";
static const char SETTING_TASK_TAG[] = "settings";
static const char STATS_TAG[] = "stats";
static const char UART_TAG[] = "uart_events";

#endif /* LOG_TAGS_H */

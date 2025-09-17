
#include <ctype.h>

#include "captive_dns.h"
#include "config_storage.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "log_tags.h"
#include "lwip/ip4_addr.h"
#include "nvs_flash.h"
#include "settings.h"

#define PGPEMU_SSID "PGPemu-Setup"

static const char* TAG = "CFG_PORTAL";
static httpd_handle_t s_srv;

static bool seen_ac, seen_as, seen_pp, seen_ub, seen_ul, seen_sl;

static inline int clamp_int(int v, int lo, int hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static void url_decode(char* s) {
    char* o = s;
    while (*s) {
        if (*s == '+') {
            *o++ = ' ';
            s++;
        } else if (*s == '%' && s[1] && s[2]) {
            int hi = (s[1] <= '9' ? s[1] - '0' : tolower(s[1]) - 'a' + 10);
            int lo = (s[2] <= '9' ? s[2] - '0' : tolower(s[2]) - 'a' + 10);
            *o++ = (hi << 4) | lo;
            s += 3;
        } else {
            *o++ = *s++;
        }
    }
    *o = 0;
}

/* ---------- HTTP handlers ---------- */
static esp_err_t root_get(httpd_req_t* r) {
    char page[1500];
    int n = snprintf(page, sizeof(page),
                     "<html><head>"
                     "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                     "<style>"
                     "body { background:#000; color:#fff; font-family:sans-serif; line-height:1.2; "
                     "margin:10px; }"
                     "form { margin:0; }"
                     "h2 { margin:0 0 8px; }"
                     "input[type=number], button { width:100%%; padding:6px 8px; margin:2px 0; "
                     "box-sizing:border-box; background:#222; color:#fff; border:1px solid #555; }"
                     "label { display:flex; align-items:center; margin:6px 0; }"
                     "label input[type=checkbox] { margin-right:8px; transform:scale(1.2); }"
                     "button { cursor:pointer; }"
                     "</style>"
                     "</head><body>"
                     "<h2>PGP-emu setup</h2>"
                     "<form method='POST' action='/save'>"
                     "Chosen Device (0-9):<input type='number' name='d' value='%d'>"
                     "Max connections (1-4):<input type='number' name='c' value='%d'>"
                     "<label><input type='checkbox' name='ac' %s> Auto-catch</label>"
                     "<label><input type='checkbox' name='as' %s> Auto-spin</label>"
                     "<label><input type='checkbox' name='pp' %s> Power-bank ping</label>"
                     "<label><input type='checkbox' name='ub' %s> Use button</label>"
                     "<label><input type='checkbox' name='ul' %s> Use LED</label>"
                     "<label><input type='checkbox' name='sl' %s> Show LED interactions</label>"
                     "<button type='submit'>Save & Reboot</button>"
                     "</form></body></html>",
                     get_setting_uint8(&settings.chosen_device),
                     get_setting_uint8(&settings.target_active_connections),
                     get_setting(&settings.autocatch) ? "checked" : "",
                     get_setting(&settings.autospin) ? "checked" : "",
                     get_setting(&settings.powerbank_ping) ? "checked" : "",
                     get_setting(&settings.use_button) ? "checked" : "",
                     get_setting(&settings.use_led) ? "checked" : "",
                     get_setting(&settings.led_interactions) ? "checked" : "");

    httpd_resp_send(r, page, n);
    return ESP_OK;
}

static void save_kv(char* key, char* val) {
    if (!strcmp(key, "d"))
        set_setting_uint8(&settings.chosen_device, clamp_int(atoi(val), 0, 9));
    else if (!strcmp(key, "c"))
        set_setting_uint8(&settings.target_active_connections, clamp_int(atoi(val), 1, 4));

    else if (!strcmp(key, "ac")) {
        set_setting_bool(&settings.autocatch, true);
        seen_ac = true;
    } else if (!strcmp(key, "as")) {
        set_setting_bool(&settings.autospin, true);
        seen_as = true;
    } else if (!strcmp(key, "pp")) {
        set_setting_bool(&settings.powerbank_ping, true);
        seen_pp = true;
    } else if (!strcmp(key, "ub")) {
        set_setting_bool(&settings.use_button, true);
        seen_ub = true;
    } else if (!strcmp(key, "ul")) {
        set_setting_bool(&settings.use_led, true);
        seen_ul = true;
    } else if (!strcmp(key, "sl")) {
        set_setting_bool(&settings.led_interactions, true);
        seen_sl = true;
    }
}

static esp_err_t save_post(httpd_req_t* r) {
    seen_ac = seen_as = seen_pp = seen_ub = seen_ul = false;
    char buf[256];
    int len = httpd_req_recv(r, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_500(r);
        return ESP_FAIL;
    }
    buf[len] = '\0';

    char* tok = strtok(buf, "&");
    while (tok) {
        char* eq = strchr(tok, '=');
        if (eq) {
            *eq = '\0';
            url_decode(tok);
            url_decode(eq + 1);
            save_kv(tok, eq + 1);
        }
        tok = strtok(NULL, "&");
    }

    if (!seen_ac)
        set_setting_bool(&settings.autocatch, false);
    if (!seen_as)
        set_setting_bool(&settings.autospin, false);
    if (!seen_pp)
        set_setting_bool(&settings.powerbank_ping, false);
    if (!seen_ub)
        set_setting_bool(&settings.use_button, false);
    if (!seen_ul)
        set_setting_bool(&settings.use_led, false);
    if (!seen_sl)
        set_setting_bool(&settings.led_interactions, false);

    write_config_storage();

    ESP_LOGI("PORTAL", "Saved settings, rebooting");
    httpd_resp_sendstr(r, "Saved settings, rebooting<br><a href='/'>Go back</a>");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return ESP_OK;
}

/* ---------- Wi-Fi Soft-AP ---------- */
static void wifi_ap_init(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap = {.ap = {.ssid = PGPEMU_SSID,
                               .authmode = WIFI_AUTH_OPEN,
                               .ssid_len = sizeof(PGPEMU_SSID) - 1,
                               .channel = 1,
                               .max_connection = 4}};
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    esp_wifi_start();
}

/* ---------- public entry ---------- */
void start_config_portal(void) {
    ESP_LOGI(TAG, "Button held - starting Wi-Fi setup portal");
    nvs_flash_init();  // make sure NVS is ready
    wifi_ap_init();

    // look up AP netif and fetch its IP
    esp_netif_ip_info_t ip;
    esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    char ip_str[16];

    if (ap_netif && esp_netif_get_ip_info(ap_netif, &ip) == ESP_OK) {
        // cast silences the ‘incompatible-pointer-type’ warning
        ip4addr_ntoa_r((const ip4_addr_t*)&ip.ip, ip_str, sizeof(ip_str));
        // captive_dns_start(ip_str); // needs more testing
        // ESP_LOGI("PORTAL", "Captive DNS replies with %s", ip_str);
    } else {
        ESP_LOGE("PORTAL", "Soft-AP netif not ready - DNS not started");
    }

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    httpd_start(&s_srv, &cfg);

    static const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get};
    static const httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = save_post};
    httpd_register_uri_handler(s_srv, &root);
    httpd_register_uri_handler(s_srv, &save);

    ESP_LOGI(TAG, "Connect to Wi-Fi “%s”, browse to %s", PGPEMU_SSID, ip_str);
    while (true) vTaskDelay(portMAX_DELAY);  // idle forever
}
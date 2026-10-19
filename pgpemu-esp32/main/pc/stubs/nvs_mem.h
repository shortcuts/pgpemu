// PC-test stub: in-memory NVS. Include once, in the test that links the real config_storage.c.
// A namespace exists after its first READWRITE open, as on the device.
#pragma once
#include "nvs.h"

#include <string.h>

#define NVS_MEM_MAX 64
static struct {
    char ns[16], key[16];
    uint8_t val[256];
    size_t len;
    int used;
} nvs_mem_entries[NVS_MEM_MAX];
static char nvs_mem_namespaces[8][16];
static char nvs_mem_handle_ns[8][16];
static int nvs_mem_handle_rw[8];
static int nvs_mem_fail_set;             // test hook: the Nth nvs_set_* call fails (0 = never)
static int nvs_mem_fail_commit;          // test hook: nvs_commit returns ESP_FAIL
static esp_err_t nvs_mem_init_part_err;  // test hook: nvs_flash_init_partition returns this
static char nvs_mem_last_partition[16];  // partition of the last nvs_open_from_partition

static inline void nvs_mem_reset(void) {
    memset(nvs_mem_entries, 0, sizeof(nvs_mem_entries));
    memset(nvs_mem_namespaces, 0, sizeof(nvs_mem_namespaces));
    memset(nvs_mem_handle_ns, 0, sizeof(nvs_mem_handle_ns));
    nvs_mem_fail_set = 0;
    nvs_mem_fail_commit = 0;
    nvs_mem_init_part_err = ESP_OK;
    nvs_mem_last_partition[0] = 0;
}

esp_err_t nvs_open(const char* ns, nvs_open_mode_t mode, nvs_handle_t* out) {
    int known = 0, free_slot = -1;
    for (int i = 0; i < 8; i++) {
        if (strcmp(nvs_mem_namespaces[i], ns) == 0) {
            known = 1;
        } else if (!nvs_mem_namespaces[i][0] && free_slot < 0) {
            free_slot = i;
        }
    }
    if (!known) {
        if (mode == NVS_READONLY) {
            return ESP_ERR_NVS_NOT_FOUND;
        }
        strcpy(nvs_mem_namespaces[free_slot], ns);
    }
    for (int h = 1; h < 8; h++) {
        if (!nvs_mem_handle_ns[h][0]) {
            strcpy(nvs_mem_handle_ns[h], ns);
            nvs_mem_handle_rw[h] = mode == NVS_READWRITE;
            *out = h;
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}
void nvs_close(nvs_handle_t h) {
    nvs_mem_handle_ns[h][0] = 0;
}
esp_err_t nvs_commit(nvs_handle_t h) {
    (void)h;
    return nvs_mem_fail_commit ? ESP_FAIL : ESP_OK;
}
static int nvs_mem_find(nvs_handle_t h, const char* key, int create) {
    int free_slot = -1;
    for (int i = 0; i < NVS_MEM_MAX; i++) {
        if (nvs_mem_entries[i].used && !strcmp(nvs_mem_entries[i].ns, nvs_mem_handle_ns[h])
            && !strcmp(nvs_mem_entries[i].key, key)) {
            return i;
        }
        if (!nvs_mem_entries[i].used && free_slot < 0) {
            free_slot = i;
        }
    }
    if (create && free_slot >= 0) {
        nvs_mem_entries[free_slot].used = 1;
        strcpy(nvs_mem_entries[free_slot].ns, nvs_mem_handle_ns[h]);
        strcpy(nvs_mem_entries[free_slot].key, key);
        return free_slot;
    }
    return -1;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char* key, const void* v, size_t len) {
    if (nvs_mem_fail_set && --nvs_mem_fail_set == 0) {
        return ESP_FAIL;
    }
    if (!nvs_mem_handle_rw[h] || len > sizeof(nvs_mem_entries[0].val)) {
        return ESP_FAIL;
    }
    int i = nvs_mem_find(h, key, 1);
    if (i < 0) {
        return ESP_FAIL;
    }
    memcpy(nvs_mem_entries[i].val, v, len);
    nvs_mem_entries[i].len = len;
    return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h, const char* key, void* v, size_t* len) {
    int i = nvs_mem_find(h, key, 0);
    if (i < 0) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if (v) {
        if (*len < nvs_mem_entries[i].len) {
            return ESP_ERR_NVS_INVALID_LENGTH;
        }
        memcpy(v, nvs_mem_entries[i].val, nvs_mem_entries[i].len);
    }
    *len = nvs_mem_entries[i].len;
    return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t h, const char* key, uint8_t v) {
    return nvs_set_blob(h, key, &v, 1);
}
esp_err_t nvs_set_i8(nvs_handle_t h, const char* key, int8_t v) {
    return nvs_set_blob(h, key, &v, 1);
}
esp_err_t nvs_get_u8(nvs_handle_t h, const char* key, uint8_t* v) {
    size_t n = 1;
    return nvs_get_blob(h, key, v, &n);
}
esp_err_t nvs_get_i8(nvs_handle_t h, const char* key, int8_t* v) {
    size_t n = 1;
    return nvs_get_blob(h, key, v, &n);
}
esp_err_t nvs_erase_key(nvs_handle_t h, const char* key) {
    int i = nvs_mem_find(h, key, 0);
    if (i < 0) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    nvs_mem_entries[i].used = 0;
    return ESP_OK;
}
esp_err_t nvs_open_from_partition(const char* part, const char* ns, nvs_open_mode_t mode, nvs_handle_t* out) {
    strcpy(nvs_mem_last_partition, part);
    return nvs_open(ns, mode, out);
}
esp_err_t nvs_flash_init_partition(const char* part) {
    (void)part;
    return nvs_mem_init_part_err;
}
esp_err_t nvs_get_str(nvs_handle_t h, const char* key, char* v, size_t* len) {
    return nvs_get_blob(h, key, v, len);
}
esp_err_t nvs_set_str(nvs_handle_t h, const char* key, const char* v) {
    return nvs_set_blob(h, key, v, strlen(v) + 1);
}
esp_err_t nvs_erase_all(nvs_handle_t h) {
    for (int i = 0; i < NVS_MEM_MAX; i++) {
        if (nvs_mem_entries[i].used && !strcmp(nvs_mem_entries[i].ns, nvs_mem_handle_ns[h])) {
            nvs_mem_entries[i].used = 0;
        }
    }
    return ESP_OK;
}

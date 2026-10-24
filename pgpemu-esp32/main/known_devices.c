#include "known_devices.h"

#include "config_storage.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "log_tags.h"
#include "nvs_helper.h"
#include "pgp_handshake_multi.h"

#include <string.h>

#define BLOB_LEN 26  // bda[6] seq u32 LE name[16]

typedef struct {
    esp_bd_addr_t bda;  // all zero: free slot
    uint32_t seq;
    char name[KNOWN_DEVICE_NAME_LEN];
} record_t;

static const char NAMESPACE[] = "known_devs";

// No mutex: touch and the Control Service handlers both run on BTC_TASK.
static record_t table[KNOWN_DEVICES_MAX];

static bool is_free(const record_t* r) {
    static const esp_bd_addr_t zero = { 0 };
    return memcmp(r->bda, zero, sizeof(zero)) == 0;
}

static int find(const uint8_t* bda) {
    for (int i = 0; i < KNOWN_DEVICES_MAX; i++) {
        if (!is_free(&table[i]) && memcmp(table[i].bda, bda, sizeof(esp_bd_addr_t)) == 0) {
            return i;
        }
    }
    return -1;
}

static bool is_connected(const record_t* r) {
    return get_client_state_entry_by_bda((uint8_t*)r->bda) != NULL;
}

static uint32_t highest_seq(void) {
    uint32_t top = 0;
    for (int i = 0; i < KNOWN_DEVICES_MAX; i++) {
        if (!is_free(&table[i]) && table[i].seq > top) {
            top = table[i].seq;
        }
    }
    return top;
}

static void put_seq_le(uint8_t* out, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        out[i] = (uint8_t)(v >> (8 * i));
    }
}

static void make_key(char key[4], int i) {
    key[0] = 'k';
    key[1] = (char)('0' + i / 10);
    key[2] = (char)('0' + i % 10);
    key[3] = '\0';
}

static bool save_slot(int i) {
    uint8_t blob[BLOB_LEN];
    memcpy(blob, table[i].bda, 6);
    put_seq_le(blob + 6, table[i].seq);
    memcpy(blob + 10, table[i].name, KNOWN_DEVICE_NAME_LEN);

    char key[4];
    make_key(key, i);
    nvs_handle_t handle = {};
    if (!nvs_open_readwrite(KNOWN_DEVICES_TAG, NAMESPACE, &handle)) {
        return false;
    }
    if (!nvs_write_check(KNOWN_DEVICES_TAG, nvs_set_blob(handle, key, blob, BLOB_LEN), key)) {
        nvs_safe_close(handle);
        return false;
    }
    return nvs_commit_and_close(KNOWN_DEVICES_TAG, handle, key);
}

void known_devices_init(void) {
    memset(table, 0, sizeof(table));
    nvs_handle_t handle = {};
    if (!nvs_open_readonly(KNOWN_DEVICES_TAG, NAMESPACE, &handle)) {
        return;
    }
    int loaded = 0;
    for (int i = 0; i < KNOWN_DEVICES_MAX; i++) {
        char key[4];
        uint8_t blob[BLOB_LEN];
        make_key(key, i);
        if (!nvs_read_blob_checked(KNOWN_DEVICES_TAG, handle, key, blob, BLOB_LEN)) {
            continue;
        }
        memcpy(table[i].bda, blob, 6);
        table[i].seq = (uint32_t)blob[6] | (uint32_t)blob[7] << 8 | (uint32_t)blob[8] << 16 | (uint32_t)blob[9] << 24;
        memcpy(table[i].name, blob + 10, KNOWN_DEVICE_NAME_LEN);
        table[i].name[KNOWN_DEVICE_NAME_LEN - 1] = '\0';
        loaded += !is_free(&table[i]);
    }
    nvs_safe_close(handle);
    ESP_LOGI(KNOWN_DEVICES_TAG, "known devices: loaded %d", loaded);
}

// Lowest seq among the records that are not connected; -1 when every record is connected.
static int eviction_victim(void) {
    int victim = -1;
    for (int i = 0; i < KNOWN_DEVICES_MAX; i++) {
        if (!is_connected(&table[i]) && (victim < 0 || table[i].seq < table[victim].seq)) {
            victim = i;
        }
    }
    return victim;
}

static int find_free(void) {
    for (int i = 0; i < KNOWN_DEVICES_MAX; i++) {
        if (is_free(&table[i])) {
            return i;
        }
    }
    return -1;
}

static bool erase_device(const uint8_t* bda) {
    esp_bd_addr_t addr;
    memcpy(addr, bda, sizeof(addr));
    bool erased = erase_device_data(addr);
    esp_ble_remove_bond_device(addr);
    return erased;
}

void known_devices_touch(const uint8_t* bda) {
    static const esp_bd_addr_t zero = { 0 };
    if (memcmp(bda, zero, sizeof(zero)) == 0) {
        return;
    }
    uint32_t top = highest_seq();
    int i = find(bda);
    if (i >= 0 && table[i].seq == top) {
        return;
    }
    if (i < 0) {
        i = find_free();
        if (i < 0) {
            i = eviction_victim();
            if (i < 0) {
                ESP_LOGW(KNOWN_DEVICES_TAG, "index full and every record connected, not indexed");
                return;
            }
            ESP_LOGI(KNOWN_DEVICES_TAG, "evicting least recently seen record %d", i);
            if (!erase_device(table[i].bda)) {
                ESP_LOGW(KNOWN_DEVICES_TAG, "erasing the evicted record's data failed");
            }
        }
        memset(&table[i], 0, sizeof(table[i]));
        memcpy(table[i].bda, bda, sizeof(esp_bd_addr_t));
    }
    table[i].seq = top + 1;
    if (!save_slot(i)) {
        ESP_LOGW(KNOWN_DEVICES_TAG, "saving record %d failed", i);
    }
}

known_devices_result_t known_devices_list(uint8_t* buf, size_t cap, size_t* out_len) {
    if (cap < KNOWN_DEVICES_LIST_MAX_LEN) {
        return KNOWN_DEVICES_INTERNAL;
    }
    int order[KNOWN_DEVICES_MAX];
    int count = 0;
    for (int i = 0; i < KNOWN_DEVICES_MAX; i++) {
        if (is_free(&table[i])) {
            continue;
        }
        int k = count++;
        for (; k > 0 && table[order[k - 1]].seq < table[i].seq; k--) {
            order[k] = order[k - 1];
        }
        order[k] = i;
    }
    buf[0] = (uint8_t)count;
    uint8_t* out = buf + 1;
    for (int k = 0; k < count; k++, out += KNOWN_DEVICE_WIRE_LEN) {
        const record_t* r = &table[order[k]];
        memcpy(out, r->bda, 6);
        put_seq_le(out + 6, r->seq);
        out[10] = is_connected(r) ? 1 : 0;
        memcpy(out + 11, r->name, KNOWN_DEVICE_NAME_LEN);
    }
    *out_len = (size_t)(out - buf);
    return KNOWN_DEVICES_OK;
}

known_devices_result_t known_devices_rename(const uint8_t* bda, const char* name, size_t len) {
    int i = find(bda);
    if (i < 0) {
        return KNOWN_DEVICES_NOT_FOUND;
    }
    if (len >= KNOWN_DEVICE_NAME_LEN) {
        len = KNOWN_DEVICE_NAME_LEN - 1;
    }
    memset(table[i].name, 0, KNOWN_DEVICE_NAME_LEN);
    memcpy(table[i].name, name, len);
    return save_slot(i) ? KNOWN_DEVICES_OK : KNOWN_DEVICES_INTERNAL;
}

known_devices_result_t known_devices_forget(const uint8_t* bda) {
    int i = find(bda);
    if (i < 0) {
        return KNOWN_DEVICES_NOT_FOUND;
    }
    if (is_connected(&table[i])) {
        return KNOWN_DEVICES_BUSY;
    }
    // A failed erase keeps the record, so a retry still finds the address.
    if (!erase_device(table[i].bda)) {
        return KNOWN_DEVICES_INTERNAL;
    }
    memset(&table[i], 0, sizeof(table[i]));
    ESP_LOGI(KNOWN_DEVICES_TAG, "forgot record %d", i);
    return save_slot(i) ? KNOWN_DEVICES_OK : KNOWN_DEVICES_INTERNAL;
}

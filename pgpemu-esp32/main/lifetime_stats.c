#include "lifetime_stats.h"

#include "config_storage.h"
#include "esp_log.h"
#include "known_devices.h"
#include "log_tags.h"
#include "pgp_handshake_multi.h"
#include "stats.h"

#include <string.h>

typedef struct {
    bool used;
    uint16_t conn_id;
    esp_bd_addr_t bda;
    uint32_t total[LIFETIME_STAT_COUNT];
    uint8_t unflushed;
} entry_t;

// No mutex: the LED handler, the disconnect path and the Control Service handlers all run on BTC_TASK.
static entry_t entries[CONFIG_BT_ACL_CONNECTIONS];

static void load(const uint8_t* bda, uint32_t total[LIFETIME_STAT_COUNT]) {
    uint8_t blob[LIFETIME_STATS_BLOB_LEN];
    esp_bd_addr_t addr;
    memcpy(addr, bda, sizeof(addr));
    if (!read_lifetime_stats(addr, blob)) {
        memset(blob, 0, sizeof(blob));
    }
    for (int i = 0; i < LIFETIME_STAT_COUNT; i++) {
        total[i] = (uint32_t)blob[4 * i] | (uint32_t)blob[4 * i + 1] << 8 | (uint32_t)blob[4 * i + 2] << 16
                   | (uint32_t)blob[4 * i + 3] << 24;
    }
}

static bool store(const uint8_t* bda, const uint32_t total[LIFETIME_STAT_COUNT]) {
    uint8_t blob[LIFETIME_STATS_BLOB_LEN];
    esp_bd_addr_t addr;
    memcpy(addr, bda, sizeof(addr));
    for (int i = 0; i < LIFETIME_STAT_COUNT; i++) {
        for (int b = 0; b < 4; b++) {
            blob[4 * i + b] = (uint8_t)(total[i] >> (8 * b));
        }
    }
    return write_lifetime_stats(addr, blob);
}

static entry_t* find_by_conn(uint16_t conn_id) {
    for (int i = 0; i < CONFIG_BT_ACL_CONNECTIONS; i++) {
        if (entries[i].used && entries[i].conn_id == conn_id) {
            return &entries[i];
        }
    }
    return NULL;
}

static entry_t* find_by_bda(const uint8_t* bda) {
    for (int i = 0; i < CONFIG_BT_ACL_CONNECTIONS; i++) {
        if (entries[i].used && memcmp(entries[i].bda, bda, sizeof(esp_bd_addr_t)) == 0) {
            return &entries[i];
        }
    }
    return NULL;
}

static entry_t* create(uint16_t conn_id) {
    static const esp_bd_addr_t zero = { 0 };
    client_state_t* client = get_client_state_entry(conn_id);
    if (client == NULL || memcmp(client->remote_bda, zero, sizeof(zero)) == 0
        || !known_devices_contains(client->remote_bda)) {
        return NULL;
    }
    for (int i = 0; i < CONFIG_BT_ACL_CONNECTIONS; i++) {
        if (!entries[i].used) {
            memset(&entries[i], 0, sizeof(entries[i]));
            entries[i].used = true;
            entries[i].conn_id = conn_id;
            memcpy(entries[i].bda, client->remote_bda, sizeof(esp_bd_addr_t));
            load(entries[i].bda, entries[i].total);
            return &entries[i];
        }
    }
    return NULL;
}

// A failed write keeps the count pending, so the next trigger retries it.
static void flush(entry_t* e) {
    if (e->unflushed == 0) {
        return;
    }
    if (store(e->bda, e->total)) {
        e->unflushed = 0;
    } else {
        ESP_LOGW(LIFETIME_TAG, "[%d] writing lifetime stats failed", e->conn_id);
    }
}

void lifetime_stats_add(uint16_t conn_id, lifetime_stat_t kind) {
    if (kind >= LIFETIME_STAT_COUNT) {
        return;
    }
    entry_t* e = find_by_conn(conn_id);
    if (e == NULL) {
        e = create(conn_id);
    }
    if (e == NULL) {
        return;
    }
    e->total[kind]++;
    if (++e->unflushed >= LIFETIME_FLUSH_EVERY) {
        flush(e);
    }
}

void lifetime_stats_release(uint16_t conn_id) {
    entry_t* e = find_by_conn(conn_id);
    if (e != NULL) {
        flush(e);
        e->used = false;
    }
}

void lifetime_stats_flush_all(void) {
    for (int i = 0; i < CONFIG_BT_ACL_CONNECTIONS; i++) {
        if (entries[i].used) {
            flush(&entries[i]);
        }
    }
}

void lifetime_stats_get(const uint8_t* bda,
    uint32_t total[LIFETIME_STAT_COUNT],
    uint16_t session[LIFETIME_STAT_COUNT],
    bool* connected) {
    memset(session, 0, LIFETIME_STAT_COUNT * sizeof(session[0]));
    entry_t* e = find_by_bda(bda);
    if (e != NULL) {
        memcpy(total, e->total, LIFETIME_STAT_COUNT * sizeof(total[0]));
    } else {
        load(bda, total);
    }
    client_state_t* client = get_client_state_entry_by_bda((uint8_t*)bda);
    *connected = client != NULL;
    Stats stats;
    if (client != NULL && stats_get_for_conn(client->conn_id, &stats)) {
        session[LIFETIME_CAUGHT] = stats.caught;
        session[LIFETIME_FLED] = stats.fled;
        session[LIFETIME_SPIN] = stats.spin;
        session[LIFETIME_BAG_FULL] = stats.bag_full;
        session[LIFETIME_BOX_FULL] = stats.box_full;
        session[LIFETIME_BALLS_EMPTY] = stats.balls_empty;
    }
}

bool lifetime_stats_reset(const uint8_t* bda) {
    uint32_t zero[LIFETIME_STAT_COUNT] = { 0 };
    if (!store(bda, zero)) {
        return false;
    }
    entry_t* e = find_by_bda(bda);
    if (e != NULL) {
        memset(e->total, 0, sizeof(e->total));
        e->unflushed = 0;
    }
    return true;
}

#include "stats.h"

#include "buf_writer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lifetime_stats.h"
#include "log_tags.h"
#include "nvs.h"

#include <stdint.h>

#define MAX_CONNECTIONS CONFIG_BT_ACL_CONNECTIONS

static StatsForConn stats[MAX_CONNECTIONS];
static size_t stats_len = 0;

StatsForConn* get_conn_entry(uint16_t conn_id) {
    for (size_t i = 0; i < stats_len; i++) {
        if (stats[i].conn_id == conn_id) {
            return &stats[i];
        }
    }
    if (stats_len < MAX_CONNECTIONS) {
        stats[stats_len].conn_id = conn_id;
        stats[stats_len].stats = (Stats){ 0 };
        stats_len++;

        ESP_LOGI(STATS_TAG, "new entry for conn_id %d added, current len %d", conn_id, stats_len);

        return &stats[stats_len - 1];
    }

    ESP_LOGE(STATS_TAG, "no stat found for conn_id %d or impossible to store a new entry", conn_id);

    return NULL;
}

bool stats_get_for_conn(uint16_t conn_id, Stats* out) {
    for (size_t i = 0; i < stats_len; i++) {
        if (stats[i].conn_id == conn_id) {
            *out = stats[i].stats;
            return true;
        }
    }
    return false;
}

void stats_clear_for_conn(uint16_t conn_id) {
    for (size_t i = 0; i < stats_len; i++) {
        if (stats[i].conn_id == conn_id) {
            stats[i] = stats[stats_len - 1];
            stats_len--;
            return;
        }
    }
}

static void increment(uint16_t conn_id, lifetime_stat_t kind) {
    StatsForConn* s = get_conn_entry(conn_id);
    if (s == NULL) {
        return;
    }
    switch (kind) {
    case LIFETIME_CAUGHT:
        s->stats.caught++;
        break;
    case LIFETIME_FLED:
        s->stats.fled++;
        break;
    case LIFETIME_SPIN:
        s->stats.spin++;
        break;
    case LIFETIME_BAG_FULL:
        s->stats.bag_full++;
        break;
    case LIFETIME_BOX_FULL:
        s->stats.box_full++;
        break;
    default:
        s->stats.balls_empty++;
        break;
    }
    lifetime_stats_add(conn_id, kind);
}

void increment_caught(uint16_t conn_id) {
    increment(conn_id, LIFETIME_CAUGHT);
}

void increment_fled(uint16_t conn_id) {
    increment(conn_id, LIFETIME_FLED);
}

void increment_spin(uint16_t conn_id) {
    increment(conn_id, LIFETIME_SPIN);
}

void increment_bag_full(uint16_t conn_id) {
    increment(conn_id, LIFETIME_BAG_FULL);
}

void increment_box_full(uint16_t conn_id) {
    increment(conn_id, LIFETIME_BOX_FULL);
}

void increment_balls_empty(uint16_t conn_id) {
    increment(conn_id, LIFETIME_BALLS_EMPTY);
}

size_t stats_format_runtime(char* buf, size_t buf_len) {
    if (stats_len == 0) {
        return 0;
    }

    buf_writer_t writer;
    buf_writer_init(&writer, buf, buf_len);
    for (size_t i = 0; i < stats_len; i++) {
        buf_writer_appendf(&writer,
            "---STATS---\n"
            "Connection %d:\n"
            "- Caught: %d\n"
            "- Fled: %d\n"
            "- Spin: %d\n"
            "- Bag full: %d\n"
            "- Box full: %d\n"
            "- Balls empty: %d\n",
            stats[i].conn_id,
            stats[i].stats.caught,
            stats[i].stats.fled,
            stats[i].stats.spin,
            stats[i].stats.bag_full,
            stats[i].stats.box_full,
            stats[i].stats.balls_empty);
    }
    return buf_writer_len(&writer);
}

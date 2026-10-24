#ifndef LIFETIME_STATS_H
#define LIFETIME_STATS_H

#include <stdbool.h>
#include <stdint.h>

// Order is the same in RAM, NVS (u32 LE each) and on the wire.
typedef enum {
    LIFETIME_CAUGHT,
    LIFETIME_FLED,
    LIFETIME_SPIN,
    LIFETIME_BAG_FULL,
    LIFETIME_BOX_FULL,
    LIFETIME_BALLS_EMPTY,
    LIFETIME_STAT_COUNT,
} lifetime_stat_t;

// A pending count reaches NVS after this many events, on disconnect and before a restart.
#define LIFETIME_FLUSH_EVERY 16

// Counts one event for the phone on conn_id. Ignored when the phone is not in the Known Devices index.
void lifetime_stats_add(uint16_t conn_id, lifetime_stat_t kind);
// Writes the pending counts of conn_id to NVS and drops its entry; conn_id values get reused.
void lifetime_stats_release(uint16_t conn_id);
void lifetime_stats_flush_all(void);
// Fills the lifetime totals (RAM if connected and counted, else NVS) and this connection's session counts.
void lifetime_stats_get(const uint8_t* bda,
    uint32_t total[LIFETIME_STAT_COUNT],
    uint16_t session[LIFETIME_STAT_COUNT],
    bool* connected);
// Zeroes the lifetime totals in NVS and RAM. False when the NVS write fails.
bool lifetime_stats_reset(const uint8_t* bda);

#endif /* LIFETIME_STATS_H */

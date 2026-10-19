#ifndef STATS_H
#define STATS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Formats the runtime stats into buf.
// Returns the number of bytes written (excluding the null terminator).
size_t stats_format_runtime(char* buf, size_t buf_len);

void increment_caught(uint16_t conn_id);
void increment_fled(uint16_t conn_id);
void increment_spin(uint16_t conn_id);

typedef struct {
    uint16_t caught;
    uint16_t fled;
    uint16_t spin;
} Stats;

typedef struct {
    uint16_t conn_id;
    Stats stats;
} StatsForConn;

// Returns true and fills *out if a stats entry exists for conn_id.
// Unlike the internal get_conn_entry(), never creates a new entry.
bool stats_get_for_conn(uint16_t conn_id, Stats* out);

// Drops conn_id's entry so a later connection reusing the id starts from zero.
void stats_clear_for_conn(uint16_t conn_id);

#endif /* STATS_H */

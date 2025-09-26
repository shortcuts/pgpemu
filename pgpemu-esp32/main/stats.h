#ifndef STATS_H
#define STATS_H

#include <stdint.h>

void stats_get_runtime();

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

#endif /* STATS_H */

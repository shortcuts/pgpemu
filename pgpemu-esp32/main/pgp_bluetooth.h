#ifndef PGP_BLUETOOTH_H
#define PGP_BLUETOOTH_H

#include <stdbool.h>
#include <stdint.h>

bool init_bluetooth();

extern uint8_t bt_mac[6];

#endif /* PGP_BLUETOOTH_H */

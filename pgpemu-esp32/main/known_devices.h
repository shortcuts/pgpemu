#ifndef KNOWN_DEVICES_H
#define KNOWN_DEVICES_H

#include "esp_bt_defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Bluedroid holds no more bonds than this, so a longer index would have nothing left to evict.
#define KNOWN_DEVICES_MAX CONFIG_BT_SMP_MAX_BONDS
#define KNOWN_DEVICE_NAME_LEN 16
// bda[6] seq u32 LE flags u8 (bit 0: connected now) name[16]
#define KNOWN_DEVICE_WIRE_LEN 27
#define KNOWN_DEVICES_LIST_MAX_LEN (1 + KNOWN_DEVICES_MAX * KNOWN_DEVICE_WIRE_LEN)

typedef enum {
    KNOWN_DEVICES_OK,
    KNOWN_DEVICES_NOT_FOUND,
    KNOWN_DEVICES_BUSY,
    KNOWN_DEVICES_INTERNAL,
} known_devices_result_t;

// Loads the index from NVS. Before this call the index reads as empty.
void known_devices_init(void);
// Marks the address as seen now; evicts the least recently seen one (keys, settings, bond) when the index is full.
void known_devices_touch(const uint8_t* bda);
// Writes count u8, then the records, most recently seen first. Fails when cap is too small.
known_devices_result_t known_devices_list(uint8_t* buf, size_t cap, size_t* out_len);
// Sets the name; an empty name clears it. At most KNOWN_DEVICE_NAME_LEN - 1 bytes.
known_devices_result_t known_devices_rename(const uint8_t* bda, const char* name, size_t len);
// Removes the record, the address's session keys and settings, and its bond. Refuses a connected address.
known_devices_result_t known_devices_forget(const uint8_t* bda);

#endif /* KNOWN_DEVICES_H */

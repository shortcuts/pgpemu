#ifndef CONFIG_SECRETS_H
#define CONFIG_SECRETS_H

#include <stdbool.h>
#include <stdint.h>

// print to log
void show_secrets_slots();

// delete slot data
bool delete_secrets_id(uint8_t id);

// read secrets slot with given id from nvs. you must ensure the output buffers have the right
// sizes!
bool read_secrets_id(uint8_t id, char* name, uint8_t* mac, uint8_t* key, uint8_t* blob);

bool write_secrets_id(uint8_t id, char* name, uint8_t* mac, uint8_t* key, uint8_t* blob);

// check if id is within range of valid slots
bool is_valid_secrets_id(uint8_t id);

// get crc32 checksum over data in the given secrets slot in nvs
uint32_t get_secrets_crc32(uint8_t* mac, uint8_t* key, uint8_t* blob);

#endif /* CONFIG_SECRETS_H */

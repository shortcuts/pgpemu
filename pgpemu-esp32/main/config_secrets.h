#ifndef CONFIG_SECRETS_H
#define CONFIG_SECRETS_H

#include <stdbool.h>
#include <stdint.h>

// init the dedicated secrets partition; never erases it
bool init_secrets_partition();
// read secrets from nvs
bool read_secrets(char* name, uint8_t* mac, uint8_t* key, uint8_t* blob);
// reset loaded secrets in nvs
bool reset_secrets();

#endif /* CONFIG_SECRETS_H */

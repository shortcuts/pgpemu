#include "secrets.h"

char PGP_CLONE_NAME[] = { 0 };
uint8_t PGP_MAC[] = { 0 };
uint8_t PGP_BLOB[] = { 0 };
uint8_t PGP_DEVICE_KEY[] = { 0 };

bool PGP_VALID() {
    // check if mac is non-zero
    int sum = 0;
    for (int i = 0; i < 6; i++) {
        sum += PGP_MAC[i];
    }

    return sum != 0;
}
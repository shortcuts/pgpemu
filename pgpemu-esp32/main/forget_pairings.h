#pragma once

// Requests the erase and restarts; the erase itself runs at the next boot.
void forget_pairings_and_restart(void);
// Call first in app_main, before anything opens the default nvs partition.
void forget_pairings_if_requested(void);

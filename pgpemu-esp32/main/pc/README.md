# PC unit tests

Run `make test` from the repo root. It builds each `test_*.c` with `gcc -std=c99 -Istubs` and runs it. No ESP32 needed.

## Rules

- A test `#include`s the real firmware `.c` file it tests. Never re-declare firmware code in a test.
- `stubs/` holds ESP-IDF header stubs. `stubs/nvs_mem.h` is an in-memory NVS.
- A test defines the mocks for everything outside the module under test (GATT sends, timers, other modules).
- `test_pgp_control_protocol.c` reads the Control Service golden vectors in `protocol/control_vectors.txt` (repo root).
- `fixtures/led_patterns.h` holds LED writes per game event. They are synthetic: replace them with packet captures.

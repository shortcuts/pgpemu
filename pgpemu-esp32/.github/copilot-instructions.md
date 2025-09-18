# Copilot Instructions for `pgpemu-esp32`

## Project Overview
  - All main logic is in `main/` (e.g., `pgp_gatts.c`, `pgp_gap.c`, `pgp_handshake.c`).
 **Purpose:** Emulates a Pokémon GO Plus device on ESP32-C3 hardware, providing BLE, button, and LED functionality.
 **Structure:**
   - All main logic is in `main/` (e.g., `pgp_gatts.c`, `pgp_gap.c`, `pgp_handshake.c`).
  - Uses ESP-IDF build system (CMake/Makefile) and ESP-IDF APIs for BLE, WiFi, NVS, etc.
  - `build/` is generated; do not edit.

## Key Components
- **BLE GATT Server:** `pgp_gatts.c`, `pgp_gap.c`, `pgp_gatts_debug.c` handle BLE services and device advertising.
- **Button/LED:** `button_input.c`, `led_output.c`, `pgp_led_handler.c` manage hardware I/O.
 
- **Secrets/Settings:** `config_secrets.c`, `settings.c`, `nvs_helper.c` manage persistent storage in NVS.
- **Certificates:** `pgp_cert.c` and related files handle device certificates for authentication.
- **PC Test Harness:** `main/pc/` and `Makefile.test` allow building a test binary (`cert-test`) for handshake/cert logic on a PC.


## Build, Flash & Format
- **Build:** Use VS Code tasks or run `idf.py build` (see `.vscode/tasks.json`).
- **Flash:** Use `idf.py -p <PORT> flash` or the VS Code "Flash - Flash the device" task.
- **Monitor:** Use `idf.py -p <PORT> monitor` or the "Monitor: Start the monitor" task.
- **Clean:** Use `idf.py fullclean` or the "Clean - Clean the project" task.
- **Set Target:** Use the "Set ESP-IDF Target" task if switching ESP32 variants.
- **Format/Lint:** Run `make -f Makefile.format format` to apply `clang-format` to all C/H files in `main/` (uses `.clang-format` style).

## Project Conventions
- **All C source files in `main/` are auto-included in the build.**
- **Component registration:** See `main/CMakeLists.txt` and `main/component.mk` for build inclusion.
- **Logging:** Uses ESP-IDF logging (`esp_log.h`). Tag constants are in `log_tags.h`.
- **Secrets:** Never hardcode secrets; use NVS helpers and `config_secrets.h` API.
- **Partition Table:** Custom partition table in `partitions.csv`.
- **No C++/RTTI/exceptions:** Project is C-only, no C++ features enabled in config.
- **API Documentation:** Public APIs in headers (e.g., `nvs_helper.h`) use Doxygen-style comments for clarity.

## Integration & Patterns
- **BLE and WiFi run concurrently.** BLE GATT server is always active; WiFi captive portal is started for setup.
- **NVS is used for all persistent config/secrets.**
- **PC test harness** (`cert-test`) is for validating handshake/cert logic outside ESP32.
- **Unit tests for helpers:** Add C unit tests in `main/pc/` and build with `make -f Makefile.test test-nvs-helper` (see `main/pc/test_nvs_helper.c`).
- **All hardware abstraction is in `main/` (no `components/` folder used).**

## Examples
- To add a new BLE service: see `pgp_gatts.c` and follow the pattern for service/characteristic registration.
- To persist new config: add NVS helpers in `nvs_helper.c` and expose via `settings.c` or `config_secrets.c`.
- To add a new helper unit test: create a test in `main/pc/`, add a target to `Makefile.test`, and document in `main/pc/README.md`.

## Do/Don't
- **Do**: Use ESP-IDF APIs, follow file/module patterns, keep all logic in `main/`.
- **Do**: Use `clang-format` and Doxygen-style comments for new code/APIs.
- **Don't**: Add new folders at top-level, use C++/RTTI, hardcode secrets, or edit `build/`.

For more, see source comments in each file. Update this file if project structure or conventions change.

# Decide the fate of USB-Serial-JTAG console output after uart.c removal

Type: grilling
Status: resolved

## Question

`uart.c` both parses commands *and* backs the USB-Serial-JTAG driver that `esp_log` output rides on. "Fully remove UART control" was decided, but does passive log output (no commands accepted, just `ESP_LOGx` visibility over USB for development/flashing) survive, or does removing `uart.c` mean no USB console output at all? If logs survive, where does that driver init move (a new minimal module, or folded into an existing one) — per AGENTS.md, debug features must stay compile-time gated and modules stay single-responsibility.

## Answer

Log output survives for free — no new module needed. `sdkconfig` already sets `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` project-wide, which is ESP-IDF's own default console/log routing over USB-Serial-JTAG, installed at IDF startup independent of any app code. `uart.c`'s own `usb_serial_jtag_driver_install()` + `esp_vfs_usb_serial_jtag_use_driver()` calls only existed to swap in the interrupt-driven driver variant so it could also read incoming command bytes without blocking — that's purely in service of command parsing, which is being removed. `uart.c` is deleted entirely; nothing replaces it, nothing is compile-gated, `ESP_LOGx` keeps working unchanged.

Orphan note (not this ticket's scope, flagged for the implementation pass): `pgp_led_handler.c` includes `uart.h` but doesn't call anything from it — a pre-existing unused include, independent of this decision.

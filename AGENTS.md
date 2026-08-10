# AGENTS.md

## Scope

This repository targets **ESP32-C3** firmware built with **ESP-IDF v5.4.1**. The device implements **Bluetooth Low Energy (BLE)** pairing and behavior compatible with **Android** clients. The firmware **emulates the Pokémon GO Plus accessory**. This is non-negotiable: if a change weakens BLE fidelity, timing, or state accuracy, do not merge it.

## Tooling

* **Editor**: neovim. Anything else is rejected.
* **Build**: use the CLI only, never the VSCode ESP-IDF extension. Run `make build` to compile and `make clean` to fullclean. Verify every change builds before calling it done.
* **Flash**: default is NEVER run `idf.py flash`, `make monitor`, or touch real hardware. Exception: the user may explicitly instruct a flash/monitor in the current session (e.g. "flash it", "run make flash now") — only then run it, and only the command asked for. Without that explicit instruction, the maintainer flashes and tests on-device.
* **Formatting**: clang-format with project config. If formatting churn obscures diffs, you did it wrong.

## Architecture Rules

* Single-responsibility modules. BLE, storage, timing, and game-protocol logic are isolated.
* No dynamic allocation in hot paths. Heap churn breaks timing.
* ISR code is minimal. If it grows, refactor.
* State machines are explicit. Implicit state is banned.

## Logging and Debug

* Logs are concise and structured.
* No logs in tight loops.
* Debug features are compile-time gated.

## Testing

* Unit tests for protocol encoding/decoding.
* On-device integration tests for pairing and reconnection.
* Packet captures are the source of truth. If tests disagree with captures, tests are wrong.

## Code Review Bar

* BLE correctness > performance > readability.
* If you cannot explain a timing choice, remove it.
* Workarounds without root-cause analysis are rejected.

## Contribution Discipline

* Small, atomic commits.
* Commit messages state *what* and *why*.
* Breaking changes require migration notes.

## Decision Authority

* The maintainer has final say. If a change risks BLE parity or Android stability, the answer is no.

## Companion App (Kotlin/Android)

The `companion-app/` directory is a separate Kotlin/Jetpack Compose Gradle project (package `com.pgpemu.companion`). Use `make companion-*` targets (see Makefile) to build, install, and test it.

* Work is NOT complete until `make companion-test` passes. Run it after every set of edits, not just at the end.
* Fix every lint/build error before declaring done. Never suppress lint errors; if a rule is a genuine false positive, add an inline comment explaining why.
* Doc changes go in the same commit as the code change, not a follow-up.
* Never add co-authoring or "Claude-Sessions" trailers to commits.

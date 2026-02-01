# AGENTS.md

## Scope

This repository targets **ESP32-C3** firmware built with **ESP-IDF v5.4.1**. The device implements **Bluetooth Low Energy (BLE)** pairing and behavior compatible with **Android** clients. The firmware **emulates the Pokémon GO Plus accessory**. This is non-negotiable: if a change weakens BLE fidelity, timing, or state accuracy, do not merge it.

## Tooling

* **Editor**: neovim. Anything else is rejected.
* **Build**: esp-idf 5.4.1 only. Upgrades require a separate RFC.
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

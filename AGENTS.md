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

## Website (GitHub Pages)

Static documentation site at `docs/wiki/`, served at `pgpemu.shrtcts.fr` via GitHub Pages. Deployed automatically on push to `main` by `.github/workflows/pages.yml`. Run locally with:

```bash
make wiki-serve   # http://localhost:8080
```

### Structure

| File | Purpose |
|------|---------|
| `docs/wiki/index.html` | Overview + what you need + quick start |
| `docs/wiki/connect.html` | Pairing the companion app and the device with Pokémon GO |
| `docs/wiki/profiles.html` | Autospin / Autocatch per device profile |
| `docs/wiki/settings.html` | Advertising, max connections, log level |
| `docs/wiki/diagnostics.html` | Status, diagnostics dumps, danger zone |
| `docs/wiki/troubleshooting.html` | Connection and usage problems |
| `docs/wiki/changelog.html` | User-facing curated changelog |
| `docs/wiki/privacy.html` | Privacy policy |
| `docs/wiki/acknowledgements.html` | Open-source credits |
| `docs/wiki/style.css` | Single stylesheet — all pages share it |
| `docs/wiki/wiki-init.js` | Injects the sidebar nav, search, and heading anchors into every page at runtime |

### Maintaining content

Follow `docs/wiki/CONTRIBUTING.md` for page structure, nav ordering, and writing style. Wiki pages are for **app and device users, not developers** — no code symbols, opcode names, firmware internals, or build/contribution instructions.

- New user-visible companion app or device behavior → update the matching wiki page. Add a new page only if the change doesn't fit any existing one, and add it to `NAV_ITEMS` in `docs/wiki/wiki-init.js`.
- Wiki prose must pass the audience test in `docs/wiki/CONTRIBUTING.md`: could a non-technical user understand every sentence? If not, rewrite.
- No external resources beyond the DocSearch CDN script/stylesheet already in use — no other CDN fonts or JS libraries.

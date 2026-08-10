# Prototype the Companion App's screen/command coverage and destructive-action UX

Type: prototype
Status: resolved

## Question

Raise the fidelity of "full UART parity in the app" into a rough screen layout: which of the ~15 UART commands (see investigation list) map to a live status view vs an explicit action vs a settings toggle, and how destructive actions (secrets reset `xr`, restart `R`) get confirmed in-app. Use `/prototype` to produce a rough Compose screen/wireframe to react to, not a build-out.

## Answer

Prototype (HTML wireframe, 3 variants): https://claude.ai/code/artifact/4011ea4c-ae27-4a09-80bd-818934147b46 — throwaway, not committed to any branch (no app module exists yet to attach it to; ticket 09 will be the first real code).

- **Layout: single scrolling screen**, not tabbed. Sections ordered by risk/frequency: read-only Status (LED, advertising, connection count, log level) → Device Profiles (4-chip row, autospin/autocatch toggles per profile) → Settings (advertising toggle, max connections, save-to-device) → Diagnostics (stats/task-list/client-state string dumps, disconnect-all) → Danger zone pinned last (reset secrets, restart). One screen matches a single-device, single-module app — tab chrome buys nothing here.
- **Opcode-to-element mapping**: `GET_*` opcodes (0x02, 0x07, 0x09, 0x0A, 0x0D) back the read-only Status/Diagnostics cards, refreshed on connect and on-demand. `TOGGLE_AUTOSPIN`/`TOGGLE_AUTOCATCH` (0x10/0x11) and `ADVERTISE_START`/`STOP` (0x0B/0x0C) are toggles. `SET_MAX_CONNECTIONS` (0x0F) is a stepper/value row. `SAVE_SETTINGS` (0x03), `CYCLE_LOG_LEVEL` (0x08), `RESET_CLIENT_STATES` (0x0E) are explicit tap actions. `RESET_SECRETS` (0x05) and `RESTART` (0x06) are danger-zone actions. `HELP` (0x01) has no natural UI surface — not shown as a screen element, left for an in-app help/about string if ever needed.
- **Destructive-action confirmation**: bottom sheet with a plain-language consequence line naming the affected Device Profile, plus a **type-to-confirm** text gate (type `RESET` / `RESTART`) before the confirm button enables — not a plain Cancel/Confirm dialog. Matches the stakes: both actions are silent and irreversible over BLE, no undo.

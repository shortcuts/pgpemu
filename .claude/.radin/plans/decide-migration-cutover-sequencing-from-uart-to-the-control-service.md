# Plan: same-release paired cutover — delete uart.c, rewrite README usage docs

Scope: execute the resolved decision. Delete `uart.c`/`uart.h` and every
remaining call site, in the same commit as the README rewrite that replaces
the "Serial Menu Commands" doc with a Companion App usage guide. No code
review, no build — this plan only lists edits.

## Facts established (no open questions left for the executor)

- `uart.c` has exactly one caller outside itself: `pgpemu.c` (`#include
  "uart.h"` at line 14, `init_uart();` call at line 18, wrapped in a comment
  "// uart menu. put it first because it purges all logs").
- No other file calls into `uart.c`/`uart.h`. Checked `button_input.c`,
  `setup_button.c`, `pgp_led_handler.c`, `pgp_gatts.c` — zero references.
  (The one stray `#include "uart.h"` in `pgp_led_handler.c` was already
  removed in commit 1bb4e04, prior to this session.)
- `main/CMakeLists.txt` globs `*.c` (`FILE(GLOB CSources *.c)`) — deleting
  the two files is sufficient, no build-file edit needed.
- Log output over USB does **not** depend on `uart.c`. Per ticket
  `07-console-log-output-fate.md` (already resolved): `sdkconfig` sets
  `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` project-wide, which is ESP-IDF's own
  console/log routing, independent of app code. `uart.c`'s own
  `usb_serial_jtag_driver_install()` / `esp_vfs_usb_serial_jtag_use_driver()`
  calls exist only to enable non-blocking command *reads* — that's the part
  being deleted. `ESP_LOGx` output survives with zero code changes.
- `pgp_control.c:309` has a comment referencing "uart.c's 't' handler" as the
  historical rationale for a buffer size choice. This is a comment, not a
  call site or include — leave it untouched (it documents *why* a number was
  picked; still true after uart.c is gone). Do not touch this file.
- `companion-app/` is a real, working Compose app (`DeviceScreen.kt`,
  `DeviceViewModel.kt`, `ConfirmActionSheet.kt`) with full opcode coverage
  confirmed by reading the built screens — the README section below
  describes what's actually there, not a hypothetical.

## File-by-file changes

### 1. Delete `pgpemu-esp32/main/uart.c`

Full delete (`git rm`). Nothing replaces it — no new module, no
compile-time gate, per ticket 07's resolution.

### 2. Delete `pgpemu-esp32/main/uart.h`

Full delete (`git rm`). `uart.h` only declares `init_uart()` and
`process_char()`, both fully removed with `uart.c`. Confirmed dead: `grep -rn
"init_uart\|process_char" pgpemu-esp32/` outside `uart.c`/`uart.h` matches
only the one call site in `pgpemu.c` (item 3).

### 3. `pgpemu-esp32/main/pgpemu.c`

- Remove line 14: `#include "uart.h"`.
- Remove lines 17-18:
  ```c
  // uart menu. put it first because it purges all logs
  init_uart();
  ```
  Leave the blank line pattern matching surrounding style (don't leave a
  double blank line — collapse to the existing single blank line before
  `// set log levels which let init msgs through`).

### 4. `README.md` — Usage Guide rewrite (primary scope, per the resolved decision)

Replace the block from `### Serial Menu Commands` (line 143) through the end
of `### Per-Device UART Commands` (line 225, just before the `---` at line
227) — this is the full "describes the UART command surface" block under
`## Usage Guide`. Replace with a Companion App usage section, content
derived from what `companion-app/` actually renders (`DeviceScreen.kt`,
`build.gradle.kts`, ticket `08-app-screen-coverage-prototype.md`):

```markdown
### Companion App

Configuration and diagnostics are done from the **PGPemu Companion** Android
app (`companion-app/`) over BLE — the on-device serial menu has been
removed.

#### Install

The app isn't published; build and install it from source:

1. Open `companion-app/` in Android Studio (or run Gradle directly:
   `cd companion-app && ./gradlew installDebug`).
2. Requires Android 12 (API 31) or newer.
3. Install to a phone with Bluetooth Low Energy support.

#### Pair

1. Power on the PGPemu device.
2. Open the app and tap **Connect** — it scans for the device, connects over
   BLE, and completes bonding automatically.
3. Once connected, the screen switches from the Connect button to
   **Disconnect** and the sections below become visible.

#### Screens

The app is a single scrolling screen, ordered by risk/frequency:

- **Status** — read-only: LED state, advertising state, active connection
  count, log level.
- **Device Profiles** — one row per device slot (0-3), each with
  **Autospin** and **Autocatch** toggle chips.
- **Settings** — advertising on/off switch, max-connections stepper, a
  **Cycle log level** button, and **Save settings** to persist to flash.
- **Diagnostics** — on-demand dumps (runtime stats, FreeRTOS task list,
  BLE client states), each with its own **Refresh** button, plus
  **Disconnect all** clients.
- **Danger Zone** — **Reset secrets** and **Restart device**. Both open a
  confirmation sheet that names the affected device and requires typing
  `RESET` or `RESTART` before the action is enabled — these are silent,
  irreversible BLE operations with no undo.
```

### 5. `README.md` — mechanical fixes to structural references to `uart.c`

These describe the codebase's file layout as fact; after step 1/2 they'd be
wrong (naming a file that no longer exists). Small, mechanical, not a
content rewrite:

- Line 69 (`## Hardware Requirements`):
  `- **Serial Interface**: USB UART for configuration menu`
  → delete this line entirely (no longer a hardware-facing config
  interface; USB is still used for flashing/logs but that's already covered
  by the Installation section, not this bullet list).

- Lines 253-265 (Architecture ASCII diagram, `## Architecture` >
  `### System Overview`): remove the
  ```
      ├─ Serial Interface (uart.c)               │
      │  └─ Configuration menu                   │
      │                                           │
  ```
  block (currently lines 257-259). Leave the trailing `└─ Serial Monitor
  (console output)` line as-is (line 264) — that's the log-output path,
  which still exists per ticket 07's resolution.

- Lines 343-347 (`## Project Structure` tree, "Communication:" group):
  remove the `│   │   │   └── uart.c(.h)                   # Serial menu
  interface` line (347).

- Lines 392-395 (`## Code Organization` > `#### 1. Hardware Abstraction
  Layer (HAL)`): remove the `- \`uart.c\` - Serial communication` bullet
  (394).

- Lines 424-445 (`### Module Dependencies` tree): remove the
  ```
    ├─ uart.c
    │   └─ settings.c
    │   └─ config_secrets.c
    │
  ```
  block (currently lines 440-443).

### 6. Out of scope — flag only, do not edit

`README.md` still has stale UART references outside the block this task's
resolved decision named (`## Usage Guide` / "Serial Menu Commands"). The
resolution text scopes the rewrite to that one section; these others are
narrative walkthroughs, not structural facts, and rewriting them is a
materially bigger doc effort than "replace the Usage Guide section":

- `## Manual Testing Guide` → `### Part 3: Per-Device Settings Testing`
  (roughly lines 710-829): three test procedures (`Test 3.1`-`3.3`)
  instruct sending UART commands like `0s`/`1c` — these need rewriting into
  Companion App button-tap steps.
- `## Contributing` (roughly lines 1151-1206): a commit-message example
  mentions "the UART menu" (line 1157), and the "Adding New Settings"
  checklist says "Add UART command in `uart.c`" / "Document in serial menu
  help text (uart.c)" (lines 1203, 1205) — these should reference
  `pgp_control.c` and the Companion App instead.

Leave both as-is in this change. File a follow-up backlog entry
(`radin-record`, category `docs`/`chore`) titled something like "Rewrite
README Manual Testing Guide and Contributing UART references to match the
Companion App" so this doesn't get lost — it's real doc drift, just outside
this task's named scope.

## Order of operations

1. Delete `uart.c`, `uart.h` (item 1-2).
2. Edit `pgpemu.c` (item 3) — the build only stays green if this happens in
   the same commit as the deletions, since it's the only remaining caller.
3. Edit `README.md` (items 4-5) — no code dependency, can happen in any
   order relative to 1-2, but ship in the **same commit** per the resolved
   decision (no interim state).
4. One atomic commit for all of the above (AGENTS.md: small, atomic
   commits; this is one coherent cutover, not several).

## Verification

- `make format` (the only build-adjacent command this repo allows an agent
  to run per AGENTS.md) — confirms `pgpemu.c` still formats cleanly after
  the include/call removal.
- `grep -rn "uart\.h\|uart\.c\|init_uart\|process_char" pgpemu-esp32/` —
  expect zero matches (the `pgp_control.c:309` comment is prose, not a
  literal `uart.c` path match with this pattern... actually it does contain
  the literal substring `uart.c`, so expect exactly that one comment hit and
  nothing else). Confirms no orphaned references survive.
- `grep -n "uart\|UART" README.md` after editing — expect zero hits in the
  `## Usage Guide`, `## Architecture`, `## Project Structure`, `## Code
  Organization` sections; expect hits only inside the flagged-but-untouched
  Manual Testing Guide / Contributing sections (item 6), confirming those
  were deliberately left and not silently missed.
- No on-device build/flash — AGENTS.md forbids running esp-idf; the human
  maintainer builds and flashes separately.

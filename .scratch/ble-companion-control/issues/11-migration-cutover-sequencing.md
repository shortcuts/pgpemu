# Decide migration/cutover sequencing from UART to the Control Service

Type: grilling
Status: resolved

## Question

`uart.c` is fully removed (map decision), and the Companion App is a new artifact. Does a firmware release ship with UART already gone before the app exists (relying on the existing `?`/log commands being unavailable for a gap), or is there an interim state? What's the README/docs rewrite plan for the sections describing the removed UART command set?

## Answer

- **Cutover ordering**: same release, paired. One version bump removes `uart.c` and ships the Control Service; the Companion App ships alongside it. No interim state where a device has a control surface removed but no replacement — avoids stranding existing users with a console-only device (log output survives per ticket 07, but no commands) mid-migration.
- **README rewrite**: full rewrite, same PR as the `uart.c` removal. The "Serial Menu Commands" section (`README.md`, ~lines 141-200+, documents every UART command 1:1) is replaced with a Companion App usage guide (install/pair/screens, per ticket 08's prototype) in that same PR — the docs never describe a command surface that no longer exists.

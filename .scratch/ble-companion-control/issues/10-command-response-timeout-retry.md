# Decide timeout/retry semantics for the Command/Response exchange

Type: grilling
Status: resolved

## Question

Using ticket 04's protocol (one Command write, one Response indicate+read, `ERR_BUSY` if a command is already in flight) and ticket 06's `suspend fun sendCommand(...): Result<ResponseFrame>`: what timeout does the app apply while waiting for the Response indicate, what happens on timeout (retry once, surface an error, both?), and what's the recovery behavior if the BLE link disconnects mid-command (in-flight `sendCommand` call — does it complete with a failure `Result`, and does the firmware's in-flight command state get reset on disconnect so a reconnect doesn't see stale `ERR_BUSY`)?

## Answer

- **Timeout**: 5 seconds waiting for the Response indicate. No automatic retry — firmware commands aren't uniformly idempotent (`RESTART`, `RESET_CLIENT_STATES` have side effects), so a silent retry could double-fire a disconnect-causing command. `sendCommand` completes `Result.failure(TimeoutException)`; the UI surfaces the error and the user re-taps.
- **Mid-command disconnect**: the in-flight `sendCommand` call completes `Result.failure(...)` immediately when `ConnectionState` moves to `Disconnected` — it does not wait out the 5s timeout. One consistent failure path for the UI regardless of *why* the command didn't complete.
- **Firmware busy-state reset**: spec requirement — the Control Service's "command in flight" flag is explicitly cleared on `ESP_GATTS_DISCONNECT_EVT`, so a reconnect never inherits a stale `ERR_BUSY`. Same cleanup pattern as existing per-connection state handling in `pgp_gatts.c`. Flagged for the implementation pass, not blocking this spec.

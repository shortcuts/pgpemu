# Research BLE MTU and payload limits on ESP32-C3

Type: research
Status: resolved

## Question

What ATT MTU can the ESP32-C3 (ESP-IDF v5.4.1 BLE stack) realistically negotiate with an Android central, what's the max single notify/indicate payload at that MTU, and what's the idiomatic ESP-IDF pattern for fragmenting a response larger than one payload (e.g. the 256-byte Session Secrets blob, or a variable-length task-list dump) across multiple notifications with reassembly on the client side? Cite ESP-IDF API calls (`esp_ble_gap_set_prefer_default_mtu` / GATT MTU exchange event) and any existing precedent in this codebase's BLE stack (`pgp_gatts.c`, `pgp_bluetooth.c`).

## Answer

**Correction on the API name.** `esp_ble_gap_set_prefer_default_mtu` does not
exist in ESP-IDF v5.4.1. Grep of the vendored SDK
(`/Users/k/esp/v5.4.1/esp-idf/components/bt/`) finds no such symbol anywhere
in the tree. The real call is `esp_ble_gatt_set_local_mtu(uint16_t mtu)`,
declared in
`esp-idf/components/bt/host/bluedroid/api/include/api/esp_gatt_common_api.h:37`.
Its doc comment: "This function is called to set local MTU, the function is
called before BLE connection." This sets the device's own MTU cap; it does
not force the peer to use it.

**MTU ceiling and default.**
`esp_gatt_common_api.h:20-24` defines:
- `ESP_GATT_DEF_BLE_MTU_SIZE = 23` (the default/starting ATT MTU before any exchange)
- `ESP_GATT_MAX_MTU_SIZE = 517` (the hard ceiling `esp_ble_gatt_set_local_mtu` accepts)

This repo calls `esp_ble_gatt_set_local_mtu(500)` in
`pgpemu-esp32/main/pgp_bluetooth.c:105`, inside `pgp_ble_init` before
advertising starts. 500 is a safe value under the 517 cap, and matches the
`MAX_VALUE_LENGTH` (500) used for GATT attribute value sizing in
`pgp_gatts.c:36`.

**What actually gets negotiated with an Android central.** The GATT MTU
Exchange is peer-initiated in Bluedroid: whichever side sends the MTU
Exchange Request first proposes its value, and the negotiated MTU is
`min(local_mtu, peer_requested_mtu)`. The ESP32 side only exposes its
*ceiling* via `esp_ble_gatt_set_local_mtu`; it can't compel the Android
central to request a large MTU. Android's `BluetoothGatt` defaults to MTU 23
unless the app explicitly calls `requestMtu()`, at which point Android
typically requests up to 517. Real-world Android BLE stacks commonly settle
around 247–517 depending on device/chipset; 500-517 is realistic on modern
Android but not guaranteed — 23 (no payload beyond 20 bytes) remains a
legitimate fallback the firmware must tolerate.

Server-side, the negotiated MTU surfaces via `ESP_GATTS_MTU_EVT`
(`esp_gatts_api.h:24`, struct `gatts_mtu_evt_param` at line 104-107,
carrying `param->mtu.mtu`). This repo already logs it at
`pgp_gatts.c:658-660` (`ESP_GATTS_MTU_EVT` case) but does not currently act
on the negotiated value — it never resizes buffers or gates behavior based on
`param->mtu.mtu`.

**Max single notify/indicate payload at a given MTU.**
`esp_ble_gatts_send_indicate()` doc comment in `esp_gatts_api.h:488`:
"The size of indication or notification data must be less than or equal to
MTU size." Concretely this means `MTU - 3` usable payload bytes (3-byte ATT
header: 1-byte opcode + 2-byte attribute handle) — a well-known BLE/GATT
constant, consistent with the code comment at
`pgp_handshake.c:108` ("the size of notify_data[] need less than MTU size").
So at MTU 23 (unrequested default) the safe indicate/notify payload is 20
bytes; at MTU 500 it's 497 bytes.

**GATT-level attribute size ceiling.** Independent of MTU, the Bluedroid
stack itself caps a single characteristic attribute value at
`GATT_MAX_ATTR_LEN = 512` bytes
(`esp-idf/components/bt/host/bluedroid/stack/include/stack/gatt_api.h:142`).
This bounds what `esp_ble_gatts_set_attr_value()` can store per attribute
regardless of MTU.

**Existing precedent in this codebase: read-with-blob, not
notify-fragmentation.** This codebase does not fragment large payloads
across multiple notifications. Instead it uses GATT's built-in Read Blob
mechanism:

1. All attributes in `pgp_gatts.c` are declared `ESP_GATT_AUTO_RSP`
   (e.g. `IDX_CHAR_SFIDA_TO_CENTRAL_VAL` at `pgp_gatts.c:521-527`, permission
   `ESP_GATT_PERM_READ`, max length `MAX_VALUE_LENGTH` = 500). With
   auto-response mode, Bluedroid's GATT server handles `ATT_READ_BLOB_REQ`
   internally and transparently reassembles reads of attribute values larger
   than `MTU - 1` across multiple Read Blob request/response round trips —
   this is standard ATT protocol behavior, not app code.
2. `pgp_handshake.c:96-104` builds a 378-byte challenge blob
   (`struct challenge_data`) and stores it with
   `esp_ble_gatts_set_attr_value(certificate_handle_table[IDX_CHAR_SFIDA_TO_CENTRAL_VAL], 378, client_state->cert_buffer)`
   — no fragmentation performed by app code; the full 378 bytes are set on
   one attribute, safely under the 512-byte `GATT_MAX_ATTR_LEN` ceiling.
3. Immediately after, a tiny 4-byte "data ready" signal is pushed via
   `esp_ble_gatts_send_indicate(gatts_if, conn_id, certificate_handle_table[IDX_CHAR_SFIDA_COMMANDS_VAL], sizeof(notify_data), notify_data, false)`
   (`pgp_handshake.c:109-114`). This tiny indicate is guaranteed to fit even
   at MTU 23.
4. The Android client, on receiving the indicate, is expected to perform a
   GATT **Read** of `SFIDA_TO_CENTRAL` — the ESP-IDF/Bluedroid stack handles
   Read Blob chunking automatically regardless of negotiated MTU, up to the
   attribute's declared max length (500) and the stack ceiling
   (`GATT_MAX_ATTR_LEN` 512).

**Answer to the fragmentation question for future work (e.g. Session
Secrets blob or task-list dump).** Follow the codebase's existing precedent
rather than inventing a notify-chunking protocol:
- Store the payload (up to 500-512 bytes) via `esp_ble_gatts_set_attr_value`
  on a `ESP_GATT_AUTO_RSP` read characteristic — the stack fragments the read
  automatically via ATT Read Blob, independent of the negotiated MTU.
- Use a small indicate/notify (well under `MTU - 3`, e.g. the existing
  4-byte `notify_data` pattern) purely as a "data is ready, go read it"
  signal.
- If a payload can exceed 512 bytes (the hard `GATT_MAX_ATTR_LEN` ceiling),
  this read-blob approach alone is insufficient and would need an
  application-level chunking protocol (sequence header + reassembly) layered
  on top — there is no precedent for that in this codebase today, and none
  of the existing payloads (256-byte Session Secrets blob, 378-byte
  challenge) exceed 512 bytes.

**Sources cited:**
- `/Users/k/esp/v5.4.1/esp-idf/components/bt/host/bluedroid/api/include/api/esp_gatt_common_api.h` (lines 20-37) — MTU constants, `esp_ble_gatt_set_local_mtu`
- `/Users/k/esp/v5.4.1/esp-idf/components/bt/host/bluedroid/api/include/api/esp_gatts_api.h` (lines 24, 102-107, 488-497) — `ESP_GATTS_MTU_EVT`, `esp_ble_gatts_send_indicate` doc
- `/Users/k/esp/v5.4.1/esp-idf/components/bt/host/bluedroid/stack/include/stack/gatt_api.h:142` — `GATT_MAX_ATTR_LEN`
- `/Users/k/Documents/pgpemu/pgpemu-esp32/main/pgp_bluetooth.c:105-108` — `esp_ble_gatt_set_local_mtu(500)` call site
- `/Users/k/Documents/pgpemu/pgpemu-esp32/main/pgp_gatts.c:36, 514-527, 658-660` — `MAX_VALUE_LENGTH`, `IDX_CHAR_SFIDA_TO_CENTRAL_VAL` declaration, `ESP_GATTS_MTU_EVT` handling
- `/Users/k/Documents/pgpemu/pgpemu-esp32/main/pgp_handshake.c:96-114` — read-blob + tiny-indicate precedent for the 378-byte challenge blob

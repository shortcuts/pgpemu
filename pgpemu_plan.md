# Per-Device Settings Implementation Plan

## Executive Summary

The current pgpemu implementation supports 4 simultaneous device connections with per-device `autospin` and `autocatch` settings. However, there are two critical issues:

1. **Device Re-pairing Bug**: Devices lose their session on disconnect and must re-enter passphrases on reconnect
2. **Global Retoggle Settings**: The retoggle feature (currently disabled) operates globally instead of per-device

This plan addresses both issues by implementing persistent device identification and per-device retoggle controls.

---

## Problem Analysis

### Issue 1: Device Re-pairing on Reconnection

**Root Cause**: The reconnect key (`has_reconnect_key`) is stored in-memory only and lost on disconnect.

**Current Flow**:
1. Device connects → `ESP_GATTS_CONNECT_EVT` in `pgp_gatts.c:679`
2. Remote MAC stored in `client_state_t.remote_bda` (in-memory)
3. Device settings loaded by MAC from NVS
4. Handshake negotiates and generates `session_key` and `reconnect_challenge` (in-memory)
5. Device disconnects → `connection_stop()` calls `delete_client_state_entry()` which **zeroes the entire state**
6. Device reconnects → No state exists, full handshake required again

**The Fix**:
- Persist `session_key` and `reconnect_challenge` to NVS after successful handshake
- On reconnection, retrieve these keys from NVS using the MAC address
- Detect reconnecting device by comparing incoming MAC against stored MACs

**Implementation Level**: config_storage.c (2-3 new functions)

---

### Issue 2: Global Retoggle Settings

**Current State**: 
- Retoggle logic commented out in `pgp_led_handler.c:93-209`
- References non-existent `setting[conn_id]` array (line 92 comment: "// TODO: re-enable when setting[conn_id] available")
- Uses global `settings.autospin` and `settings.autocatch` instead of per-device settings

**Desired Behavior**:
- Bag is full (white LED) → Toggle device's autospin OFF for 120s, then ON
- Box is full (red LED) → Toggle device's autocatch OFF for 120s, then ON
- Pokéballs empty (red+off pattern) → Toggle device's autocatch OFF for 120s, then ON

**Current Architecture**: Already supports per-device settings!
- `client_state_t.settings` points to device-specific `DeviceSettings` struct
- Each connected device has independent `autospin`/`autocatch` toggles
- Settings are per-device in NVS, keyed by MAC address

**The Fix**:
1. Extend `DeviceSettings` struct with retoggle flags:
   - `autospin_retoggle_pending` (bool)
   - `autocatch_retoggle_pending` (bool)
2. Update `pgp_autosetting.c` to handle per-device delayed toggles
3. Uncomment and refactor `pgp_led_handler.c` to:
   - Use `client_state_t.settings->autospin` instead of global `settings.autospin`
   - Queue per-device retoggle using `conn_id`
4. Persist retoggle states to NVS (optional but recommended)

---

## Data Flow Analysis

### Current Settings Architecture

```
NVS (persistent storage)
  └─ device_settings/
      ├─ spin_<hashed_MAC>      → autospin (bool)
      ├─ catch_<hashed_MAC>     → autocatch (bool)
      └─ spinp_<hashed_MAC>     → autospin_probability (uint8)
         ↓
connected devices (in-memory)
  └─ client_state_t[] (4 slots)
      ├─ conn_id: BLE connection ID
      ├─ remote_bda: device MAC
      ├─ settings: pointer to DeviceSettings
      └─ (cryptographic keys - not persisted)
         ↓
LED event handlers
  └─ pgp_led_handler.c
      ├─ Reads client_state_t.settings->autospin
      ├─ Reads client_state_t.settings->autocatch
      └─ Queues actions via pgp_autobutton.c and pgp_autosetting.c
```

### Problem: Cryptographic Keys Not Persisted

```
Reconnection Scenario:
─────────────────────────

Device A connects (MAC: AA:BB:CC:DD:EE:FF)
  ↓
Handshake negotiates: session_key, reconnect_challenge
  → Stored in client_state_t[0] (in-memory only)
  ↓
Device A disconnects
  → delete_client_state_entry() zeros client_state_t[0]
  → session_key and reconnect_challenge are LOST
  ↓
Device A reconnects with same MAC
  → No cached session_key found
  → Full handshake required (device must enter passphrase again)
  → Creates new session_key and reconnect_challenge
```

---

## Implementation Plan

### Phase 1: Fix Device Reconnection (High Priority)

#### 1.1 Extend NVS Storage for Cryptographic Keys

**File**: `config_storage.h` (add function declarations)
**File**: `config_storage.c` (add implementations)

New functions needed:
```c
// After successful handshake, persist the session keys
bool persist_device_session_keys(esp_bd_addr_t bda, 
                                  const uint8_t* session_key,
                                  const uint8_t* reconnect_challenge);

// On reconnection attempt, retrieve cached keys
bool retrieve_device_session_keys(esp_bd_addr_t bda,
                                   uint8_t* session_key_out,
                                   uint8_t* reconnect_challenge_out);

// Check if device has cached session
bool has_cached_session(esp_bd_addr_t bda);

// Clear session on manual user request (optional)
bool clear_device_session(esp_bd_addr_t bda);
```

**Storage Keys** (in NVS "device_settings" partition):
- `sess_key_<hashed_MAC>` → session_key (16 bytes)
- `reco_chk_<hashed_MAC>` → reconnect_challenge (32 bytes)

**Reuse existing**: `make_device_key_for_option()` to generate hashed keys from MAC

#### 1.2 Update Handshake to Persist Keys

**File**: `pgp_handshake.c`

Locations where keys are generated:
- **Line 59-64**: Initial handshake generates `session_key`
- **Line 181**: `has_reconnect_key` is set to true

Changes needed:
1. After successful key generation (line 64), call:
   ```c
   persist_device_session_keys(client_state->remote_bda, 
                               client_state->session_key,
                               client_state->reconnect_challenge);
   ```
2. Add logging for persistence confirmation

#### 1.3 Update Connection Initialization

**File**: `pgp_handshake.c` → `handle_pgp_handshake_first()`

Before sending reconnect challenge (line 31-42):
```c
if (has_reconnect_key check fails) {
    if (has_cached_session(client_state->remote_bda)) {
        retrieve_device_session_keys(client_state->remote_bda,
                                     client_state->session_key,
                                     client_state->reconnect_challenge);
        client_state->has_reconnect_key = true;
    }
}
```

**Impact**: Minimal, non-breaking change. Falls back to full handshake if no cache exists.

---

### Phase 2: Implement Per-Device Retoggle (High Priority)

#### 2.1 Extend DeviceSettings Structure

**File**: `settings.h` (update struct)

```c
typedef struct {
    // existing fields...
    SemaphoreHandle_t mutex;
    esp_bd_addr_t bda;
    bool autocatch, autospin;
    uint8_t autospin_probability;
    
    // NEW: Retoggle state tracking
    bool autospin_retoggle_pending;
    bool autocatch_retoggle_pending;
    TickType_t autospin_retoggle_time;    // when to restore
    TickType_t autocatch_retoggle_time;   // when to restore
} DeviceSettings;
```

#### 2.2 Update pgp_autosetting.c for Per-Device Retoggle

**File**: `pgp_autosetting.c`

Current state: Has queue-based system but never used

Changes:
1. Update `setting_queue_item_t` struct (if needed) to include device context:
   ```c
   typedef struct {
       uint16_t conn_id;      // which device
       char setting;          // 's' or 'c'
       uint32_t delay;        // milliseconds
   } setting_queue_item_t;
   ```
2. In `autosetting_task()`:
   - Retrieve device settings via `get_client_state_entry(conn_id)->settings`
   - Toggle the correct device's setting
   - Persist changes to NVS

#### 2.3 Update LED Handler for Per-Device Awareness

**File**: `pgp_led_handler.c` (uncomment and refactor)

Changes at critical points:

**Line 102** (Bag is full - white LED):
```c
// OLD:
// ESP_LOGW(LEDHANDLER_TAG, "[%d] Bag is full: press button %s", 
//          conn_id, get_setting_log_value(&settings.autospin));

// NEW:
client_state_t* state = get_client_state_entry(conn_id);
if (state && state->settings) {
    ESP_LOGW(LEDHANDLER_TAG, "[%d] Bag is full: retoggling autospin", conn_id);
    retoggle_setting = 's';  // will be processed below
}
```

**Line 116** (Box is full - red LED):
```c
// OLD:
// ESP_LOGW(LEDHANDLER_TAG, "[%d] Box is full: press button %s", 
//          conn_id, get_setting_log_value(&settings.autocatch));

// NEW:
client_state_t* state = get_client_state_entry(conn_id);
if (state && state->settings) {
    ESP_LOGW(LEDHANDLER_TAG, "[%d] Box is full: retoggling autocatch", conn_id);
    retoggle_setting = 'c';
}
```

**Lines 196-209** (Queue retoggle):
Uncomment and update to use per-device settings:
```c
if (retoggle_setting != 0) {
    setting_queue_item_t item = {
        .conn_id = conn_id,
        .setting = retoggle_setting,
        .delay = retoggle_delay,
    };
    xQueueSend(setting_queue, &item, portMAX_DELAY);
}
```

**Lines 123-153** (Button press logic):
Update all global `settings` references to use `client_state_t->settings`:
```c
// OLD:
if (get_setting(&settings.autocatch)) { press_button = true; }

// NEW:
client_state_t* state = get_client_state_entry(conn_id);
if (state && state->settings && get_setting(&state->settings->autocatch)) {
    press_button = true;
}
```

---

### Phase 3: Persistence and Recovery (Medium Priority)

#### 3.1 Store Retoggle State

**File**: `config_storage.c`

Add NVS keys:
```c
static const char KEY_AUTOSPIN_RETOGGLE[] = "spinrt";
static const char KEY_AUTOCATCH_RETOGGLE[] = "catchrt";
```

Extend `read_stored_device_settings()` and `write_devices_settings_to_nvs()` to handle retoggle bools.

**Benefit**: Retoggle state survives device reboot (rare but good UX)

#### 3.2 Extend Logging and Diagnostics

**File**: `pgp_handshake_multi.c` → `dump_client_state()`

Add to dump output:
```c
"retoggle state:\n"
"- Autospin retoggle pending: %s\n"
"- Autocatch retoggle pending: %s\n",
// ...
entry->settings->autospin_retoggle_pending ? "yes" : "no",
entry->settings->autocatch_retoggle_pending ? "yes" : "no"
```

---

## Risk Analysis

### Phase 1 Risks (Session Key Persistence)
- **Risk**: NVS wear (keys written after each handshake)
  - **Mitigation**: Only write once after full handshake completes, not on reconnect
- **Risk**: Session key compromise if NVS is breached
  - **Mitigation**: Existing security model; keys are already sensitive
- **Risk**: Stale keys if user factory-resets device
  - **Mitigation**: User will need to re-enter passphrase once; then cached

### Phase 2 Risks (Per-Device Retoggle)
- **Risk**: Race condition if device disconnects during retoggle
  - **Mitigation**: `autosetting_task` checks `get_client_state_entry()` before accessing
- **Risk**: Retoggle queue overflow with 4 devices
  - **Mitigation**: Queue size is usually adequate; monitor with logging
- **Risk**: Concurrent access to DeviceSettings during retoggle
  - **Mitigation**: Mutex already in place in DeviceSettings struct

### Phase 3 Risks (Persistence)
- **Risk**: NVS capacity exceeded
  - **Mitigation**: Only 2 additional bool fields + timestamps per device (minimal)

---

## Implementation Checklist

### Phase 1 (Session Persistence)
- [ ] Add declarations to `settings.h` for new functions
- [ ] Implement `persist_device_session_keys()` in `config_storage.c`
- [ ] Implement `retrieve_device_session_keys()` in `config_storage.c`
- [ ] Implement `has_cached_session()` in `config_storage.c`
- [ ] Implement `clear_device_session()` in `config_storage.c`
- [ ] Update `pgp_handshake.c` line 64 area to persist keys
- [ ] Update `pgp_handshake.c` `handle_pgp_handshake_first()` to retrieve cached keys
- [ ] Test: Device disconnect/reconnect without passphrase prompt
- [ ] Test: Verify no breakage with new devices (no cached session case)

### Phase 2 (Per-Device Retoggle)
- [ ] Add retoggle fields to `DeviceSettings` struct in `settings.h`
- [ ] Verify `pgp_autosetting.c` compiles and functions work with per-device toggle
- [ ] Uncomment and refactor `pgp_led_handler.c` lines 102, 116, 196-209
- [ ] Update button press logic in `pgp_led_handler.c` lines 123-153
- [ ] Add includes if needed (`pgp_handshake_multi.h` for `get_client_state_entry()`)
- [ ] Test: Bag full scenario with Device 1 only → autospin toggles on Device 1
- [ ] Test: Box full scenario with Device 2 → autocatch toggles on Device 2, Device 1 unaffected
- [ ] Test: 4 devices simultaneously with mixed retoggle states

### Phase 3 (Persistence)
- [ ] Add NVS keys for retoggle state in `config_storage.c`
- [ ] Update `read_stored_device_settings()` to load retoggle bools
- [ ] Update `write_devices_settings_to_nvs()` to save retoggle bools
- [ ] Update `dump_client_state()` logging in `pgp_handshake_multi.c`
- [ ] Test: Settings preserved across power cycles

---

## Testing Strategy

### Unit Tests
1. Session key persistence/retrieval
2. Per-device setting retrieval in LED handler
3. Retoggle queue operations

### Integration Tests
1. **Single Device Re-pairing**: Connect → Disconnect → Reconnect (no passphrase)
2. **Multi-Device Mixed States**: 
   - Device A: Autospin ON, Autocatch OFF
   - Device B: Autospin OFF, Autocatch ON
   - Verify LED events trigger correct device toggles
3. **Retoggle Scenarios**:
   - Bag full → autospin OFF → wait 300ms → autospin ON
   - Box full → autocatch OFF → wait 300ms → autocatch ON
   - Verify only triggered device is affected
4. **Edge Cases**:
   - Device disconnects during retoggle
   - Retoggle requested when setting already OFF
   - NVS corruption scenario (graceful fallback)

---

## Future Enhancements

1. **Adjustable retoggle delay**: Make 300ms configurable per-device
2. **Retoggle disable per-device**: Allow user to disable auto-retoggle for specific device
3. **Smart retoggle**: Detect if user has manually toggled and don't auto-retoggle
4. **Metrics**: Track retoggle frequency per-device for analytics
5. **Session expiry**: Optionally expire cached sessions after X days

---

## Files Modified Summary

| File | Changes | Impact |
|------|---------|--------|
| `settings.h` | Add retoggle fields to DeviceSettings | Struct size +8 bytes |
| `config_storage.c` | Add 4 new functions for key persistence | +~100 lines, new NVS keys |
| `pgp_handshake.c` | Call persist/retrieve functions | +~10 lines |
| `pgp_led_handler.c` | Uncomment & refactor for per-device | +~20 lines modified |
| `pgp_autosetting.c` | Ensure per-device support (likely no changes) | 0-5 lines |

**Total new code**: ~150 lines
**Total modified lines**: ~50 lines
**Files modified**: 5


# PGPemu Manual Testing Guide

## Overview

This guide covers manual device testing for the per-device settings and session key caching features. After PC unit tests pass, use these procedures to validate the features on actual hardware.

## Prerequisites

- ESP32 device (flashed with latest PGPemu firmware)
- Pokemon Go app installed on smartphone
- Bluetooth-capable smartphone
- Serial monitor (e.g., `idf.py monitor` or serial console)
- Optional: 2-4 additional devices for multi-device testing

## Part 1: Build & Flash

### Step 1: Compile the Project

```bash
cd pgpemu-esp32
idf.py build
```

Watch for compilation errors and warnings. Known issues:
- TickType_t format specifier warnings in pgp_handshake_multi.c (non-critical on ESP32)

### Step 2: Flash to Device

```bash
idf.py flash -p /dev/ttyUSB0 -b 921600  # Adjust port as needed
```

### Step 3: Monitor Serial Output

```bash
idf.py monitor -p /dev/ttyUSB0
```

Keep this running during all test procedures to observe device behavior.

---

## Part 2: Session Key Persistence Testing

### Test 2.1: Single Device Reconnection Without Passphrase

**Objective:** Verify that a device can reconnect without re-entering the passphrase.

**Setup:**
1. Flash device and open serial monitor
2. Let device boot and show startup logs
3. Open Pokemon Go app

**Procedure:**
1. **Initial Connection:**
   - Scan for PGPemu in Pokemon Go Bluetooth settings
   - Select PGPemu device
   - Enter passphrase when prompted
   - Observe serial logs showing successful handshake (approx 3-5 seconds)
   - Connection should establish and device should show "connected" status

2. **Disconnect:**
   - In Pokemon Go, disconnect from PGPemu
   - Wait 2 seconds
   - Observe device logs: you should see "connection_stop" message

3. **Reconnect:**
   - In Pokemon Go, reconnect to PGPemu (don't scan for new device, select previously paired)
   - **DO NOT enter passphrase** - device should use cached session key
   - Observe serial logs showing expedited handshake (< 1 second)
   - Connection should establish without passphrase prompt

**Expected Outcome:**
- First connection: Full handshake with passphrase, ~3-5 seconds
- Second connection: Session key reuse, ~1 second, NO passphrase needed
- Serial output shows:
  ```
  [handshake] Attempting to retrieve cached session keys...
  [handshake] Session keys found in NVS, skipping full handshake
  ```

**Pass Criteria:** ✓ Device reconnects in <2 seconds without passphrase

---

### Test 2.2: Session Key Persistence Across Power Cycle

**Objective:** Verify that session keys persist after device power cycle.

**Setup:**
1. Complete Test 2.1 successfully
2. Device should be connected

**Procedure:**
1. **Establish and Record State:**
   - Verify device is connected with cached session key
   - Note the device's MAC address from logs (format: `AA:BB:CC:DD:EE:FF`)

2. **Power Cycle Device:**
   - Disconnect from Pokemon Go
   - Unplug device
   - Wait 5 seconds
   - Plug device back in
   - Wait for device to boot (observe startup logs)

3. **Verify Cache Persistence:**
   - Open Pokemon Go
   - Reconnect to PGPemu
   - Should reconnect without passphrase in <2 seconds
   - Check NVS logs showing cache retrieval

**Expected Outcome:**
- Session key remains in NVS storage after power cycle
- Reconnection succeeds without passphrase
- Serial logs show cache hit after boot

**Pass Criteria:** ✓ Session persists across power cycles, no passphrase required

---

### Test 2.3: MAC Address Mismatch Detection

**Objective:** Verify that cached keys are device-specific (MAC-based).

**Setup:**
1. Device A flashed and tested (has cached session keys)
2. Device B (different device, different MAC address) flashed

**Procedure:**
1. **Attempt Cross-Device Reconnection:**
   - Device A previously paired and cached
   - Attempt to use cached key from Device A on Device B (not possible directly, but test MAC isolation)
   - If possible in setup, swap firmware images to verify device isolation

2. **Verify Device B Requires Passphrase:**
   - Device B should not have cached keys
   - Should prompt for passphrase on first connection
   - Handshake should be full (~3-5 seconds)

**Expected Outcome:**
- Each device maintains independent cache
- Device MAC address is used to generate cache key
- Different MACs don't share cached session keys

**Pass Criteria:** ✓ Devices remain isolated, each requires independent passphrase

---

## Part 3: Per-Device Settings Testing

### Test 3.1: Single Device Settings Persistence

**Objective:** Verify that device settings persist independently.

**Setup:**
1. Single device flashed and connected
2. Serial monitor active

**Procedure:**
1. **Configure Device Settings via UART:**
   - Open serial monitor (may also use web interface if available)
   - Send UART command: `0s` (toggle autospin for device 0)
   - Observe logs showing toggle and NVS write
   - Send UART command: `0p5` (set autospin probability to 50%)
   - Observe probability set in logs

2. **Disconnect and Reconnect:**
   - Disconnect from Pokemon Go
   - Wait 2 seconds
   - Reconnect to Pokemon Go

3. **Verify Settings Loaded:**
   - Connected device should show saved autospin state
   - Check serial logs confirming loaded settings
   - Settings should match what was configured in step 1

4. **Modify and Verify Again:**
   - Send: `0c` (toggle autocatch)
   - Disconnect/reconnect
   - Autocatch state should persist

**Expected Outcome:**
- Settings saved to NVS after each UART command
- Settings loaded when device connects
- Persists across disconnect/reconnect cycles

**Pass Criteria:** ✓ All device settings persist independently

---

### Test 3.2: Multi-Device Independent Settings

**Objective:** Verify that 4 devices can have different settings simultaneously.

**Setup:**
1. 4 devices flashed (or simulate with controlled connections)
2. All devices within range

**Procedure:**
1. **Initial Configuration:**
   - Device 0: Autospin ON, Probability 5
   - Device 1: Autospin OFF, Probability 3
   - Device 2: Autospin ON, Probability 8
   - Device 3: Autospin OFF, Probability 1
   
   Configure via UART:
   ```
   0s  (toggle autospin device 0, becomes ON)
   0p5
   1s  (toggle autospin device 1, becomes OFF)
   1p3
   2p8
   3s  (toggle autospin device 3, becomes OFF)
   3p1
   ```

2. **Verify Independent Operation:**
   - Connect Device 0 to Pokemon Go → autospin ON, prob=5
   - Disconnect, connect Device 1 → autospin OFF, prob=3
   - Each device should show its own settings
   - Check serial logs confirming per-device loading

3. **Simultaneous Multi-Device:**
   - Connect up to 4 devices simultaneously to Pokemon Go
   - Each device should operate with its independent settings
   - No cross-contamination of settings between devices

**Expected Outcome:**
- Each device slot maintains independent settings
- Settings loaded correctly when device connects
- No interference between simultaneous connections

**Pass Criteria:** ✓ 4 devices operate simultaneously with independent settings

---

### Test 3.3: Settings Isolation After Settings Changes

**Objective:** Verify toggling one device's settings doesn't affect others.

**Setup:**
1. 2+ devices connected
2. Device 0: Autospin ON
3. Device 1: Autospin OFF

**Procedure:**
1. **Record Initial States:**
   - Observe device 0 autospin: ON
   - Observe device 1 autospin: OFF

2. **Toggle Device 0:**
   - Send UART: `0s` (toggle device 0 autospin OFF)
   - Observe: Device 0 now OFF

3. **Verify Device 1 Unchanged:**
   - Check device 1 autospin status: should still be OFF
   - Verify logs show no changes to device 1

4. **Toggle Device 1:**
   - Send UART: `1s` (toggle device 1 autospin ON)
   - Verify device 0 still OFF, device 1 now ON

**Expected Outcome:**
- Settings mutations are device-specific
- No cross-device interference
- Each device maintains independent mutex/locks

**Pass Criteria:** ✓ Settings changes isolated between devices

---

## Part 4: Retoggle Feature Testing

### Test 4.1: Bag Full Retoggle (White LED)

**Objective:** Verify autospin is toggled OFF then ON when bag is full.

**Setup:**
1. Device connected to Pokemon Go
2. Pokemon Go app running with gotcha autospin enabled
3. Fill bag in game (triggers white LED on device)

**Procedure:**
1. **Normal Operation:**
   - Play normally, autospin toggling Pokemon
   - Observe device LED changes

2. **Trigger Bag Full:**
   - Collect items until bag is full
   - Pokemon Go app will send bag-full signal
   - Device shows white LED indicator

3. **Observe Retoggle:**
   - Device receives LED pattern
   - Autospin toggles OFF (stops catching)
   - After 300ms, autospin toggles back ON
   - Behavior: Device stops catching momentarily, then resumes
   - Check serial logs showing retoggle queue entry/processing

**Expected Outcome:**
- White LED triggers autospin OFF → ON sequence
- 300ms delay between OFF and ON
- Retoggle only affects the connected device
- Other devices (if connected) unaffected

**Pass Criteria:** ✓ Autospin retoggle correctly handles bag full

---

### Test 4.2: Box Full Retoggle (Red Solid LED)

**Objective:** Verify autocatch is toggled when item box is full.

**Setup:**
1. Device connected to Pokemon Go
2. Autocatch enabled
3. Fill item box in game

**Procedure:**
1. **Fill Item Box:**
   - Collect items until item box is full
   - Pokemon Go sends box-full signal
   - Device shows red solid LED

2. **Observe Retoggle:**
   - Device receives LED pattern
   - Autocatch toggles OFF (stops catching items)
   - After 300ms, autocatch toggles back ON
   - Observe device behavior: stops catching momentarily, resumes

3. **Verify Persistence:**
   - Disconnect and reconnect device
   - Retoggle settings should be cleared (not persistent on reconnect)

**Expected Outcome:**
- Red solid LED triggers autocatch OFF → ON
- 300ms delay
- Only affects current device
- Retoggle clears after device disconnects

**Pass Criteria:** ✓ Autocatch retoggle correctly handles box full

---

### Test 4.3: Empty Pokéballs Retoggle (Red Blinking LED)

**Objective:** Verify autocatch toggles when pokéballs empty.

**Setup:**
1. Device connected with autocatch enabled
2. Use up all pokéballs in game

**Procedure:**
1. **Empty Pokéballs:**
   - Use all pokéballs catching Pokemon
   - Pokemon Go sends pokéballs-empty signal
   - Device shows red blinking LED

2. **Observe Retoggle:**
   - Autocatch toggles OFF
   - After 300ms, toggles back ON
   - Behavior: Device stops catching, resumes

3. **Multi-Device Verification:**
   - If 2+ devices connected, verify other devices unaffected
   - Only the device receiving the signal retoggle

**Expected Outcome:**
- Red blinking LED triggers retoggle
- Isolated to receiving device
- Settings return to previous state after 300ms

**Pass Criteria:** ✓ Pokéballs empty retoggle works correctly

---

## Part 5: Multi-Device Scenario Testing

### Test 5.1: Four Devices with Different Settings

**Objective:** Verify full feature set with 4 devices.

**Setup:**
1. 4 devices available
2. Each configured with unique settings:
   - Device 0: Autospin ON, Autocatch OFF, Prob=2
   - Device 1: Autospin OFF, Autocatch ON, Prob=5
   - Device 2: Autospin ON, Autocatch ON, Prob=8
   - Device 3: Autospin OFF, Autocatch OFF, Prob=0

**Procedure:**
1. **Configure All Devices:**
   ```bash
   0s     # Device 0: autospin ON
   0p2
   1s     # Device 1: autospin OFF
   1c     # Device 1: autocatch ON
   1p5
   2p8
   3s     # Device 3: autospin OFF
   3c     # Device 3: autocatch OFF
   ```

2. **Simultaneous Connection:**
   - Connect all 4 devices to Pokemon Go simultaneously
   - Verify all connect without passphrase (cached keys)
   - Settings loaded correctly for each

3. **Mixed Operations:**
   - Bag fills on Device 0 → only autospin retoggle
   - Pokéballs empty on Device 2 → only autocatch retoggle
   - Verify other devices unaffected
   - Each device operates independently

4. **Disconnect One:**
   - Disconnect Device 1
   - Active connections count: 3
   - Remaining devices continue operating

5. **Reconnect:**
   - Reconnect Device 1
   - Uses cached session key
   - Settings loaded: autocatch ON, prob=5
   - No passphrase required

**Expected Outcome:**
- 4 devices with independent settings
- All share session persistence feature
- Retoggle isolated per device
- Settings survive reconnections

**Pass Criteria:** ✓ Full feature integration works with 4 devices

---

## Part 6: Edge Cases & Stress Testing

### Test 6.1: Rapid Disconnect/Reconnect

**Objective:** Verify stability during rapid connection cycles.

**Procedure:**
1. Connect device
2. Immediately disconnect (< 1 second)
3. Immediately reconnect
4. Repeat 5 times
5. Observe no crashes or memory leaks

**Expected Outcome:**
- Device handles rapid reconnects gracefully
- No log errors or crashes
- Session keys not corrupted

**Pass Criteria:** ✓ Device stable under rapid reconnects

---

### Test 6.2: Connection at Max Capacity

**Objective:** Verify 4-device limit enforcement.

**Procedure:**
1. Connect 4 devices
2. Attempt to connect 5th device
3. Observe connection rejection

**Expected Outcome:**
- 5th device fails to connect
- Clear error message in logs
- Connected 4 devices unaffected

**Pass Criteria:** ✓ Max connection limit enforced

---

### Test 6.3: Very Long Sessions

**Objective:** Verify stability over extended runtime.

**Procedure:**
1. Connect device
2. Let device run for 1 hour with normal Pokemon Go usage
3. Monitor serial logs for errors
4. Observe NVS operations

**Expected Outcome:**
- No memory leaks
- No NVS corruption
- Settings remain consistent

**Pass Criteria:** ✓ Device stable over long sessions

---

## Part 7: UART Command Reference

### Device Configuration Commands

Format: `<device_idx><command>[<value>]`

- `<device_idx>`: 0-3 for device 0-3
- `<command>`:
  - `s`: Toggle autospin
  - `c`: Toggle autocatch
  - `p<value>`: Set autospin probability (0-9)
  - `l`: Cycle log level

### Examples

```
0s     → Toggle autospin on device 0
1c     → Toggle autocatch on device 1
2p5    → Set device 2 probability to 50%
0l     → Cycle log level (1→2→3→1)
```

---

## Test Summary Checklist

- [ ] Test 2.1: Single device reconnection without passphrase
- [ ] Test 2.2: Session key persistence across power cycle
- [ ] Test 2.3: MAC address mismatch detection
- [ ] Test 3.1: Device settings persistence
- [ ] Test 3.2: Multi-device independent settings
- [ ] Test 3.3: Settings isolation
- [ ] Test 4.1: Bag full retoggle
- [ ] Test 4.2: Box full retoggle
- [ ] Test 4.3: Empty pokéballs retoggle
- [ ] Test 5.1: Four devices with different settings
- [ ] Test 6.1: Rapid disconnect/reconnect
- [ ] Test 6.2: Connection at max capacity
- [ ] Test 6.3: Very long sessions

---

## Troubleshooting

### Device won't connect first time
- Verify UART commands don't show connection errors
- Check NVS is initialized correctly
- Verify Pokemon Go has bluetooth permissions

### Session key not cached
- Check NVS partition has space
- Verify handshake completes fully (3-5 second timeout)
- Check logs for "persist_device_session_keys" messages

### Settings not loaded on reconnect
- Verify device MAC address correct in logs
- Check NVS key generation (should be 15 hex chars)
- Ensure `read_stored_device_settings()` called on connect

### Retoggle not working
- Verify LED pattern detection in pgp_led_handler.c
- Check retoggle queue has space
- Ensure autosetting_task is running

### Multi-device conflicts
- Verify conn_id_map not corrupting
- Check each device has independent DeviceSettings struct
- Ensure mutex protects device settings

---

## Success Criteria

All tests in the checklist must pass with:
- No crashes or watchdog resets
- No memory leaks
- Clear, informative log messages
- Expected behavior without manual intervention
- Features work on all 4 device slots
- Settings persist across power cycles

# PGPemu Refactoring Guide

This document outlines code refactoring opportunities identified during codebase analysis. These refactorings would improve code maintainability, reduce duplication, and improve consistency without changing functionality.

## Overview

- **Total Lines Analyzed**: 2,199 lines
- **Duplicate Code Found**: 220+ lines (~10% of codebase)
- **High-Impact Opportunities**: 8 major patterns identified
- **Estimated Effort**: 7-11 hours for full implementation
- **Risk Level**: LOW (comprehensive test suite exists)

---

## Priority 1: Mutex Pattern Extraction (HIGH IMPACT)

### Current State

Mutex acquisition and release appears in 12+ locations with inconsistent patterns:

**Pattern 1: Blocking acquisition**
```c
// config_storage.c:55
if (!xSemaphoreTake(global_settings.mutex, portMAX_DELAY)) {
    ESP_LOGE(CONFIG_STORAGE_TAG, "cannot get global settings mutex");
    return;
}
// ... critical section ...
xSemaphoreGive(global_settings.mutex);
```

**Pattern 2: Timeout acquisition**
```c
// pgp_handshake_multi.c:31
if (active_connections_mutex && xSemaphoreTake(active_connections_mutex, 100 / portTICK_PERIOD_MS)) {
    count = active_connections;
    xSemaphoreGive(active_connections_mutex);
}
```

**Pattern 3: Conditional acquisition**
```c
// settings.c:19
xSemaphoreTake(global_settings.mutex, portMAX_DELAY);
// ... critical section ...
xSemaphoreGive(global_settings.mutex);
```

### Recommended Refactoring

Create `mutex_helpers.h` with helper functions:

```c
// mutex_helpers.h

inline static bool mutex_acquire_blocking(SemaphoreHandle_t mutex) {
    if (!mutex) return false;
    return xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE;
}

inline static bool mutex_acquire_timeout(SemaphoreHandle_t mutex, uint32_t timeout_ms) {
    if (!mutex) return false;
    return xSemaphoreTake(mutex, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
}

inline static void mutex_release(SemaphoreHandle_t mutex) {
    if (mutex) xSemaphoreGive(mutex);
}

// Usage:
// if (mutex_acquire_blocking(my_mutex)) {
//     // Critical section
//     mutex_release(my_mutex);
// }
```

### Impact

- **Lines Removed**: ~40 lines
- **Consistency**: All mutex operations follow same pattern
- **Safety**: NULL checks built into helpers
- **Readability**: Intent is clearer with function names

### Implementation Steps

1. Create `pgpemu-esp32/main/mutex_helpers.h`
2. Update 6 files to use new helpers:
   - config_storage.c (lines 55, 95, 152, 167, 200, 207, 224, 272, 281, 298)
   - pgp_handshake_multi.c (lines 31, 127, 159)
   - settings.c (lines 19, 23, 27)
3. Run full test suite to verify no regressions
4. Commit with message: "refactor: Extract mutex operations into helper functions"

### Risk Assessment

- **Code Coverage**: HIGH - All mutex operations are covered by tests
- **Complexity**: LOW - Simple extraction, no logic changes
- **Testing**: Existing tests verify correctness

---

## Priority 2: NVS Handle Management (HIGH IMPACT)

### Current State

NVS handle opening and closing appears in 8 functions with ~150 lines of near-duplicate code:

**Pattern in config_storage.c**
```c
// Lines 61-74, 153-165, 207-216
nvs_handle_t handle = {};
esp_err_t err = nvs_open("partition_name", NVS_READONLY, &handle);
if (err != ESP_OK) {
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "partition doesn't exist, using default settings");
    } else {
        ESP_ERROR_CHECK(err);
    }
    if (use_mutex) {
        xSemaphoreGive(mutex);
    }
    return;
}
// ... use handle ...
nvs_close(handle);
```

This pattern repeats with variations in:
- `read_global_settings_from_nvs()` (lines 49-99)
- `read_stored_device_settings()` (lines 114-205)
- `write_stored_device_settings()` (lines 226-305)
- And more utility functions

### Recommended Refactoring

Create `nvs_helpers.h` with helper functions:

```c
// nvs_helpers.h

/**
 * @brief Open NVS partition for reading
 * @return Handle if successful, empty handle {} if failed
 */
inline static nvs_handle_t nvs_open_readonly(const char* namespace, const char* tag) {
    nvs_handle_t handle = {};
    esp_err_t err = nvs_open(namespace, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(tag, "nvs_open failed for %s: %s", namespace, esp_err_to_name(err));
        }
        return (nvs_handle_t){};
    }
    return handle;
}

/**
 * @brief Safe NVS close (no-op if handle is invalid)
 */
inline static void nvs_close_safe(nvs_handle_t handle) {
    if (handle != 0) {
        nvs_close(handle);
    }
}
```

### Impact

- **Lines Removed**: ~80-100 lines  
- **Consistency**: All NVS opens follow same error handling
- **Safety**: Error codes consistently checked
- **Readability**: Clearer intent with function names

### Implementation Steps

1. Create `pgpemu-esp32/main/nvs_helpers.h`
2. Refactor `config_storage.c` functions:
   - `read_global_settings_from_nvs()` 
   - `read_stored_device_settings()`
   - `write_stored_device_settings()`
   - And related functions
3. Run full test suite
4. Commit with message: "refactor: Extract NVS handle operations into helpers"

### Risk Assessment

- **Code Coverage**: HIGH - NVS operations tested
- **Complexity**: MEDIUM - Multiple functions with variations
- **Testing**: Existing tests verify persistence

---

## Priority 3: Connection State Lookup (LOW COMPLEXITY)

### Current State

Looking up connection state by conn_id appears in 4+ locations:

**Pattern in pgp_handshake_multi.c**
```c
// Lines 42-49, 82-90, 116-122, etc.
client_state_t* entry = NULL;
for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (conn_id_map[i] == conn_id) {
        entry = &client_states[i];
        break;
    }
}
if (!entry) {
    return NULL;
}
```

### Recommended Refactoring

Create helper function:

```c
// pgp_handshake_multi.c

/**
 * @brief Find client state index by connection ID
 * @return Index 0-3 if found, -1 if not found
 */
static int _find_client_index(uint16_t conn_id) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conn_id_map[i] == conn_id) {
            return i;
        }
    }
    return -1;
}

// Then refactor existing functions to use it:
client_state_t* get_client_state_entry(uint16_t conn_id) {
    int index = _find_client_index(conn_id);
    return (index >= 0) ? &client_states[index] : NULL;
}
```

### Impact

- **Lines Removed**: ~30 lines
- **Consistency**: Single canonical lookup path
- **Readability**: Clear intent vs. loop duplication
- **Performance**: No change (same algorithm)

### Implementation Steps

1. Add `_find_client_index()` helper to pgp_handshake_multi.c
2. Refactor 4 functions to use new helper
3. Run tests to verify
4. Commit with message: "refactor: Extract connection lookup into helper"

### Risk Assessment

- **Code Coverage**: HIGH - All paths tested
- **Complexity**: LOW - Simple extraction
- **Testing**: No behavioral changes

---

## Priority 4: Task/Queue Initialization (MEDIUM COMPLEXITY)

### Current State

Task and queue initialization patterns are duplicated in `pgpemu.c`:

```c
// pgpemu.c - similar patterns for multiple tasks
xTaskCreate(led_handler_task, "led_handler", 
            2048, NULL, 5, NULL);

xTaskCreate(autobutton_task, "autobutton",
            2048, NULL, 5, NULL);

xTaskCreate(autosetting_task, "autosetting",
            2048, NULL, 5, NULL);
```

### Recommended Refactoring

Create helper macro:

```c
#define CREATE_TASK(name, func, stack, priority) \
    do { \
        if (xTaskCreate((func), (name), (stack), NULL, (priority), NULL) != pdPASS) { \
            ESP_LOGE(TAG, "Failed to create task: %s", (name)); \
        } \
    } while(0)

// Usage:
// CREATE_TASK("led_handler", led_handler_task, 2048, 5);
```

### Impact

- **Lines Removed**: ~20 lines
- **Consistency**: Uniform error handling
- **Maintainability**: Single place to change error behavior

### Implementation Steps

1. Add macro to pgpemu.c
2. Replace 3-4 task creation sites
3. Run full test suite
4. Commit

### Risk Assessment

- **Complexity**: LOW
- **Testing**: No logic changes, existing tests verify

---

## Priority 5: Device Settings Access Pattern (MEDIUM)

### Current State

Per-device settings access with mutex protection repeats in:
- pgp_autobutton.c 
- pgp_autosetting.c
- pgp_led_handler.c

### Recommended Refactoring

Create helper function in settings.c:

```c
// settings.h

/**
 * @brief Execute function while holding device settings mutex
 * @param device_settings Pointer to DeviceSettings
 * @param func Function to execute (takes DeviceSettings* param)
 * @return Result from func, or false if mutex timeout
 */
typedef bool (*settings_func_t)(DeviceSettings*);
bool with_device_settings_locked(DeviceSettings* settings, settings_func_t func);
```

This allows:
```c
// Before:
if (xSemaphoreTake(device->settings->mutex, portMAX_DELAY)) {
    device->settings->autospin = !device->settings->autospin;
    xSemaphoreGive(device->settings->mutex);
}

// After:
with_device_settings_locked(device->settings, toggle_autospin_callback);
```

### Impact

- **Lines Removed**: ~50 lines
- **Safety**: Consistent mutex handling
- **Readability**: Clear intent

### Implementation Steps

1. Add helper to settings.c/h
2. Refactor 3 files to use helper
3. Test
4. Commit

---

## Priority 6: NVS Read/Write Validation (MEDIUM)

### Current State

NVS read/write validation appears in `nvs_helper.c` and is called consistently, but similar patterns repeat:

```c
// Multiple locations
err = nvs_get_u8(handle, key, &value);
if (nvs_read_check(tag, err, key)) {
    // Use value
}

err = nvs_set_u8(handle, key, value);
if (nvs_write_check(tag, err, key)) {
    // Success
}
```

### Recommended Refactoring

Create typed helper macros:

```c
#define NVS_READ_U8(handle, key, out_ptr, tag) \
    nvs_read_check((tag), nvs_get_u8((handle), (key), (out_ptr)), (key))

#define NVS_WRITE_U8(handle, key, value, tag) \
    nvs_write_check((tag), nvs_set_u8((handle), (key), (value)), (key))
```

This reduces:
```c
// From:
err = nvs_get_u8(handle, KEY_AUTOCATCH, &autocatch);
if (nvs_read_check(CONFIG_STORAGE_TAG, err, KEY_AUTOCATCH)) {
    // use autocatch
}

// To:
if (NVS_READ_U8(handle, KEY_AUTOCATCH, &autocatch, CONFIG_STORAGE_TAG)) {
    // use autocatch
}
```

### Impact

- **Lines Removed**: ~30 lines
- **Safety**: Fewer opportunities to miss error checks
- **Consistency**: All operations follow same pattern

---

## Priority 7: Debug/Status Output Consolidation (LOW)

### Current State

Debug output for listing client states appears in:
- `pgp_handshake_multi.c::debug_dump_client_states()` (lines 168-210)
- Similar code in `uart.c` for status output

### Recommended Refactoring

Extract common output formatting into shared function.

### Impact

- **Lines Removed**: ~20-30 lines
- **Consistency**: Single source for client state display

---

## Priority 8: Connection Index Lookup Optimization (LOW PRIORITY)

### Current State

Current implementation does linear search O(n) for each lookup:
```c
client_state_t* entry = get_client_state_entry(conn_id);
```

### Recommended Optimization

Add caching if conn_id → index mapping becomes hot path:
```c
// Avoid repeated linear searches for same conn_id
static uint16_t _last_conn_id = 0xffff;
static int _last_index = -1;

static int _find_client_index(uint16_t conn_id) {
    if (conn_id == _last_conn_id && _last_index >= 0) {
        return _last_index;  // Cache hit
    }
    // Linear search...
}
```

### Impact

- **Performance**: Minimal (few connections)
- **Complexity**: LOW
- **Priority**: LOW - Not a bottleneck

---

## Implementation Roadmap

### Phase 1: Immediate (1-2 hours)
- [x] Identify refactoring opportunities
- [ ] Create `mutex_helpers.h`
- [ ] Create `nvs_helpers.h`
- [ ] Update 6 files to use mutex helpers
- [ ] Run tests

### Phase 2: Short-term (2-3 hours)
- [ ] Update 3 files to use NVS helpers
- [ ] Extract connection lookup helper
- [ ] Test thoroughly

### Phase 3: Medium-term (2-3 hours)
- [ ] Refactor remaining duplications
- [ ] Update task initialization
- [ ] Add device settings lock helpers

### Phase 4: Polish (1-2 hours)
- [ ] Extract debug output functions
- [ ] Add optimization if needed
- [ ] Final cleanup

---

## Testing Strategy

Each refactoring phase should:

1. **Run existing tests** before and after
   ```bash
   ./run_tests.sh
   # Must show: All 226 assertions passed
   ```

2. **Compile and check for warnings**
   ```bash
   idf.py build
   # Must show: 0 errors, acceptable warnings
   ```

3. **Manual verification on hardware** if available
   - Connect device
   - Verify all features work
   - Check serial output for errors

---

## Code Review Checklist

When implementing these refactorings:

- [ ] All 226 unit tests pass
- [ ] No new compiler warnings introduced
- [ ] Code follows existing style (clang-format)
- [ ] Comments explain non-obvious logic
- [ ] Function names are clear and unambiguous
- [ ] Error handling is comprehensive
- [ ] No behavioral changes to existing functionality
- [ ] Commit message clearly describes changes

---

## Estimated Effort

| Refactoring | Effort | Impact | Risk |
|---|---|---|---|
| Mutex helpers | 1.5 hrs | HIGH | LOW |
| NVS helpers | 2.5 hrs | HIGH | MEDIUM |
| Connection lookup | 0.5 hrs | MEDIUM | LOW |
| Task initialization | 0.5 hrs | LOW | LOW |
| Device settings lock | 1 hr | MEDIUM | MEDIUM |
| NVS read/write | 1 hr | MEDIUM | LOW |
| Debug output | 0.5 hrs | LOW | LOW |
| **TOTAL** | **7-11 hrs** | **HIGH** | **LOW** |

---

## Notes for Maintainers

### Why This Refactoring Matters

1. **Maintenance**: Reduce cognitive load when reading code
2. **Safety**: Build error handling into helpers, eliminate manual mistakes
3. **Consistency**: All similar operations follow same pattern
4. **Testing**: Existing comprehensive test suite validates correctness

### When to Apply

- After code review and approval
- In a dedicated refactoring PR
- When no other features are in flight
- Before major version releases

### How to Validate

After each refactoring:
```bash
# 1. Run all tests
./run_tests.sh

# 2. Build the project
cd pgpemu-esp32
idf.py build

# 3. Test on hardware if available
idf.py flash monitor

# 4. Verify all features work
```

---

## Related Documents

- [README.md](../README.md) - Usage and testing guide
- [TEST_SUITE_GUIDE.md](../TEST_SUITE_GUIDE.md) - Detailed test information
- Code comments in specific files for implementation details

---

**Last Updated**: January 9, 2026  
**Status**: Identified, documented, ready for implementation  
**Priority**: High-impact, low-risk improvements  
**Estimated Total Effort**: 7-11 hours  

---

## Appendix: Code Snippets

### Before and After Examples

#### Mutex Pattern
**Before** (44 lines):
```c
// config_storage.c
if (!xSemaphoreTake(global_settings.mutex, portMAX_DELAY)) {
    ESP_LOGE(CONFIG_STORAGE_TAG, "cannot get global settings mutex");
    return;
}
// ... critical section (15 lines) ...
xSemaphoreGive(global_settings.mutex);

// Later in same file...
if (!xSemaphoreTake(global_settings.mutex, portMAX_DELAY)) {
    ESP_LOGE(CONFIG_STORAGE_TAG, "cannot get global settings mutex");
    return;
}
// ... critical section (20 lines) ...
xSemaphoreGive(global_settings.mutex);
```

**After** (with helpers):
```c
// mutex_helpers.h
inline bool mutex_acquire_blocking(SemaphoreHandle_t m) {
    return m && xSemaphoreTake(m, portMAX_DELAY) == pdTRUE;
}

inline void mutex_release(SemaphoreHandle_t m) {
    if (m) xSemaphoreGive(m);
}

// config_storage.c - usage becomes:
if (mutex_acquire_blocking(global_settings.mutex)) {
    // ... critical section ...
    mutex_release(global_settings.mutex);
}

// Same code repeats, but helpers are called, not duplicated
```

#### NVS Pattern
**Before**:
```c
nvs_handle_t handle = {};
esp_err_t err = nvs_open("global_settings", NVS_READONLY, &handle);
if (err != ESP_OK) {
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "partition doesn't exist");
    } else {
        ESP_ERROR_CHECK(err);
    }
    // cleanup...
    return;
}
// ... use handle ...
nvs_close(handle);

// Later, similar code:
nvs_handle_t handle2 = {};
esp_err_t err2 = nvs_open("device_settings", NVS_READWRITE, &handle2);
if (err2 != ESP_OK) {
    // ... same error handling ...
}
// ... use handle2 ...
nvs_close(handle2);
```

**After**:
```c
// nvs_helpers.h
nvs_handle_t nvs_open_ro(const char* ns) {
    nvs_handle_t h = {};
    esp_err_t err = nvs_open(ns, NVS_READONLY, &h);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    }
    return h;
}

// Usage becomes one-liner:
nvs_handle_t handle = nvs_open_ro("global_settings");
if (handle) {
    // ... use handle ...
    nvs_close(handle);
}
```

---

This document serves as a roadmap for future code maintenance and improvement.

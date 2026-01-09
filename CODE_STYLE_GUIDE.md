# PGPemu Code Style Guide

This document defines the code style conventions for the PGPemu project. All code should follow these guidelines for consistency and maintainability.

## Overview

- **Language**: C99 with C11 features where supported by ESP-IDF
- **Formatter**: `clang-format` (configuration in `.clang-format`)
- **Line Length**: 120 characters maximum
- **Indentation**: 4 spaces (never tabs)
- **Style Base**: Google C++ Style with ESP-specific modifications

## Quick Start

### Format a File
```bash
cd pgpemu-esp32
clang-format -i main/myfile.c
clang-format -i main/myfile.h
```

### Format All Source Files
```bash
cd pgpemu-esp32
clang-format -i main/*.c main/*.h
```

### Format Before Committing
```bash
clang-format -i main/myfile.c && git add main/myfile.c && git commit
```

---

## Naming Conventions

### File Names
- **Source files**: `snake_case.c`
- **Header files**: `snake_case.h`
- **Test files**: `test_snake_case.c`

Examples:
- ✓ `pgp_handshake_multi.c` - Multi-word, lowercase, underscores
- ✓ `config_storage.h` - Clear, descriptive
- ✗ `pgpHandshakeMulti.c` - CamelCase (not allowed)
- ✗ `pgp-handshake.c` - Hyphens (use underscores instead)

### Function Names
- **Format**: `snake_case`
- **Private functions**: Prefix with underscore `_private_function()`
- **Public functions**: No underscore prefix

Examples:
```c
// Public functions - exported in header
bool read_stored_device_settings(esp_bd_addr_t bda, DeviceSettings* out_settings);
void connection_start(uint16_t conn_id);
int get_active_connections(void);

// Private functions - static in .c file
static int _find_client_index(uint16_t conn_id);
static bool _is_valid_bda(const esp_bd_addr_t bda);
static void _initialize_retoggle_fields(DeviceSettings* settings);
```

### Variable Names

#### Global Variables
- **Format**: `snake_case`
- **Prefix**: Consider prefixing with `g_` for mutable globals to indicate global scope
- **Static module variables**: `snake_case` or `g_snake_case`

Examples:
```c
// Module-level static (equivalent to private global)
static int active_connections = 0;
static SemaphoreHandle_t g_settings_mutex = NULL;
static client_state_t g_client_states[MAX_CONNECTIONS] = {};

// Global (exported in header)
GlobalSettings global_settings = {};
```

#### Local Variables
- **Format**: `snake_case`
- **Meaning**: Names should be clear and descriptive
- **Loop counters**: `i`, `j`, `k` acceptable for simple loops only

Examples:
```c
// Good - descriptive names
bool autocatch_enabled = device->settings->autocatch;
uint32_t timeout_ms = 1000;
client_state_t* client_entry = get_client_state_entry(conn_id);

// Acceptable - simple counters
for (int i = 0; i < MAX_CONNECTIONS; i++) {
    // ...
}

// Avoid - unclear names
int x = 5;  // What is x?
bool f = true;  // What does f mean?
```

#### Pointer Variables
- **Format**: `*` attached to variable name, not type

Examples:
```c
// Correct style
esp_bd_addr_t* remote_bda;
client_state_t* client_entry;
DeviceSettings* out_settings;

// Avoid
esp_bd_addr_t *remote_bda;
client_state_t*client_entry;
```

### Constants and Macros

#### Compile-Time Constants
- **Format**: `UPPER_CASE`
- **Scope**: Use `#define` with file-scope or module-scope prefix when appropriate

Examples:
```c
// File-scoped constants
#define MAX_CONNECTIONS CONFIG_BT_ACL_CONNECTIONS
#define CONFIG_STORAGE_TAG "config_storage"
#define FNV1A_OFFSET_BASIS 0x811c9dc5
#define FNV1A_PRIME 0x01000193

// Scoped constants when part of larger enum/group
#define DEVICE_KEY_BUFFER_SIZE 32
#define NVS_KEY_MAX_LEN 15
#define RETOGGLE_DELAY_MS 300
```

#### Macros with Parameters
- **Format**: `UPPER_CASE`
- **Safety**: Always parenthesize parameters and result
- **Use sparingly**: Consider inline functions instead for type safety

Examples:
```c
// Good macro - used for compile-time constants
#define CONFIG_TAG(name) CONFIG_STORAGE_TAG

// Good macro - used for type-agnostic operations
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Prefer inline function for better type safety
inline static bool is_valid_bda(const esp_bd_addr_t bda) {
    // ...
}

// Avoid - complex macros are hard to debug
#define UPDATE_CONNECTION(conn_id, state) \
    do { \
        if (active_connections_mutex) { \
            xSemaphoreTake(active_connections_mutex, portMAX_DELAY); \
            /* 10 more lines... */ \
            xSemaphoreGive(active_connections_mutex); \
        } \
    } while(0)
// Better: Extract into a real function
```

### Type Names

#### Typedef Structs
- **Format**: `snake_case_t`
- **Suffix**: Always end with `_t`

Examples:
```c
// Correct
typedef struct {
    uint8_t autocatch;
    uint8_t autospin;
    uint8_t autospin_probability;
} device_settings_t;

// Also acceptable if struct has body
typedef struct device_settings_t {
    uint8_t autocatch;
    // ...
} device_settings_t;

// Avoid
typedef struct {
    uint8_t autocatch;
} DeviceSettings;  // No _t suffix

typedef struct {
    uint8_t autocatch;
} device_settings;  // No _t suffix
```

#### Enum Types
- **Format**: `snake_case_t` for the enum typedef
- **Values**: `UPPER_CASE` with semantic prefix

Examples:
```c
// Correct
typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_ON = 1,
    LED_STATE_BLINKING = 2,
} led_state_t;

// Usage
led_state_t current_state = LED_STATE_ON;

// Also acceptable - shorter names if context is clear
typedef enum {
    STATE_INIT = 0,
    STATE_HANDSHAKE = 1,
    STATE_CONNECTED = 2,
} handshake_state_t;
```

### Log Tags
- **Format**: `snake_case_TAG` or just upper case
- **Scope**: Usually defined in each module

Examples:
```c
// In config_storage.c
static const char* CONFIG_STORAGE_TAG = "config_storage";

// In pgp_handshake_multi.c
static const char* HANDSHAKE_TAG = "pgp_handshake";

// Or collected in log_tags.h
extern const char* const CONFIG_STORAGE_TAG;
extern const char* const HANDSHAKE_TAG;
```

---

## Code Formatting

### Indentation
- **Size**: 4 spaces per level
- **No tabs**: Always spaces, never tabs
- **Continuation lines**: Indent 4 more spaces

```c
// Good
if (condition1 && 
    condition2) {
    // 4 spaces from if
}

// Avoid
if(condition1&&condition2){
    // Hard to read
}
```

### Brace Placement

#### Functions
```c
// Correct - opening brace on new line
void example_function(int param1, int param2)
{
    // Function body
}
```

#### Control Statements
```c
// Correct - brace on same line
if (condition) {
    // Body
} else if (other_condition) {
    // Body
} else {
    // Body
}

// Also correct
for (int i = 0; i < count; i++) {
    // Body
}

while (condition) {
    // Body
}
```

#### Structs and Enums
```c
// Correct
typedef struct {
    uint8_t field1;
    uint8_t field2;
} my_struct_t;

typedef enum {
    VALUE1 = 0,
    VALUE2 = 1,
} my_enum_t;
```

### Line Length
- **Maximum**: 120 characters
- **Rare exceptions**: Necessary for clarity (e.g., long strings)

```c
// Good - within 120 chars
ESP_LOGI(TAG, "This is a moderately long log message "
              "that spans multiple lines for clarity");

// Avoid - exceeding 120 chars when avoidable
ESP_LOGI(TAG, "This is an unnecessarily long log message that should have been split across multiple lines");
```

### Spacing

#### Around Operators
```c
// Binary operators - space on both sides
int result = a + b;
bool condition = (x == y) && (z != w);

// Assignment - space on both sides
int value = 42;
pointer = &variable;

// Function calls - no space before parenthesis
function_call(arg1, arg2);
if (condition) { }  // Space between keyword and paren
for (int i = 0; i < 10; i++) { }  // Space after comma
```

#### In Declarations
```c
// Pointer asterisk - attached to variable name
uint8_t* pointer;
int* array_of_ints;

// Multiple pointers on one line - separate lines preferred
uint8_t* ptr1;
uint8_t* ptr2;

// Const placement
const uint8_t* const_ptr;
uint8_t const* also_const_ptr;  // Less common but acceptable
```

### Empty Lines
- **Between functions**: 2 blank lines
- **Within function**: 1 blank line for logical sections
- **In general**: Maximum 2 consecutive blank lines

```c
void function_one(void)
{
    // Function body
}


void function_two(void)
{
    // Setup section
    variable = 0;
    
    // Processing section
    process_data();
    
    // Cleanup section
    cleanup();
}
```

---

## Comments

### File Headers
```c
/**
 * @file config_storage.c
 * @brief Device settings persistence and retrieval
 * 
 * This module handles reading and writing device settings to NVS
 * (Non-Volatile Storage). Each device has independent settings
 * indexed by MAC address.
 */
```

### Function Comments
```c
/**
 * @brief Read device settings from NVS storage
 * 
 * Attempts to load previously saved settings for the device
 * identified by @p bda. If no settings exist, initializes with
 * default values.
 * 
 * @param bda Device Bluetooth address (6 bytes)
 * @param out_settings Pointer to DeviceSettings structure to populate
 * @return true if settings loaded successfully, false on error
 * 
 * @note Acquires mutex protection during read
 * @note Settings are indexed by device MAC address (BDA)
 */
bool read_stored_device_settings(esp_bd_addr_t bda, DeviceSettings* out_settings);
```

### Inline Comments
```c
// Good - explains why, not what
if (connection_count > MAX_CONNECTIONS) {
    // Prevent exceeding BLE connection limit (hardware constraint)
    ESP_LOGE(TAG, "too many devices connected");
    return false;
}

// Avoid - redundant explanation of obvious code
if (x > 0) {  // Check if x is greater than zero
    return true;  // Return true
}
```

### Complex Logic Comments
```c
// Complex state machine - explain transitions
switch (cert_state) {
    case STATE_INIT:
        // First packet: certificate exchange
        cert_state = STATE_CERT_EXCHANGE;
        break;
    
    case STATE_CERT_EXCHANGE:
        // Validate certificate, then move to key exchange
        if (!validate_cert(data)) {
            cert_state = STATE_INIT;  // Reset on failure
            break;
        }
        cert_state = STATE_KEY_EXCHANGE;
        break;
}
```

---

## Error Handling

### Return Codes
```c
// Functions returning bool - indicate success/failure
bool read_stored_device_settings(...);  // true = success, false = failure
bool mutex_acquire_blocking(SemaphoreHandle_t mutex);  // true = acquired

// Functions returning int - return status or count
int get_active_connections(void);  // Returns count or -1 for error
int find_client_index(uint16_t conn_id);  // Returns index 0-3, or -1 if not found

// ESP-IDF functions - use esp_err_t
esp_err_t err = nvs_get_u8(handle, key, &value);
if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_get_u8 failed: %s", esp_err_to_name(err));
    return false;
}
```

### Error Logging
```c
// Use appropriate log levels
ESP_LOGE(TAG, "critical error: %s", esp_err_to_name(err));  // Errors
ESP_LOGW(TAG, "device settings not found");  // Warnings
ESP_LOGI(TAG, "device connected");  // Info level
ESP_LOGD(TAG, "detailed debug info");  // Debug (not in release builds)

// Include context in error messages
ESP_LOGE(TAG, "read_stored_device_settings failed: invalid bda");
ESP_LOGE(TAG, "connection_start: conn_id %d unknown", conn_id);
```

---

## Specific Patterns

### Null Pointer Checks
```c
// Check before dereference
if (!pointer) {
    ESP_LOGE(TAG, "pointer is null");
    return false;
}

// Or return early
if (pointer == NULL) {
    ESP_LOGE(TAG, "settings pointer is NULL");
    return;
}

// Multiple checks
if (!device || !device->settings || !device->settings->bda) {
    ESP_LOGE(TAG, "invalid device structure");
    return false;
}
```

### Mutex Operations
```c
// Modern pattern with helper functions
if (mutex_acquire_blocking(my_mutex)) {
    // Critical section
    value = shared_variable;
    mutex_release(my_mutex);
}

// Or with guard macro
WITH_MUTEX_LOCK(my_mutex) {
    // Automatic acquisition and release
    shared_variable++;
    // Mutex released automatically on exit
}

// With timeout
if (mutex_acquire_timeout(my_mutex, 100)) {
    // Critical section
    mutex_release(my_mutex);
} else {
    ESP_LOGW(TAG, "mutex timeout");
}
```

### Array Iteration
```c
// Simple counter loops
for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (devices[i].active) {
        process_device(&devices[i]);
    }
}

// With array element variable
for (int i = 0; i < count; i++) {
    device_settings_t* settings = &device_array[i];
    if (validate_settings(settings)) {
        apply_settings(settings);
    }
}

// Avoid overly verbose
for (int connection_index = 0; 
     connection_index < maximum_connection_count;  // Too verbose
     connection_index++) {
    // ...
}
```

### String Handling
```c
// Use proper string functions
char buffer[64] = {0};
int written = snprintf(buffer, sizeof(buffer), "Value: %d", value);
if (written < 0 || written >= sizeof(buffer)) {
    ESP_LOGE(TAG, "snprintf truncated or failed");
    return false;
}

// Safe string copy with length check
char device_name[32] = {0};
if (strlen(source_name) >= sizeof(device_name)) {
    ESP_LOGE(TAG, "device name too long");
    return false;
}
strncpy(device_name, source_name, sizeof(device_name) - 1);
```

---

## Module Organization

### Header File Structure
```c
/**
 * @file module_name.h
 * @brief Brief description
 */

#pragma once

#include "esp_err.h"
#include "other_header.h"

// Type definitions
typedef struct {
    // ...
} my_type_t;

typedef enum {
    // ...
} my_enum_t;

// Public function declarations
bool public_function(parameters);
void another_public_function(void);

// Public data (if any)
extern int public_global_variable;

#endif // MODULE_NAME_H
```

### Source File Structure
```c
/**
 * @file module_name.c
 * @brief Implementation of module_name
 */

#include "module_name.h"
#include "esp_log.h"
#include "other_dependencies.h"

#include <string.h>
#include <stdbool.h>

static const char* TAG = "module_name";

// Private type definitions
typedef struct {
    // ...
} _private_type_t;

// Module-level statics
static int _module_state = 0;
static SemaphoreHandle_t _mutex = NULL;

// Private functions (before public ones)
static void _private_helper_function(void);
static int _internal_calculation(int param);

// Public function implementations
void public_function(int param)
{
    // Implementation
}

void another_public_function(void)
{
    // Implementation
}

// Private function implementations
static void _private_helper_function(void)
{
    // Implementation
}

static int _internal_calculation(int param)
{
    // Implementation
}
```

---

## Checklist for Code Review

Before submitting code for review:

- [ ] All code formatted with `clang-format -i`
- [ ] File names in `snake_case.c` and `snake_case.h`
- [ ] Function names in `snake_case`
- [ ] Variable names in `snake_case`
- [ ] Constants in `UPPER_CASE`
- [ ] Pointer asterisks attached to variable names
- [ ] Line length <= 120 characters (except rare exceptions)
- [ ] Comments explain "why" not just "what"
- [ ] Error handling for all error conditions
- [ ] Null pointer checks before dereference
- [ ] Mutex operations protected
- [ ] No trailing whitespace
- [ ] No commented-out code
- [ ] Log tags defined consistently
- [ ] Type definitions end with `_t`

---

## Tooling

### Automatic Formatting
```bash
# Format single file
clang-format -i main/myfile.c

# Format all .c and .h files
find main -name "*.c" -o -name "*.h" | xargs clang-format -i

# Check without modifying (preview changes)
clang-format main/myfile.c
```

### Static Analysis
```bash
# Run clang-tidy
clang-tidy main/myfile.c -- -I./main

# Full project analysis (if available)
cd pgpemu-esp32
idf.py build  # includes static analysis if configured
```

### Pre-commit Hook
```bash
#!/bin/bash
# .git/hooks/pre-commit

cd pgpemu-esp32/main
clang-format -i *.c *.h
git add *.c *.h
```

---

## Common Mistakes to Avoid

| ❌ Avoid | ✓ Correct |
|---|---|
| `int i,j,k;` | `int i;` `int j;` `int k;` |
| `CamelCase` | `snake_case` |
| Function name (arg) | `function_name(arg)` |
| `void function ( )` | `void function(void)` |
| `int* ptr1, ptr2;` | `int* ptr1;` `int* ptr2;` |
| `if(condition)` | `if (condition)` |
| `//TODO: fix this` | (Use PR comments instead) |
| `x += 1;` with unclear x | `counter++;` with clear name |
| `#define MAX 100` | `#define ARRAY_MAX 100` (scoped) |
| Very long functions (>50 lines) | Break into smaller functions |
| Multiple return points | Consider restructuring |

---

## References

- `.clang-format` - Actual formatting configuration
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) - Base reference
- [CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard) - Safety guidelines
- [ESP-IDF Coding Guidelines](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html)

---

## Questions?

If you have questions about style conventions:

1. Check this guide first
2. Look at existing code for examples
3. Ask in PR review comments
4. Update this guide if clarification is needed

---

**Last Updated**: January 9, 2026  
**Status**: Active and maintained  
**Enforcement**: Required for all commits  

---

## Appendix: Before and After Examples

### Example 1: Function Naming
```c
// ❌ Before - unclear names
int calc(int a, int b) {
    return a + b;
}

void proc_dev(void) {
    int x = get_conn();
    if (x > 0) {
        update_st();
    }
}

// ✓ After - clear, descriptive names
int calculate_total(int quantity, int price) {
    return quantity * price;
}

void process_device_connection(void) {
    int active_count = get_active_connections();
    if (active_count > 0) {
        update_device_state();
    }
}
```

### Example 2: Variable Naming
```c
// ❌ Before
uint8_t a, b, c;
bool s, x, y;
int n = 5;

// ✓ After
uint8_t autocatch;
uint8_t autospin;
uint8_t autospin_probability;
bool session_cached;
bool is_encrypted;
bool is_valid;
int max_connections = 5;
```

### Example 3: Comments
```c
// ❌ Before - what, not why
if (value > 100) {  // Check if value is greater than 100
    reset();  // Call reset function
    return true;  // Return true
}

// ✓ After - explain the reasoning
if (value > 100) {
    // Exceed maximum allowed value - reset to safe state
    reset();
    return true;
}
```

### Example 4: Formatting
```c
// ❌ Before - poor spacing, unclear
int get_value(void){if(x==NULL){return -1;}return x->value;}

// ✓ After - clear and readable
int get_value(void)
{
    if (x == NULL) {
        return -1;
    }
    return x->value;
}
```

---

**Remember**: Code is read 10x more often than it's written. Write for your future self and other maintainers!

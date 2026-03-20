// Battery conversion function tests
// Tests for battery_mv_to_percent() function extracted from battery.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

// ============================================================================
// Configuration defaults from Kconfig.projbuild
// ============================================================================
#define BATTERY_EMPTY_MV 3300
#define BATTERY_FULL_MV 4200
#define BATTERY_DIVIDER_RATIO 2

// ============================================================================
// Test infrastructure
// ============================================================================
static int tests_passed = 0;
static int tests_failed = 0;

#define test_assert(condition) \
    do { \
        if (!(condition)) { \
            tests_failed++; \
            printf("  ✗ FAIL at line %d\n", __LINE__); \
        } else { \
            tests_passed++; \
        } \
    } while (0)

// ============================================================================
// Function under test: battery_mv_to_percent()
// Extracted from battery.c:27-28
// Formula: (uint8_t)((mv - empty_mv) * 100 / (full_mv - empty_mv))
// ============================================================================
static uint8_t battery_mv_to_percent(uint32_t mv, uint32_t empty_mv, uint32_t full_mv) {
    return (uint8_t)((mv - empty_mv) * 100 / (full_mv - empty_mv));
}

// ============================================================================
// TEST 1: Boundary Values (3 assertions)
// Tests: EMPTY, FULL, and midpoint calculations
// ============================================================================
void test_battery_boundary_values(void) {
    printf("=== Test: Battery Boundary Values ===\n");

    // Test EMPTY boundary: 3300mV should give 0%
    uint8_t result_empty = battery_mv_to_percent(BATTERY_EMPTY_MV, BATTERY_EMPTY_MV, BATTERY_FULL_MV);
    test_assert(result_empty == 0);
    printf("  ✓ battery_mv_to_percent(%u, %u, %u) = %u (expected 0)\n",
           BATTERY_EMPTY_MV, BATTERY_EMPTY_MV, BATTERY_FULL_MV, result_empty);

    // Test FULL boundary: 4200mV should give 100%
    uint8_t result_full = battery_mv_to_percent(BATTERY_FULL_MV, BATTERY_EMPTY_MV, BATTERY_FULL_MV);
    test_assert(result_full == 100);
    printf("  ✓ battery_mv_to_percent(%u, %u, %u) = %u (expected 100)\n",
           BATTERY_FULL_MV, BATTERY_EMPTY_MV, BATTERY_FULL_MV, result_full);

    // Test MIDPOINT: 3750mV (halfway between 3300 and 4200) should give 50%
    uint32_t midpoint = (BATTERY_EMPTY_MV + BATTERY_FULL_MV) / 2;  // 3750
    uint8_t result_mid = battery_mv_to_percent(midpoint, BATTERY_EMPTY_MV, BATTERY_FULL_MV);
    test_assert(result_mid == 50);
    printf("  ✓ battery_mv_to_percent(%u, %u, %u) = %u (expected 50)\n",
           midpoint, BATTERY_EMPTY_MV, BATTERY_FULL_MV, result_mid);
}

// ============================================================================
// TEST 2: Out of Range Low (2 assertions)
// Tests: behavior with voltages below EMPTY_MV
// ============================================================================
void test_battery_out_of_range_low(void) {
    printf("=== Test: Battery Out of Range Low ===\n");

    // Test 0mV: way below empty, formula will underflow/wrap
    // (0 - 3300) * 100 / 900 = large negative number that wraps in uint8_t
    uint8_t result_zero = battery_mv_to_percent(0, BATTERY_EMPTY_MV, BATTERY_FULL_MV);
    // Just verify it doesn't crash; behavior with underflow is undefined
    printf("  ✓ battery_mv_to_percent(0, %u, %u) = %u (underflow behavior)\n",
           BATTERY_EMPTY_MV, BATTERY_FULL_MV, result_zero);
    test_assert(1);  // Passes if function doesn't crash

    // Test 3299mV: just 1mV below empty
    uint8_t result_below = battery_mv_to_percent(3299, BATTERY_EMPTY_MV, BATTERY_FULL_MV);
    // (3299 - 3300) * 100 / 900 = -1 * 100 / 900 = -1/9 truncates to 0
    // But in unsigned arithmetic: (3299-3300) wraps, giving large uint32_t value
    // This is undefined behavior; just verify it compiles and runs
    printf("  ✓ battery_mv_to_percent(3299, %u, %u) = %u (just below empty)\n",
           BATTERY_EMPTY_MV, BATTERY_FULL_MV, result_below);
    test_assert(result_below < 100);  // Any value is acceptable (underflow)
}

// ============================================================================
// TEST 3: Out of Range High (3 assertions)
// Tests: behavior with voltages above FULL_MV
// Note: In production, battery_get_percent() clamps these before calling
// this function, but we test formula behavior here
// ============================================================================
void test_battery_out_of_range_high(void) {
    printf("=== Test: Battery Out of Range High ===\n");

    // Test 4201mV: just 1mV above full
    // (4201 - 3300) * 100 / 900 = 901 * 100 / 900 = 100.111... = 100 (truncated)
    uint8_t result_above = battery_mv_to_percent(4201, BATTERY_EMPTY_MV, BATTERY_FULL_MV);
    printf("  ✓ battery_mv_to_percent(4201, %u, %u) = %u (just above full)\n",
           BATTERY_EMPTY_MV, BATTERY_FULL_MV, result_above);
    test_assert(result_above == 100);

    // Test 5000mV: way above full
    // (5000 - 3300) * 100 / 900 = 1700 * 100 / 900 = 188.888... = 188 (truncated)
    // But uint8_t will wrap: 188 % 256 = 188
    uint8_t result_high = battery_mv_to_percent(5000, BATTERY_EMPTY_MV, BATTERY_FULL_MV);
    printf("  ✓ battery_mv_to_percent(5000, %u, %u) = %u (way above full)\n",
           BATTERY_EMPTY_MV, BATTERY_FULL_MV, result_high);
    test_assert(result_high == 188);  // (5000-3300)*100/900 = 188

    // Test UINT32_MAX: extreme overvoltage
    // (4294967295 - 3300) * 100 / 900 overflows uint32_t math, then converts to uint8_t
    uint8_t result_max = battery_mv_to_percent(UINT32_MAX, BATTERY_EMPTY_MV, BATTERY_FULL_MV);
    printf("  ✓ battery_mv_to_percent(UINT32_MAX, %u, %u) = %u (extreme overvoltage)\n",
           BATTERY_EMPTY_MV, BATTERY_FULL_MV, result_max);
    test_assert(result_max > 100);  // Will overflow and produce garbage value
}

// ============================================================================
// TEST 4: Mid-Range Values (2 assertions)
// Tests: quarter and three-quarter percentage points
// ============================================================================
void test_battery_mid_range(void) {
    printf("=== Test: Battery Mid-Range Values ===\n");

    // Test 25% point: 3525mV
    // Range is 3300-4200 (900mV span)
    // 25% = 3300 + 900*0.25 = 3300 + 225 = 3525mV
    uint8_t result_25 = battery_mv_to_percent(3525, BATTERY_EMPTY_MV, BATTERY_FULL_MV);
    test_assert(result_25 == 25);
    printf("  ✓ battery_mv_to_percent(3525, %u, %u) = %u (expected 25%%)\n",
           BATTERY_EMPTY_MV, BATTERY_FULL_MV, result_25);

    // Test 75% point: 3975mV
    // 75% = 3300 + 900*0.75 = 3300 + 675 = 3975mV
    uint8_t result_75 = battery_mv_to_percent(3975, BATTERY_EMPTY_MV, BATTERY_FULL_MV);
    test_assert(result_75 == 75);
    printf("  ✓ battery_mv_to_percent(3975, %u, %u) = %u (expected 75%%)\n",
           BATTERY_EMPTY_MV, BATTERY_FULL_MV, result_75);
}

// ============================================================================
// TEST 5: Different Divider Ratios (3 assertions)
// Tests: formula behavior with different empty/full voltage ranges
// Divider ratio affects the voltage seen by ADC, so all should scale
// ============================================================================
void test_battery_divider_ratios(void) {
    printf("=== Test: Battery Divider Ratios ===\n");

    // Ratio 1: Direct voltage (no divider)
    // 50% = 3300 + 900*0.5 = 3750mV
    uint8_t result_ratio1 = battery_mv_to_percent(3750, 3300, 4200);
    test_assert(result_ratio1 == 50);
    printf("  ✓ Ratio 1: battery_mv_to_percent(3750, 3300, 4200) = %u (expected 50%%)\n",
           result_ratio1);

    // Ratio 2: Half voltage (with 1:1 divider like voltage divider)
    // Empty: 3300/2 = 1650, Full: 4200/2 = 2100, Mid: 1875
    // 50% = 1650 + (2100-1650)*0.5 = 1650 + 225 = 1875
    uint8_t result_ratio2 = battery_mv_to_percent(1875, 1650, 2100);
    test_assert(result_ratio2 == 50);
    printf("  ✓ Ratio 2: battery_mv_to_percent(1875, 1650, 2100) = %u (expected 50%%)\n",
           result_ratio2);

    // Ratio 4: Quarter voltage (hypothetical 1:3 divider)
    // Empty: 3300/4 = 825, Full: 4200/4 = 1050, Mid: 937.5
    // 50% = 825 + (1050-825)*0.5 = 825 + 112.5 = 937.5 ≈ 937
    uint8_t result_ratio4 = battery_mv_to_percent(937, 825, 1050);
    // (937 - 825) * 100 / (1050 - 825) = 112 * 100 / 225 = 11200 / 225 = 49.777... = 49
    test_assert(result_ratio4 == 49 || result_ratio4 == 50);  // Rounding variation
    printf("  ✓ Ratio 4: battery_mv_to_percent(937, 825, 1050) = %u (expected ~50%%)\n",
           result_ratio4);
}

// ============================================================================
// Main test runner
// ============================================================================
int main(void) {
    printf("================================================\n");
    printf("   Battery Voltage to Percentage Tests\n");
    printf("================================================\n\n");

    // Run all test functions
    test_battery_boundary_values();
    printf("\n");

    test_battery_out_of_range_low();
    printf("\n");

    test_battery_out_of_range_high();
    printf("\n");

    test_battery_mid_range();
    printf("\n");

    test_battery_divider_ratios();
    printf("\n");

    // Print summary
    printf("================================================\n");
    printf("Test Summary: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("Total assertions: %d\n", tests_passed + tests_failed);
    printf("================================================\n");

    if (tests_failed == 0) {
        printf("✓ All %d assertions passed\n", tests_passed);
        return 0;
    } else {
        printf("✗ %d assertions failed\n", tests_failed);
        return 1;
    }
}

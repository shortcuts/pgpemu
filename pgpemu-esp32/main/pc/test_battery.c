// battery_mv_to_percent() contract tests, against the real battery.c
#include "../battery.c"

#include <stdio.h>

static int tests_failed = 0;
static int tests_passed = 0;

#define test_assert(condition)                         \
    do {                                               \
        if (!(condition)) {                            \
            tests_failed++;                            \
            printf("  ✗ FAIL at line %d\n", __LINE__); \
        } else {                                       \
            tests_passed++;                            \
        }                                              \
    } while (0)

int main(void) {
    test_assert(battery_mv_to_percent(3300, 3300, 4200) == 0);
    test_assert(battery_mv_to_percent(4200, 3300, 4200) == 100);
    test_assert(battery_mv_to_percent(3750, 3300, 4200) == 50);
    test_assert(battery_mv_to_percent(3301, 3300, 4200) == 0);
    test_assert(battery_mv_to_percent(4199, 3300, 4200) == 99);
    test_assert(battery_mv_to_percent(0, 3300, 4200) == 0);
    test_assert(battery_mv_to_percent(100, 3300, 4200) == 0);
    test_assert(battery_mv_to_percent(5000, 3300, 4200) == 100);
    test_assert(battery_mv_to_percent(UINT32_MAX, 3300, 4200) == 100);
    test_assert(battery_mv_to_percent(3500, 4200, 3300) == 0);
    test_assert(battery_mv_to_percent(3500, 3500, 3500) == 0);
    test_assert(battery_get_percent() == 0);
    test_assert(battery_get_mv() == 0);

    printf("%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed ? 1 : 0;
}

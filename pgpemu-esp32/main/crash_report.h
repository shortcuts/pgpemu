#ifndef CRASH_REPORT_H
#define CRASH_REPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CRASH_REPORT_MAX_PCS 16
#define CRASH_REPORT_TASK_LEN 16
#define CRASH_REPORT_SHA_LEN 9
// present u8, mcause mtval pc ra sp u32 LE, task[16], elf_sha[9], pc_count u8, then pc_count x u32 LE
#define CRASH_REPORT_FIXED_LEN 47
#define CRASH_REPORT_MAX_LEN (CRASH_REPORT_FIXED_LEN + 4 * CRASH_REPORT_MAX_PCS)

// Reads the core dump of the last panic once and keeps a compact report in RAM. Before this call there is none.
void crash_report_init(void);
// Writes the wire form: one 0 byte without a report. Returns 0 when cap is too small.
size_t crash_report_encode(uint8_t* out, size_t cap);
// Erases the core dump and drops the report. False, with the report kept, when the erase fails.
bool crash_report_clear(void);

#endif /* CRASH_REPORT_H */

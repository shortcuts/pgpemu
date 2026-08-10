#ifndef BUF_WRITER_H
#define BUF_WRITER_H

#include <stddef.h>
#include <stdint.h>

// Bounds-checked accumulator for building a report string into a
// caller-owned buffer via repeated snprintf-style appends. Shared by every
// module that formats a debug dump into a fixed buffer instead of logging it
// directly (pgp_handshake_multi.c, stats.c), so the offset/overflow guard
// logic exists in exactly one place.
typedef struct {
    char* buf;
    size_t buf_len;
    size_t offset;
} buf_writer_t;

void buf_writer_init(buf_writer_t* w, char* buf, size_t buf_len);

// Appends a printf-style formatted string. No-ops once buf_len is reached.
void buf_writer_appendf(buf_writer_t* w, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

// Appends "<label>: <hex bytes>\n".
void buf_writer_append_hex(buf_writer_t* w, const char* label, const uint8_t* data, size_t len);

// Bytes written so far (excluding the null terminator), clamped to buf_len.
size_t buf_writer_len(const buf_writer_t* w);

#endif /* BUF_WRITER_H */

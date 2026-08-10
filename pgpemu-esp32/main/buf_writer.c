#include "buf_writer.h"

#include <stdarg.h>
#include <stdio.h>

void buf_writer_init(buf_writer_t* w, char* buf, size_t buf_len) {
    w->buf = buf;
    w->buf_len = buf_len;
    w->offset = 0;
}

void buf_writer_appendf(buf_writer_t* w, const char* fmt, ...) {
    if (w->offset >= w->buf_len) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(w->buf + w->offset, w->buf_len - w->offset, fmt, args);
    va_end(args);
    w->offset += (n > 0) ? (size_t)n : 0;
}

void buf_writer_append_hex(buf_writer_t* w, const char* label, const uint8_t* data, size_t len) {
    buf_writer_appendf(w, "%s: ", label);
    for (size_t i = 0; i < len && w->offset < w->buf_len; i++) {
        buf_writer_appendf(w, "%02x", data[i]);
    }
    buf_writer_appendf(w, "\n");
}

size_t buf_writer_len(const buf_writer_t* w) {
    return w->offset > w->buf_len ? w->buf_len : w->offset;
}

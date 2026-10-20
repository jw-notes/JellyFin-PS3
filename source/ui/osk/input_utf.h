#pragma once
#include <stdint.h>
#include <stddef.h>

// Fail rather than silently truncate a server address or password.
static inline bool input_to_utf16(const char *text, uint16_t *out, size_t cap) {
    if (!text || !out || !cap) return false;
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        uint32_t c = *p++;
        int extra = 0;
        uint32_t minimum = 0;
        if (c >= 0xc2 && c <= 0xdf) { c &= 31; extra = 1; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { c &= 15; extra = 2; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { c &= 7; extra = 3; minimum = 0x10000; }
        else if (c >= 0x80) { out[0] = 0; return false; }
        for (int i = 0; i < extra; ++i) {
            if ((*p & 0xc0) != 0x80) { out[0] = 0; return false; }
            c = (c << 6) | (*p++ & 63);
        }
        if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) {
            out[0] = 0; return false;
        }
        size_t need = c >= 0x10000 ? 2 : 1;
        if (n + need >= cap) { out[0] = 0; return false; }
        if (need == 2) {
            c -= 0x10000;
            out[n++] = (uint16_t)(0xd800 + (c >> 10));
            out[n++] = (uint16_t)(0xdc00 + (c & 1023));
        } else out[n++] = (uint16_t)c;
    }
    out[n] = 0;
    return true;
}

static inline bool input_from_utf16(const uint16_t *text, size_t units,
                                     char *out, size_t cap) {
    if (!text || !out || !cap) return false;
    size_t n = 0;
    out[0] = 0;
    for (size_t i = 0; i < units; ++i) {
        uint32_t c = text[i];
        if (!c) { out[n] = 0; return true; }
        if (c >= 0xd800 && c <= 0xdbff) {
            if (++i >= units || text[i] < 0xdc00 || text[i] > 0xdfff) return false;
            c = 0x10000 + ((c - 0xd800) << 10) + text[i] - 0xdc00;
        } else if (c >= 0xdc00 && c <= 0xdfff) return false;
        size_t need = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
        if (n + need >= cap) { out[0] = 0; return false; }
        if (need == 1) out[n++] = (char)c;
        else {
            out[n++] = (char)((need == 2 ? 0xc0 : need == 3 ? 0xe0 : 0xf0) |
                             (c >> (6 * (need - 1))));
            for (size_t j = need - 1; j > 0; --j)
                out[n++] = (char)(0x80 | ((c >> (6 * (j - 1))) & 63));
        }
    }
    out[0] = 0;
    return false;  // firmware output was not terminated within its buffer
}

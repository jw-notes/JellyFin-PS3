#pragma once
// Copyright (C) 2026 jw-notes.  GPLv3, see LICENSE.
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// -------------------------------------------------------------------------
//  A range response's headers
// -------------------------------------------------------------------------
//  Pure, so tests/test_http_range.c compiles what the console runs.  The
//  headers are the server's and are not trusted: anything but an answer for
//  exactly the bytes asked for is refused.

// The value of a header (after "Name: "), or NULL.  Names match in any case.
static inline const char *http_header_value(const char *hdr, const char *name) {
    const size_t nl = strlen(name);
    for (const char *p = strchr(hdr, '\n'); p; p = strchr(p, '\n')) {
        p++;
        if (strncasecmp(p, name, nl) == 0 && p[nl] == ':') {
            p += nl + 1;
            while (*p == ' ' || *p == '\t') p++;
            return p;
        }
    }
    return NULL;
}

// True when `hdr` answers a request for the file from byte `offset` on, with
// the data starting at exactly that byte; *total is the file's size.  A 200 is
// the whole file, so it only answers a request from byte 0.
static inline bool http_range_parse(const char *hdr, uint64_t offset, uint64_t *total) {
    if (strncmp(hdr, "HTTP/", 5) != 0) return false;
    const char *sp = strchr(hdr, ' ');
    if (!sp) return false;
    const int status = atoi(sp + 1);
    if (status == 200) {
        if (offset != 0) return false;
        const char *cl = http_header_value(hdr, "Content-Length");
        if (!cl || *cl < '0' || *cl > '9') return false;
        *total = strtoull(cl, NULL, 10);
        return *total > 0;
    }
    if (status != 206) return false;
    const char *cr = http_header_value(hdr, "Content-Range");
    if (!cr || strncasecmp(cr, "bytes ", 6) != 0) return false;
    cr += 6;
    if (*cr < '0' || *cr > '9') return false;
    char *end = NULL;
    const uint64_t first = strtoull(cr, &end, 10);
    if (first != offset || !end || *end != '-') return false;
    const char *slash = strchr(end, '/');
    if (!slash || slash[1] < '0' || slash[1] > '9') return false;
    *total = strtoull(slash + 1, NULL, 10);
    return *total > offset;
}

#pragma once
// Copyright (C) 2026 jw-notes.  GPLv3, see LICENSE.
#include <stdbool.h>
#include <string.h>

// -------------------------------------------------------------------------
//  Home Order: My Media's libraries in the viewer's own order
// -------------------------------------------------------------------------
//  Pure, so tests/test_home_order.c compiles what the console runs.  The saved
//  order is the libraries' keys, comma separated.  A key not in it (a library
//  added since) keeps its place after the saved ones; a saved key with no
//  library (one removed since) is passed over.

// order[0..n) gets the positions in keys[] in display order.
static inline void home_order_apply(const char *saved, const char *const *keys, int n, int *order) {
    bool used[64];
    if (n > 64) n = 64;
    for (int i = 0; i < n; i++) used[i] = false;
    int k = 0;
    const char *p = saved ? saved : "";
    while (*p && k < n) {
        const char *e = strchr(p, ',');
        const size_t len = e ? (size_t)(e - p) : strlen(p);
        for (int i = 0; i < n; i++) {
            if (used[i] || strlen(keys[i]) != len || strncmp(keys[i], p, len) != 0) continue;
            used[i] = true;
            order[k++] = i;
            break;
        }
        p += len;
        if (*p == ',') p++;
    }
    for (int i = 0; i < n; i++) if (!used[i]) order[k++] = i;
}

// The saved form of keys[order[0..n)].  False, leaving "" in out, when it does not fit.
static inline bool home_order_format(const char *const *keys, const int *order, int n, char *out, int cap) {
    int at = 0;
    if (cap > 0) out[0] = '\0';
    for (int i = 0; i < n; i++) {
        const int len = (int)strlen(keys[order[i]]);
        if (at + len + 2 > cap) { if (cap > 0) out[0] = '\0'; return false; }
        if (i) out[at++] = ',';
        memcpy(out + at, keys[order[i]], (size_t)len);
        at += len;
        out[at] = '\0';
    }
    return true;
}

#pragma once

// Find the package in a GitHub "latest release" response: its download URL,
// size and SHA-256 (GitHub's "digest", "sha256:<hex>").  Pure C so
// tests/test_release_asset.c runs it on a real response.
//
// The asset is the one named `want`, or failing that the first whose name ends
// in ".pkg".  Fields are read only inside that asset's own object -- each asset
// also carries an "uploader" object with URLs of its own -- found by matching
// braces outside strings.

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct release_asset {
    char     url[512];
    char     sha256[65];     // lowercase hex, "" when GitHub gave none
    unsigned size;
    char     name[96];
} release_asset;

// The end of the JSON value (object or array) starting at p (on '{' or '['),
// i.e. one past its closing bracket; NULL if it never closes.
static inline const char *ra_value_end(const char *p) {
    int depth = 0;
    bool in_str = false;
    for (; *p; p++) {
        if (in_str) {
            if (*p == '\\' && p[1]) p++;
            else if (*p == '"') in_str = false;
            continue;
        }
        if (*p == '"') in_str = true;
        else if (*p == '{' || *p == '[') depth++;
        else if (*p == '}' || *p == ']') { if (--depth == 0) return p + 1; }
    }
    return NULL;
}

// A "key":"value" string inside [b, e), at the object's top level only.
static inline bool ra_str(const char *b, const char *e, const char *key, char *out, int cap) {
    char pat[48];
    const int pl = (int)strlen(key) + 2;
    if (pl + 1 > (int)sizeof pat) return false;
    pat[0] = '"'; memcpy(pat + 1, key, (size_t)pl - 2); pat[pl - 1] = '"'; pat[pl] = '\0';
    int depth = 0;
    bool in_str = false;
    for (const char *p = b; p < e; p++) {
        if (in_str) {
            if (*p == '\\' && p + 1 < e) { p++; continue; }
            if (*p == '"') in_str = false;
            continue;
        }
        if (*p == '{' || *p == '[') { depth++; continue; }
        if (*p == '}' || *p == ']') { depth--; continue; }
        if (*p != '"') continue;
        if (depth == 1 && e - p > pl && memcmp(p, pat, (size_t)pl) == 0) {
            const char *q = p + pl;
            while (q < e && (*q == ' ' || *q == ':')) q++;
            if (q >= e || *q != '"') return false;
            q++;
            int n = 0;
            while (q < e && *q != '"' && n < cap - 1) {
                if (*q == '\\' && q + 1 < e) q++;         // \/ in URLs
                out[n++] = *q++;
            }
            out[n] = '\0';
            return q < e && *q == '"';
        }
        in_str = true;
    }
    return false;
}

static inline bool ra_num(const char *b, const char *e, const char *key, unsigned *out) {
    char pat[48];
    const int pl = (int)strlen(key) + 3;
    if (pl + 1 > (int)sizeof pat) return false;
    pat[0] = '"'; memcpy(pat + 1, key, (size_t)pl - 3); pat[pl - 2] = '"'; pat[pl - 1] = ':'; pat[pl] = '\0';
    for (const char *p = b; p + pl < e; p++) {
        if (memcmp(p, pat, (size_t)pl) != 0) continue;
        const char *q = p + pl;
        while (q < e && *q == ' ') q++;
        if (q >= e || *q < '0' || *q > '9') return false;
        unsigned long v = 0;
        while (q < e && *q >= '0' && *q <= '9') { v = v * 10 + (unsigned long)(*q - '0'); if (v > 0xFFFFFFFFul) return false; q++; }
        *out = (unsigned)v;
        return true;
    }
    return false;
}

static inline bool release_asset_find(const char *json, const char *want, release_asset *out) {
    memset(out, 0, sizeof *out);
    const char *a = strstr(json, "\"assets\"");
    if (!a) return false;
    a = strchr(a, '[');
    if (!a) return false;
    const char *arr_end = ra_value_end(a);
    if (!arr_end) return false;
    const char *best_b = NULL, *best_e = NULL;
    for (const char *p = a + 1; p < arr_end; ) {
        while (p < arr_end && *p != '{') p++;
        if (p >= arr_end) break;
        const char *e = ra_value_end(p);
        if (!e || e > arr_end) break;
        char name[96];
        if (ra_str(p, e, "name", name, (int)sizeof name)) {
            const size_t nl = strlen(name);
            if (want && strcmp(name, want) == 0) { best_b = p; best_e = e; break; }
            if (!best_b && nl > 4 && strcmp(name + nl - 4, ".pkg") == 0) { best_b = p; best_e = e; }
        }
        p = e;
    }
    if (!best_b) return false;
    ra_str(best_b, best_e, "name", out->name, (int)sizeof out->name);
    if (!ra_str(best_b, best_e, "browser_download_url", out->url, (int)sizeof out->url)) return false;
    if (strncmp(out->url, "https://", 8) != 0) return false;
    ra_num(best_b, best_e, "size", &out->size);
    char digest[80];
    if (ra_str(best_b, best_e, "digest", digest, (int)sizeof digest) &&
        strncmp(digest, "sha256:", 7) == 0 && strlen(digest + 7) == 64)
        memcpy(out->sha256, digest + 7, 65);
    return true;
}

#ifdef __cplusplus
}
#endif

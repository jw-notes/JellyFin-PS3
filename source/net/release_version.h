#pragma once
#include <limits.h>
#include <strings.h>

static inline void ver_split(const char *s, int num[3], const char **suffix) {
    if (*s == 'v' || *s == 'V') s++;
    num[0] = num[1] = num[2] = 0;
    int i = 0;
    while (i < 3 && *s >= '0' && *s <= '9') {
        int v = 0;
        while (*s >= '0' && *s <= '9') {
            int digit = *s++ - '0';
            v = v > (INT_MAX - digit) / 10 ? INT_MAX : v * 10 + digit;
        }
        num[i++] = v;
        if (*s != '.') break;
        s++;
    }
    *suffix = s;
}

// > 0 when a is newer than b.  Numeric fields decide first; on a tie a plain
// release outranks a pre-release ("1.0" > "1.0-beta"), and two pre-release
// suffixes fall back to name order ("-beta" > "-alpha").  A tag with no
// number at all ("Testing") parses as 0.0.0, i.e. never newer.
static inline int release_version_cmp(const char *a, const char *b) {
    int na[3], nb[3];
    const char *sa, *sb;
    ver_split(a, na, &sa);
    ver_split(b, nb, &sb);
    for (int i = 0; i < 3; i++)
        if (na[i] != nb[i]) return na[i] > nb[i] ? 1 : -1;
    if ((*sa == '\0') != (*sb == '\0')) return (*sa == '\0') ? 1 : -1;
    return strcasecmp(sa, sb);
}


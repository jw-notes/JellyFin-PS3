#pragma once

// Search: the recent-searches list and the result type filter.
//
// Pure C with no PS3 includes, so tests/test_search_model.c compiles this
// exact file; ui_search.cpp holds the state and does the file I/O.
//
// RECENT SEARCHES are the terms whose results the user actually opened -- not
// every term the live search fired, which while typing "batman" would also
// record "ba", "bat" and "batm".  Newest first, at most SEARCH_RECENT_MAX, a
// repeat moves to the front (compared without case), and they persist as one
// term per line.
//
// THE FILTER narrows the results already fetched to one kind of item.  It is
// done here rather than by asking the server again because a search on a
// large library can take up to ~10 s to answer (ui_search.cpp), and a filter
// that waited for that would not feel like a filter.

#include <string.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEARCH_RECENT_MAX 8
#define SEARCH_TERM_MAX   64

typedef struct {
    char term[SEARCH_RECENT_MAX][SEARCH_TERM_MAX];
    int  n;
} search_recent;

static inline int sr_same(const char *a, const char *b) {
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

// ---- editing the term at a cursor ----------------------------------------------
// The cursor is a byte offset that never lands inside a UTF-8 character (a term
// brought back from the recent list can hold any).

static inline void search_edit_clamp(const char *buf, int *cur) {
    const int n = (int)strlen(buf);
    if (*cur < 0) *cur = 0;
    if (*cur > n) *cur = n;
    while (*cur > 0 && ((unsigned char)buf[*cur] & 0xC0) == 0x80) (*cur)--;
}

static inline void search_edit_move(const char *buf, int *cur, int dir) {
    search_edit_clamp(buf, cur);
    if (dir < 0 && *cur > 0) {
        (*cur)--;
        while (*cur > 0 && ((unsigned char)buf[*cur] & 0xC0) == 0x80) (*cur)--;
    } else if (dir > 0 && buf[*cur]) {
        (*cur)++;
        while (buf[*cur] && ((unsigned char)buf[*cur] & 0xC0) == 0x80) (*cur)++;
    }
}

// One byte at the cursor, which moves on past it.  Nothing when the buffer is full.
static inline void search_edit_insert(char *buf, int cap, int *cur, char ch) {
    search_edit_clamp(buf, cur);
    const int n = (int)strlen(buf);
    if (n + 1 >= cap) return;
    memmove(buf + *cur + 1, buf + *cur, (size_t)(n - *cur + 1));
    buf[*cur] = ch;
    (*cur)++;
}

// The whole character left of the cursor.
static inline void search_edit_backspace(char *buf, int *cur) {
    search_edit_clamp(buf, cur);
    const int to = *cur;
    if (to == 0) return;
    int at = to;
    search_edit_move(buf, &at, -1);
    memmove(buf + at, buf + to, strlen(buf + to) + 1);
    *cur = at;
}

// A whole key's text at the cursor: all of it, or nothing when it does not fit.
static inline void search_edit_insert_text(char *buf, int cap, int *cur, const char *text) {
    const int t = (int)strlen(text);
    if ((int)strlen(buf) + t >= cap) return;
    for (int i = 0; i < t; i++) search_edit_insert(buf, cap, cur, text[i]);
}

// What a key types (the keys are written in capitals): R2's lower case and L2's
// accents, as on the PS3's keyboard.  out holds a character and its terminator.
static inline void search_key_text(char base, int lower, int accent, char out[3]) {
    static const char from[] = "AEIOUNC";
    static const unsigned char low[] = { 0xA0, 0xA9, 0xAD, 0xB3, 0xBA, 0xB1, 0xA7 };
    const char *at = accent && base ? strchr(from, base) : NULL;
    if (at) {
        const unsigned char l = low[at - from];
        out[0] = (char)0xC3;
        out[1] = (char)(lower ? l : l - 0x20);
        out[2] = '\0';
        return;
    }
    out[0] = lower ? (char)tolower((unsigned char)base) : base;
    out[1] = '\0';
}

// Copy `term` without surrounding spaces into out (cap bytes); 0 when empty.
static inline int sr_trim(const char *term, char *out, int cap) {
    if (!term || cap <= 0) return 0;
    while (*term == ' ' || *term == '\t' || *term == '\r' || *term == '\n') term++;
    int n = 0;
    while (term[n] && term[n] != '\r' && term[n] != '\n' && n < cap - 1) { out[n] = term[n]; n++; }
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) n--;
    // Never end on half a UTF-8 character: if the last character's bytes are
    // not all there (the term was cut at cap), drop it.
    if (n > 0) {
        int lead = n - 1;
        while (lead > 0 && ((unsigned char)out[lead] & 0xC0) == 0x80) lead--;
        const unsigned char c = (unsigned char)out[lead];
        const int need = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        if (n - lead < need) n = lead;
    }
    out[n] = '\0';
    return n;
}

// Put a term at the front.  Returns 1 if the list changed.
static inline int search_recent_add(search_recent *r, const char *term) {
    char t[SEARCH_TERM_MAX];
    if (!sr_trim(term, t, (int)sizeof t)) return 0;
    int at = -1;
    for (int i = 0; i < r->n; i++) if (sr_same(r->term[i], t)) { at = i; break; }
    if (at == 0 && strcmp(r->term[0], t) == 0) return 0;      // already first, as typed
    if (at < 0) at = r->n < SEARCH_RECENT_MAX ? r->n++ : SEARCH_RECENT_MAX - 1;
    for (int i = at; i > 0; i--) memcpy(r->term[i], r->term[i - 1], SEARCH_TERM_MAX);
    memcpy(r->term[0], t, sizeof t);
    return 1;
}

static inline void search_recent_remove(search_recent *r, int i) {
    if (i < 0 || i >= r->n) return;
    for (int k = i; k < r->n - 1; k++) memcpy(r->term[k], r->term[k + 1], SEARCH_TERM_MAX);
    r->n--;
}

// One term per line; blank lines and duplicates are dropped, extras ignored.
static inline void search_recent_parse(search_recent *r, const char *text) {
    r->n = 0;
    search_recent tmp; tmp.n = 0;
    const char *p = text;
    while (p && *p && tmp.n < SEARCH_RECENT_MAX) {
        const char *e = p;
        while (*e && *e != '\n') e++;
        char line[SEARCH_TERM_MAX];
        int len = (int)(e - p) < SEARCH_TERM_MAX - 1 ? (int)(e - p) : SEARCH_TERM_MAX - 1;
        memcpy(line, p, (size_t)len); line[len] = '\0';
        char t[SEARCH_TERM_MAX];
        if (sr_trim(line, t, (int)sizeof t)) {
            int dup = 0;
            for (int i = 0; i < tmp.n; i++) if (sr_same(tmp.term[i], t)) dup = 1;
            if (!dup) memcpy(tmp.term[tmp.n++], t, sizeof t);
        }
        p = *e ? e + 1 : e;
    }
    *r = tmp;
}

// The file's contents into out (cap bytes); returns the length.
static inline int search_recent_format(const search_recent *r, char *out, int cap) {
    int len = 0;
    if (cap > 0) out[0] = '\0';
    for (int i = 0; i < r->n; i++) {
        const int tl = (int)strlen(r->term[i]);
        if (len + tl + 2 > cap) break;
        memcpy(out + len, r->term[i], (size_t)tl);
        len += tl;
        out[len++] = '\n';
        out[len] = '\0';
    }
    return len;
}

// --- the filter --------------------------------------------------------------

enum { SF_ALL = 0, SF_MOVIES, SF_SHOWS, SF_EPISODES, SF__COUNT };

// Does an item of this Jellyfin type belong under filter f?
static inline int search_filter_match(int f, const char *type) {
    switch (f) {
    case SF_MOVIES:   return strcmp(type, "Movie") == 0;
    case SF_SHOWS:    return strcmp(type, "Series") == 0;
    case SF_EPISODES: return strcmp(type, "Episode") == 0;
    default:          return 1;
    }
}

// The next filter in direction dir (+1/-1) that has any results, given each
// filter's count; All always qualifies, so this always lands somewhere.
static inline int search_filter_step(int f, int dir, const int counts[SF__COUNT]) {
    for (int k = 1; k <= SF__COUNT; k++) {
        const int g = ((f + dir * k) % SF__COUNT + SF__COUNT) % SF__COUNT;
        if (g == SF_ALL || counts[g] > 0) return g;
    }
    return SF_ALL;
}

#ifdef __cplusplus
}
#endif

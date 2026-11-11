// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
#pragma once
// Right-to-left subtitle lines (Hebrew, Arabic, Persian) for a renderer that
// only draws left to right: one line in, in the order it is read, out in the
// order its glyphs go on screen, left to right.  Host-tested
// (tests/test_rtl_text.c).
//
// Two jobs, both cut down to what a subtitle line needs:
//
//   * ARABIC SHAPING.  An Arabic letter has up to four shapes, chosen by
//     whether it joins the letter before it and the one after.  stb_truetype
//     has no shaping engine, so each letter is swapped for its shape's code
//     point in the Presentation Forms blocks (U+FB50.., U+FE70..), which the
//     subtitle Arabic font carries.  Lam + alef becomes its ligature.
//   * BIDI ORDER.  A cut-down Unicode Bidirectional Algorithm (UAX #9) for one
//     paragraph with no explicit embeddings: the line's direction from its
//     first strong letter, numbers kept left to right, neutrals (spaces,
//     punctuation) taking the direction of the text around them, then runs
//     reversed by level (rule L2) and brackets mirrored in right-to-left runs.
//
// Combining marks (Arabic harakat, Hebrew points) are dropped: drawn as
// spacing glyphs they would land beside their letter, not on it.  Subtitles
// rarely carry them.
//
// Pure C, no allocation.  Input is UTF-8; malformed bytes pass through as
// Latin-1 code points, as the renderer's own decoder treats them.

#include <stdbool.h>
#include <stddef.h>

#define RTL_MAX_CPS 512

static inline int rtl_utf8_next(const char **pp) {
    const unsigned char *s = (const unsigned char *)*pp;
    unsigned c0 = s[0];
    int n, cp, lo;
    if      (c0 < 0x80)                { *pp += 1; return (int)c0; }
    else if (c0 >= 0xC2 && c0 <= 0xDF) { n = 1; cp = c0 & 0x1F; lo = 0x80; }
    else if (c0 >= 0xE0 && c0 <= 0xEF) { n = 2; cp = c0 & 0x0F; lo = 0x800; }
    else if (c0 >= 0xF0 && c0 <= 0xF4) { n = 3; cp = c0 & 0x07; lo = 0x10000; }
    else                               { *pp += 1; return (int)c0; }
    for (int i = 1; i <= n; i++) {
        unsigned ci = s[i];
        if ((ci & 0xC0) != 0x80) { *pp += 1; return (int)c0; }
        cp = (cp << 6) | (int)(ci & 0x3F);
    }
    if (cp < lo || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) { *pp += 1; return (int)c0; }
    *pp += n + 1;
    return cp;
}

// Writes cp at out[n] if it fits (leaving room for the NUL); returns the new length.
static inline int rtl_utf8_put(char *out, int n, int cap, int cp) {
    char b[4]; int k;
    if (cp < 0x80)         { b[0] = (char)cp; k = 1; }
    else if (cp < 0x800)   { b[0] = (char)(0xC0 | (cp >> 6)); b[1] = (char)(0x80 | (cp & 0x3F)); k = 2; }
    else if (cp < 0x10000) { b[0] = (char)(0xE0 | (cp >> 12)); b[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                             b[2] = (char)(0x80 | (cp & 0x3F)); k = 3; }
    else                   { b[0] = (char)(0xF0 | (cp >> 18)); b[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                             b[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); b[3] = (char)(0x80 | (cp & 0x3F)); k = 4; }
    if (n + k >= cap) return n;
    for (int i = 0; i < k; i++) out[n + i] = b[i];
    return n + k;
}

// ---- classes ------------------------------------------------------------------

static inline bool rtl_is_hebrew_letter(int c) {
    return (c >= 0x05D0 && c <= 0x05EA) || (c >= 0x05EF && c <= 0x05F4) ||
           (c >= 0xFB1D && c <= 0xFB4F) || c == 0x05BE || c == 0x05C0 || c == 0x05C3 || c == 0x05C6;
}
static inline bool rtl_is_arabic_letter(int c) {
    // Letters and the Arabic-letter punctuation (semicolon U+061B, question mark
    // U+061F); not the comma U+060C or the digits, which are neutral / numbers.
    return c == 0x0608 || c == 0x060B || c == 0x060D || c == 0x061B || (c >= 0x061D && c <= 0x064A) ||
           (c >= 0x066D && c <= 0x066F) || (c >= 0x0671 && c <= 0x06D5) || (c >= 0x06E5 && c <= 0x06E6) ||
           (c >= 0x06EE && c <= 0x06EF) || (c >= 0x06FA && c <= 0x06FF) || (c >= 0x0750 && c <= 0x077F) ||
           (c >= 0xFB50 && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFC);
}
static inline bool rtl_is_strong_r(int c) { return rtl_is_hebrew_letter(c) || rtl_is_arabic_letter(c); }

// Marks dropped before shaping: Arabic harakat and Quranic marks, Hebrew points
// and cantillation.  Zero-width joiners go too, once shaping has used them.
static inline bool rtl_is_mark(int c) {
    return (c >= 0x064B && c <= 0x065F) || c == 0x0670 || (c >= 0x06D6 && c <= 0x06DC) ||
           (c >= 0x06DF && c <= 0x06E4) || c == 0x06E7 || c == 0x06E8 || (c >= 0x06EA && c <= 0x06ED) ||
           (c >= 0x0591 && c <= 0x05BD) || c == 0x05BF || c == 0x05C1 || c == 0x05C2 ||
           c == 0x05C4 || c == 0x05C5 || c == 0x05C7;
}

static inline bool rtl_is_digit(int c) {
    return (c >= '0' && c <= '9') || (c >= 0x0660 && c <= 0x0669) || (c >= 0x06F0 && c <= 0x06F9);
}

// Neutral: whitespace and punctuation, which take their direction from the text around them.
static inline bool rtl_is_neutral(int c) {
    if (c < 0x80) return !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'));
    return (c >= 0x00A0 && c <= 0x00BF) || c == 0x00D7 || c == 0x00F7 || (c >= 0x2000 && c <= 0x206F) ||
           c == 0x060C || (c >= 0x3000 && c <= 0x3003);
}

// Inside a number: "3.5", "1,000", "10:30", "1/2".
static inline bool rtl_is_number_sep(int c) { return c == '.' || c == ',' || c == ':' || c == '/' || c == 0x066B || c == 0x066C; }

static inline int rtl_mirror(int c) {
    switch (c) {
    case '(': return ')'; case ')': return '(';
    case '[': return ']'; case ']': return '[';
    case '{': return '}'; case '}': return '{';
    case '<': return '>'; case '>': return '<';
    case 0x00AB: return 0x00BB; case 0x00BB: return 0x00AB;
    case 0x2039: return 0x203A; case 0x203A: return 0x2039;
    default: return c;
    }
}

// ---- Arabic shaping ---------------------------------------------------------------

// Joining types: U none, R joins only to the letter before it (in reading
// order), D joins on both sides, C causes joining (tatweel, ZWJ).
enum { RTL_JU = 0, RTL_JR, RTL_JD, RTL_JC };

typedef struct { unsigned short cp, iso, fin, ini, med; } RtlForms;

// ini == 0: right-joining.  Persian letters' forms are in Presentation Forms-A.
static const RtlForms RTL_FORMS[] = {
    {0x0621, 0xFE80, 0,      0,      0},
    {0x0622, 0xFE81, 0xFE82, 0,      0},      {0x0623, 0xFE83, 0xFE84, 0,      0},
    {0x0624, 0xFE85, 0xFE86, 0,      0},      {0x0625, 0xFE87, 0xFE88, 0,      0},
    {0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C}, {0x0627, 0xFE8D, 0xFE8E, 0,      0},
    {0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92}, {0x0629, 0xFE93, 0xFE94, 0,      0},
    {0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98}, {0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C},
    {0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0}, {0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4},
    {0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8}, {0x062F, 0xFEA9, 0xFEAA, 0,      0},
    {0x0630, 0xFEAB, 0xFEAC, 0,      0},      {0x0631, 0xFEAD, 0xFEAE, 0,      0},
    {0x0632, 0xFEAF, 0xFEB0, 0,      0},      {0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4},
    {0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8}, {0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC},
    {0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0}, {0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4},
    {0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8}, {0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC},
    {0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0}, {0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4},
    {0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8}, {0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC},
    {0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0}, {0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4},
    {0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8}, {0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC},
    {0x0648, 0xFEED, 0xFEEE, 0,      0},      {0x0649, 0xFEEF, 0xFEF0, 0xFBE8, 0xFBE9},
    {0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4},
    {0x067E, 0xFB56, 0xFB57, 0xFB58, 0xFB59}, {0x0686, 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D},
    {0x0698, 0xFB8A, 0xFB8B, 0,      0},      {0x06A9, 0xFB8E, 0xFB8F, 0xFB90, 0xFB91},
    {0x06AF, 0xFB92, 0xFB93, 0xFB94, 0xFB95}, {0x06CC, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF},
};

static inline const RtlForms *rtl_forms(int c) {
    if (c < 0x0621 || c > 0x06CC) return NULL;
    for (size_t i = 0; i < sizeof RTL_FORMS / sizeof RTL_FORMS[0]; i++)
        if (RTL_FORMS[i].cp == c) return &RTL_FORMS[i];
    return NULL;
}

static inline int rtl_join_type(int c) {
    if (c == 0x0640 || c == 0x200D) return RTL_JC;
    const RtlForms *f = rtl_forms(c);
    if (!f) return RTL_JU;
    if (f->ini) return RTL_JD;
    return f->fin ? RTL_JR : RTL_JU;      // hamza joins neither way
}

// Lam + this alef -> {isolated, final} ligature, or 0.
static inline int rtl_lam_alef(int alef, bool fin) {
    switch (alef) {
    case 0x0622: return fin ? 0xFEF6 : 0xFEF5;
    case 0x0623: return fin ? 0xFEF8 : 0xFEF7;
    case 0x0625: return fin ? 0xFEFA : 0xFEF9;
    case 0x0627: return fin ? 0xFEFC : 0xFEFB;
    default:     return 0;
    }
}

// Logical order; `out` holds at least n.  Returns the new length.
static inline int rtl_shape_arabic(const int *c, int n, int *out) {
    int o = 0;
    for (int i = 0; i < n; i++) {
        const int t     = rtl_join_type(c[i]);
        const int prevt = i > 0     ? rtl_join_type(c[i - 1]) : RTL_JU;
        const int nextt = i + 1 < n ? rtl_join_type(c[i + 1]) : RTL_JU;
        const bool prev_joins = (t == RTL_JD || t == RTL_JR) && (prevt == RTL_JD || prevt == RTL_JC);
        if (c[i] == 0x0644 && i + 1 < n) {
            const int lig = rtl_lam_alef(c[i + 1], prev_joins);
            if (lig) { out[o++] = lig; i++; continue; }
        }
        const RtlForms *f = rtl_forms(c[i]);
        if (!f) { out[o++] = c[i]; continue; }
        const bool next_joins = t == RTL_JD && (nextt == RTL_JD || nextt == RTL_JR || nextt == RTL_JC);
        int form;
        if (prev_joins && next_joins) form = f->med;
        else if (prev_joins)          form = f->fin;
        else if (next_joins)          form = f->ini;
        else                          form = f->iso;
        out[o++] = form ? form : f->iso;
    }
    // The joiners have done their work.
    int k = 0;
    for (int i = 0; i < o; i++) if (out[i] != 0x200D && out[i] != 0x200C) out[k++] = out[i];
    return k;
}

// ---- bidi ---------------------------------------------------------------------------

enum { RTL_DL = 0, RTL_DR, RTL_DN, RTL_DNUM };

// Converts one line (no '\n') to display order.  False, with `out` untouched,
// when the line has nothing right to left: the caller draws it as it is.
static inline bool rtl_visual_line(const char *in, char *out, int cap) {
    if (!in || !out || cap <= 0) return false;
    int raw[RTL_MAX_CPS], c[RTL_MAX_CPS], n = 0;
    bool any_r = false;
    for (const char *p = in; *p && n < RTL_MAX_CPS; ) {
        const int cp = rtl_utf8_next(&p);
        if (rtl_is_mark(cp)) continue;
        if (rtl_is_strong_r(cp)) any_r = true;
        raw[n++] = cp;
    }
    if (!any_r) return false;
    n = rtl_shape_arabic(raw, n, c);

    unsigned char d[RTL_MAX_CPS], lv[RTL_MAX_CPS];
    for (int i = 0; i < n; i++)
        d[i] = rtl_is_strong_r(c[i]) ? RTL_DR : rtl_is_digit(c[i]) ? RTL_DNUM
             : rtl_is_neutral(c[i]) ? RTL_DN : RTL_DL;
    // A separator between two digits is part of the number.
    for (int i = 1; i + 1 < n; i++)
        if (d[i] == RTL_DN && rtl_is_number_sep(c[i]) && d[i - 1] == RTL_DNUM && d[i + 1] == RTL_DNUM)
            d[i] = RTL_DNUM;

    int base = RTL_DR;                     // P2/P3: the first strong letter
    for (int i = 0; i < n; i++) if (d[i] == RTL_DL || d[i] == RTL_DR) { base = d[i]; break; }

    // W2/W7: a number after left-to-right text (or at the start of a
    // left-to-right line) is just left-to-right text; after right-to-left text
    // it stays a number, which reads left to right inside the right-to-left run.
    {
        int strong = base;
        for (int i = 0; i < n; i++) {
            if (d[i] == RTL_DL || d[i] == RTL_DR) strong = d[i];
            else if (d[i] == RTL_DNUM && strong == RTL_DL) d[i] = RTL_DL;
        }
    }
    // N1/N2: a run of neutrals between two of the same direction takes it
    // (a number counts as right to left here); otherwise the line's.
    for (int i = 0; i < n; ) {
        if (d[i] != RTL_DN) { i++; continue; }
        int j = i;
        while (j < n && d[j] == RTL_DN) j++;
        const int before = i > 0 ? (d[i - 1] == RTL_DNUM ? RTL_DR : d[i - 1]) : base;
        const int after  = j < n ? (d[j]     == RTL_DNUM ? RTL_DR : d[j])     : base;
        const int r = before == after ? before : base;
        for (int k = i; k < j; k++) d[k] = (unsigned char)r;
        i = j;
    }
    // I1/I2: levels.
    const int b = base == RTL_DR ? 1 : 0;
    int maxl = 0;
    for (int i = 0; i < n; i++) {
        int l;
        if (d[i] == RTL_DNUM) l = 2;
        else if (b == 0)      l = d[i] == RTL_DR ? 1 : 0;
        else                  l = d[i] == RTL_DR ? 1 : 2;
        lv[i] = (unsigned char)l;
        if (l > maxl) maxl = l;
    }
    // L1: trailing whitespace goes back to the line's level.
    for (int i = n - 1; i >= 0 && (c[i] == ' ' || c[i] == '\t'); i--) lv[i] = (unsigned char)b;

    // L4 before L2, on the logical order: mirror brackets in right-to-left runs.
    for (int i = 0; i < n; i++) if (lv[i] & 1) c[i] = rtl_mirror(c[i]);

    // L2: from the highest level down to the lowest odd one, reverse every run at or above it.
    for (int l = maxl; l >= 1; l--) {
        for (int i = 0; i < n; ) {
            if (lv[i] < l) { i++; continue; }
            int j = i;
            while (j < n && lv[j] >= l) j++;
            for (int a = i, z = j - 1; a < z; a++, z--) {
                const int t = c[a]; c[a] = c[z]; c[z] = t;
                const unsigned char u = lv[a]; lv[a] = lv[z]; lv[z] = u;
            }
            i = j;
        }
    }

    int o = 0;
    for (int i = 0; i < n; i++) o = rtl_utf8_put(out, o, cap, c[i]);
    out[o] = '\0';
    return true;
}

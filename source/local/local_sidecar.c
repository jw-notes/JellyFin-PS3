// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
// Subtitle files beside a video: see local_sidecar.h.

#include "local_sidecar.h"
#include "lang_names.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static bool ends_with_ci(const char *s, size_t n, const char *suffix) {
    const size_t k = strlen(suffix);
    return n >= k && strncasecmp(s + n - k, suffix, k) == 0;
}

bool sidecar_match(const char *video, const char *file, char *tag, int tag_cap) {
    if (tag && tag_cap > 0) tag[0] = '\0';
    if (!video || !file) return false;
    const char *dot = strrchr(video, '.');
    const size_t stem = dot && dot != video ? (size_t)(dot - video) : strlen(video);
    const size_t fn = strlen(file);
    if (stem == 0 || !ends_with_ci(file, fn, ".srt")) return false;
    if (fn < stem + 4 || strncasecmp(file, video, stem) != 0) return false;
    const size_t rest = fn - stem - 4;                  // what lies between the stem and ".srt"
    if (rest == 0) return true;                         // "Film.srt"
    if (file[stem] != '.' || rest < 2) return false;    // "Film2.srt" is another film's; "Film..srt" says nothing
    if (tag && tag_cap > 0) {
        size_t n = rest - 1;
        if (n > (size_t)tag_cap - 1) n = (size_t)tag_cap - 1;
        memcpy(tag, file + stem + 1, n);
        tag[n] = '\0';
    }
    return true;
}

// Language words and codes people use in file names that are not the ISO ones lang_name() knows.
static const struct { const char *word; const char *code; } ALIASES[] = {
    { "jp", "ja" }, { "gk", "el" }, { "gr", "el" }, { "ch", "zh" }, { "cn", "zh" }, { "cz", "cs" }, { "dk", "da" },
    { "iw", "he" }, { "kr", "ko" }, { "ua", "uk" }, { "br", "pt" }, { "se", "sv" }, { "farsi", "fa" },
    { "english", "en" }, { "french", "fr" }, { "german", "de" }, { "spanish", "es" }, { "italian", "it" },
    { "portuguese", "pt" }, { "dutch", "nl" }, { "russian", "ru" }, { "polish", "pl" }, { "swedish", "sv" },
    { "norwegian", "no" }, { "danish", "da" }, { "finnish", "fi" }, { "japanese", "ja" }, { "chinese", "zh" },
    { "korean", "ko" }, { "arabic", "ar" }, { "hebrew", "he" }, { "turkish", "tr" }, { "greek", "el" },
    { "hungarian", "hu" }, { "czech", "cs" }, { "slovak", "sk" }, { "romanian", "ro" }, { "bulgarian", "bg" },
    { "ukrainian", "uk" }, { "croatian", "hr" }, { "serbian", "sr" }, { "slovenian", "sl" }, { "hindi", "hi" },
    { "thai", "th" }, { "vietnamese", "vi" }, { "indonesian", "id" }, { "malay", "ms" }, { "persian", "fa" },
    { "catalan", "ca" }, { "icelandic", "is" },
    { "dansk", "da" }, { "deutsch", "de" }, { "norsk", "no" }, { "svenska", "sv" }, { "espanol", "es" },
    { "francais", "fr" }, { "italiano", "it" }, { "portugues", "pt" }, { "nederlands", "nl" }, { "polski", "pl" },
    { "suomi", "fi" }, { "magyar", "hu" }, { "cesky", "cs" }, { "hrvatski", "hr" },
};

// The language a word names, or false.
static bool language_of(const char *word, char *out, int cap) {
    char w[24];
    size_t n = 0;
    for (; word[n] && n < sizeof w - 1; n++) w[n] = (char)tolower((unsigned char)word[n]);
    w[n] = '\0';
    if (n == 0) return false;
    const char *code = w;
    for (size_t i = 0; i < sizeof ALIASES / sizeof ALIASES[0]; i++)
        if (!strcmp(w, ALIASES[i].word)) { code = ALIASES[i].code; break; }
    char name[40];
    lang_name(code, name, sizeof name);
    // lang_name gives an unknown code back upper-cased: that is no language
    char up[24];
    size_t k = 0;
    for (; code[k] && k < sizeof up - 1; k++) up[k] = (char)toupper((unsigned char)code[k]);
    up[k] = '\0';
    if (!strcmp(name, up) || !strcmp(name, "Unknown")) return false;
    snprintf(out, (size_t)cap, "%s", name);
    return true;
}

void sidecar_label(const char *tag, char *out, int cap) {
    if (!out || cap <= 0) return;
    if (!tag || !tag[0]) { snprintf(out, (size_t)cap, "Subtitles"); return; }
    // the first word, then whatever follows it
    size_t n = 0;
    while (tag[n] && tag[n] != '-' && tag[n] != '_' && tag[n] != '.' && tag[n] != ' ') n++;
    char first[24];
    const size_t fl = n < sizeof first - 1 ? n : sizeof first - 1;
    memcpy(first, tag, fl);
    first[fl] = '\0';
    const char *rest = tag[n] ? tag + n + 1 : "";
    char lang[40];
    if (language_of(first, lang, sizeof lang)) {
        if (rest[0]) snprintf(out, (size_t)cap, "%s - %s", lang, rest);
        else         snprintf(out, (size_t)cap, "%s", lang);
    } else {
        snprintf(out, (size_t)cap, "%s", tag);
        if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 'a' + 'A');
    }
}

// ---- text ---------------------------------------------------------------------------------------------

static int put_utf8(char *out, int o, int cap, uint32_t cp) {
    char b[4]; int k;
    if (cp < 0x80)         { b[0] = (char)cp; k = 1; }
    else if (cp < 0x800)   { b[0] = (char)(0xC0 | (cp >> 6)); b[1] = (char)(0x80 | (cp & 0x3F)); k = 2; }
    else if (cp < 0x10000) { b[0] = (char)(0xE0 | (cp >> 12)); b[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                             b[2] = (char)(0x80 | (cp & 0x3F)); k = 3; }
    else                   { b[0] = (char)(0xF0 | (cp >> 18)); b[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                             b[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); b[3] = (char)(0x80 | (cp & 0x3F)); k = 4; }
    if (o + k >= cap) return -1;
    memcpy(out + o, b, (size_t)k);
    return o + k;
}

static bool valid_utf8(const uint8_t *p, int n) {
    for (int i = 0; i < n; ) {
        const uint8_t c = p[i];
        int k;
        if (c < 0x80) { i++; continue; }
        else if (c >= 0xC2 && c <= 0xDF) k = 1;
        else if (c >= 0xE0 && c <= 0xEF) k = 2;
        else if (c >= 0xF0 && c <= 0xF4) k = 3;
        else return false;
        for (int j = 1; j <= k; j++) if (i + j >= n || (p[i + j] & 0xC0) != 0x80) return false;
        i += k + 1;
    }
    return true;
}

// Windows-1252's 0x80-0x9F (0 = undefined, kept as the Latin-1 control).
static const uint16_t CP1252[32] = {
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178,
};

int sidecar_to_utf8(const uint8_t *in, int len, char *out, int cap) {
    if (!out || cap <= 0) return 0;
    out[0] = '\0';
    if (!in || len <= 0) return 0;
    int o = 0;
    if (len >= 2 && ((in[0] == 0xFF && in[1] == 0xFE) || (in[0] == 0xFE && in[1] == 0xFF))) {
        const bool le = in[0] == 0xFF;
        for (int i = 2; i + 1 < len; i += 2) {
            uint32_t u = le ? (uint32_t)(in[i] | (in[i + 1] << 8)) : (uint32_t)((in[i] << 8) | in[i + 1]);
            if (u >= 0xD800 && u <= 0xDBFF && i + 3 < len) {            // a surrogate pair
                const uint32_t lo = le ? (uint32_t)(in[i + 2] | (in[i + 3] << 8)) : (uint32_t)((in[i + 2] << 8) | in[i + 3]);
                if (lo >= 0xDC00 && lo <= 0xDFFF) { u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00); i += 2; }
                else u = 0xFFFD;
            } else if (u >= 0xD800 && u <= 0xDFFF) {
                u = 0xFFFD;
            }
            const int w = put_utf8(out, o, cap, u);
            if (w < 0) break;
            o = w;
        }
    } else {
        int start = 0;
        if (len >= 3 && in[0] == 0xEF && in[1] == 0xBB && in[2] == 0xBF) start = 3;
        if (valid_utf8(in + start, len - start)) {
            int n = len - start;
            if (n > cap - 1) {
                n = cap - 1;
                while (n > 0 && (in[start + n] & 0xC0) == 0x80) n--;    // not inside a character
            }
            memcpy(out, in + start, (size_t)n);
            o = n;
        } else {
            for (int i = start; i < len; i++) {
                const uint8_t c = in[i];
                const uint32_t u = c < 0x80 ? c : (c < 0xA0 && CP1252[c - 0x80]) ? CP1252[c - 0x80] : c;
                const int w = put_utf8(out, o, cap, u);
                if (w < 0) break;
                o = w;
            }
        }
    }
    out[o] = '\0';
    return o;
}

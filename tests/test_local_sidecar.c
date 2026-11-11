// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
// Host test for source/local/local_sidecar.c: subtitle files beside a video.
//
//   make -f Makefile.host test_local_sidecar && ./test_local_sidecar

#include "local_sidecar.h"

#include <stdio.h>
#include <string.h>

static int s_checks = 0, s_failed = 0;
#define CHECK(cond) do { s_checks++; if (!(cond)) { s_failed++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static bool label_is(const char *tag, const char *want) {
    char out[64];
    sidecar_label(tag, out, sizeof out);
    if (strcmp(out, want)) printf("  label(%s) = '%s', want '%s'\n", tag, out, want);
    return strcmp(out, want) == 0;
}

int main(void) {
    char tag[32];
    const char *v = "tears_of_steel_720p.mov";

    // which files belong to the video
    CHECK(sidecar_match(v, "tears_of_steel_720p.srt", tag, sizeof tag) && tag[0] == 0);
    CHECK(sidecar_match(v, "tears_of_steel_720p.eng.srt", tag, sizeof tag) && !strcmp(tag, "eng"));
    CHECK(sidecar_match(v, "TEARS_OF_STEEL_720P.ch-traditional.SRT", tag, sizeof tag) && !strcmp(tag, "ch-traditional"));
    CHECK(sidecar_match(v, "tears_of_steel_720p.pl-please-review.srt", tag, sizeof tag) && !strcmp(tag, "pl-please-review"));
    CHECK(!sidecar_match(v, "tears_of_steel_720p.mov", tag, sizeof tag));
    CHECK(!sidecar_match(v, "tears_of_steel_720p2.srt", tag, sizeof tag));      // another film
    CHECK(!sidecar_match(v, "tears_of_steel.srt", tag, sizeof tag));
    CHECK(!sidecar_match(v, "tears_of_steel_720p.eng.ass", tag, sizeof tag));
    CHECK(!sidecar_match(v, "tears_of_steel_720p..srt", tag, sizeof tag));
    CHECK(!sidecar_match(v, ".srt", tag, sizeof tag));
    CHECK(sidecar_match("Film.mkv", "Film.en.srt", NULL, 0));
    CHECK(sidecar_match("Film", "Film.srt", tag, sizeof tag));                  // a name with no extension
    // a long tag is cut, never overrun
    {
        char small[4];
        CHECK(sidecar_match(v, "tears_of_steel_720p.abcdefghij.srt", small, sizeof small) && !strcmp(small, "abc"));
    }

    // what the menu calls them (the names in the test folder)
    CHECK(label_is("", "Subtitles"));
    CHECK(label_is("eng", "English"));
    CHECK(label_is("deu", "German"));
    CHECK(label_is("ch", "Chinese"));
    CHECK(label_is("ch-traditional", "Chinese - traditional"));
    CHECK(label_is("croatian", "Croatian"));
    CHECK(label_is("cz", "Czech"));
    CHECK(label_is("dansk", "Danish"));
    CHECK(label_is("fr-goofy", "French - goofy"));
    CHECK(label_is("fr-orig", "French - orig"));
    CHECK(label_is("gk", "Greek"));
    CHECK(label_is("he", "Hebrew"));
    CHECK(label_is("hu", "Hungarian"));
    CHECK(label_is("indonesian", "Indonesian"));
    CHECK(label_is("jp", "Japanese"));
    CHECK(label_is("nor", "Norwegian"));
    CHECK(label_is("persian", "Persian"));
    CHECK(label_is("pl-please-review", "Polish - please-review"));
    CHECK(label_is("pt-pt", "Portuguese - pt"));
    CHECK(label_is("rus", "Russian"));
    CHECK(label_is("forced", "Forced"));                 // no language: the tag itself
    CHECK(label_is("sdh", "Sdh"));

    // text: UTF-8 as it is, a BOM dropped
    char out[256];
    {
        const char *s = "1\n00:00:01,000 --> 00:00:02,000\nh\xC3\xA9llo\n";
        CHECK(sidecar_to_utf8((const uint8_t *)s, (int)strlen(s), out, sizeof out) == (int)strlen(s));
        CHECK(!strcmp(out, s));
        const uint8_t bom[] = { 0xEF, 0xBB, 0xBF, '1', '\n' };
        CHECK(sidecar_to_utf8(bom, sizeof bom, out, sizeof out) == 2 && !strcmp(out, "1\n"));
    }
    // UTF-16 LE and BE with a BOM (the Norwegian file is LE), and a surrogate pair
    {
        const uint8_t le[] = { 0xFF, 0xFE, '1', 0, '\r', 0, '\n', 0, 0xE6, 0x00, 0x3D, 0xD8, 0x00, 0xDE };   // "1\r\næ" U+1F600
        CHECK(sidecar_to_utf8(le, sizeof le, out, sizeof out) > 0);
        CHECK(!strcmp(out, "1\r\n\xC3\xA6\xF0\x9F\x98\x80"));
        const uint8_t be[] = { 0xFE, 0xFF, 0x05, 0xD0, 0x00, 'x' };         // Hebrew alef, x
        sidecar_to_utf8(be, sizeof be, out, sizeof out);
        CHECK(!strcmp(out, "\xD7\x90x"));
        const uint8_t lone[] = { 0xFF, 0xFE, 0x00, 0xD8, 'a', 0 };          // an unpaired surrogate
        sidecar_to_utf8(lone, sizeof lone, out, sizeof out);
        CHECK(!strcmp(out, "\xEF\xBF\xBD" "a"));
    }
    // not UTF-8: Windows-1252
    {
        const uint8_t w[] = { 'c', 'a', 'f', 0xE9, ' ', 0x93, 'q', 0x94, ' ', 0x80 };
        sidecar_to_utf8(w, sizeof w, out, sizeof out);
        CHECK(!strcmp(out, "caf\xC3\xA9 \xE2\x80\x9Cq\xE2\x80\x9D \xE2\x82\xAC"));
    }
    // a small output is cut on a character, and terminated
    {
        const char *s = "ab\xC3\xA9\xC3\xA9";
        char tiny[5];
        const int n = sidecar_to_utf8((const uint8_t *)s, (int)strlen(s), tiny, sizeof tiny);
        CHECK(n == 4 && !strcmp(tiny, "ab\xC3\xA9"));
        const uint8_t le[] = { 0xFF, 0xFE, 'a', 0, 'b', 0, 'c', 0, 'd', 0, 'e', 0 };
        CHECK(sidecar_to_utf8(le, sizeof le, tiny, sizeof tiny) == 4 && !strcmp(tiny, "abcd"));
        CHECK(sidecar_to_utf8(NULL, 5, tiny, sizeof tiny) == 0 && tiny[0] == 0);
        const uint8_t odd[] = { 0xFF, 0xFE, 'a' };                          // UTF-16 cut mid-character
        CHECK(sidecar_to_utf8(odd, sizeof odd, tiny, sizeof tiny) == 0);
        const uint8_t trunc[] = { 'a', 0xE2, 0x82 };                       // UTF-8 cut mid-character: read as 1252
        CHECK(sidecar_to_utf8(trunc, sizeof trunc, out, sizeof out) > 0);
    }

    printf("local_sidecar: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}

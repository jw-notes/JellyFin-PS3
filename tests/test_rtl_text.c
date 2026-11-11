// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
// Host test for source/util/rtl_text.h: right-to-left subtitle lines.
//
//   make -f Makefile.host test_rtl_text && ./test_rtl_text

#include "rtl_text.h"

#include <stdio.h>
#include <string.h>

static int s_checks = 0, s_failed = 0;
#define CHECK(cond) do { s_checks++; if (!(cond)) { s_failed++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

// UTF-8 from code points (0-terminated list).
static const char *u8(const int *cps) {
    static char buf[4][512];
    static int k = 0;
    char *b = buf[k++ & 3];
    int n = 0;
    for (; *cps; cps++) n = rtl_utf8_put(b, n, 512, *cps);
    b[n] = '\0';
    return b;
}

// The display order of `in`, as code points, equals `want`.
static bool visual_is(const int *in, const int *want) {
    char out[512];
    if (!rtl_visual_line(u8(in), out, sizeof out)) return false;
    return strcmp(out, u8(want)) == 0;
}

#define SHIN 0x05E9
#define LAMED 0x05DC
#define VAV 0x05D5
#define MEM_F 0x05DD

int main(void) {
    char out[512];

    // Nothing right to left: left alone.
    strcpy(out, "untouched");
    CHECK(!rtl_visual_line("Hello, world (2024).", out, sizeof out));
    CHECK(strcmp(out, "untouched") == 0);
    CHECK(!rtl_visual_line("", out, sizeof out));

    // Hebrew: reversed; the full stop ends up on the left.
    { const int in[] = { SHIN, LAMED, VAV, MEM_F, 0 };
      const int w[]  = { MEM_F, VAV, LAMED, SHIN, 0 };
      CHECK(visual_is(in, w)); }
    { const int in[] = { SHIN, LAMED, VAV, MEM_F, '.', 0 };
      const int w[]  = { '.', MEM_F, VAV, LAMED, SHIN, 0 };
      CHECK(visual_is(in, w)); }

    // A number keeps its own order inside right-to-left text.
    { const int in[] = { 0x05D1, 0x05E9, ' ', '2', '0', '2', '4', 0 };
      const int w[]  = { '2', '0', '2', '4', ' ', 0x05E9, 0x05D1, 0 };
      CHECK(visual_is(in, w)); }
    { const int in[] = { '3', '.', '5', ' ', SHIN, 0 };
      const int w[]  = { SHIN, ' ', '3', '.', '5', 0 };
      CHECK(visual_is(in, w)); }

    // Brackets mirror inside a right-to-left run, so they still enclose.
    { const int in[] = { '(', SHIN, LAMED, ')', 0 };
      const int w[]  = { '(', LAMED, SHIN, ')', 0 };
      CHECK(visual_is(in, w)); }

    // A left-to-right line with a Hebrew word: only the word turns round.
    { const int in[] = { 'H', 'i', ' ', SHIN, LAMED, ' ', 'y', 'o', 0 };
      const int w[]  = { 'H', 'i', ' ', LAMED, SHIN, ' ', 'y', 'o', 0 };
      CHECK(visual_is(in, w)); }

    // Arabic: seen + lam-alef + meem.  Seen initial, lam-alef final (it joins
    // the seen), meem isolated (alef does not join forward); then reversed.
    { const int in[] = { 0x0633, 0x0644, 0x0627, 0x0645, 0 };
      const int w[]  = { 0xFEE1, 0xFEFC, 0xFEB3, 0 };
      CHECK(visual_is(in, w)); }
    // A dual-joining letter between two others takes its medial form: beh-beh-beh.
    { const int in[] = { 0x0628, 0x0628, 0x0628, 0 };
      const int w[]  = { 0xFE90, 0xFE92, 0xFE91, 0 };
      CHECK(visual_is(in, w)); }
    // Lam-alef at the start of a word is the isolated ligature.
    { const int in[] = { 0x0644, 0x0627, 0 };
      const int w[]  = { 0xFEFB, 0 };
      CHECK(visual_is(in, w)); }
    // Two words: the space breaks the joining.
    { const int in[] = { 0x0628, 0x0628, ' ', 0x0628, 0 };
      const int w[]  = { 0xFE8F, ' ', 0xFE90, 0xFE91, 0 };
      CHECK(visual_is(in, w)); }

    // Persian letters use Presentation Forms-A: peh-dal-reh.
    { const int in[] = { 0x067E, 0x062F, 0x0631, 0 };
      const int w[]  = { 0xFEAD, 0xFEAA, 0xFB58, 0 };
      CHECK(visual_is(in, w)); }
    // Farsi yeh in the middle of a word: beh-yeh-beh.
    { const int in[] = { 0x0628, 0x06CC, 0x0628, 0 };
      const int w[]  = { 0xFE90, 0xFBFF, 0xFE91, 0 };
      CHECK(visual_is(in, w)); }

    // Marks are dropped and do not break the joining: beh + fatha + beh.
    { const int in[] = { 0x0628, 0x064E, 0x0628, 0 };
      const int w[]  = { 0xFE90, 0xFE91, 0 };
      CHECK(visual_is(in, w)); }
    // Hebrew points are dropped too.
    { const int in[] = { SHIN, 0x05B8, LAMED, 0 };
      const int w[]  = { LAMED, SHIN, 0 };
      CHECK(visual_is(in, w)); }

    // Tatweel causes joining on both sides.
    { const int in[] = { 0x0628, 0x0640, 0x0628, 0 };
      const int w[]  = { 0xFE90, 0x0640, 0xFE91, 0 };
      CHECK(visual_is(in, w)); }

    // Hostile input: a long line is cut, never overrun; a tiny buffer still terminates.
    {
        static char big[4096];
        int n = 0;
        for (int i = 0; i < 1500; i++) n = rtl_utf8_put(big, n, (int)sizeof big, SHIN);
        big[n] = '\0';
        char small[8];
        CHECK(rtl_visual_line(big, out, sizeof out));
        CHECK(strlen(out) < sizeof out);
        CHECK(rtl_visual_line(big, small, sizeof small));
        CHECK(strlen(small) < sizeof small);
        CHECK(!rtl_visual_line("\xC3", out, sizeof out));            // truncated sequence, no RTL
        CHECK(!rtl_visual_line(NULL, out, sizeof out));
    }

    printf("rtl_text: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}

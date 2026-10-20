// Recent searches and the result filter (source/ui/search_model.h).
#include <stdio.h>
#include <string.h>
#include "../source/ui/search_model.h"

static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { g_checks++; if (!(c)) { g_fail++; printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void test_recent(void) {
    search_recent r; r.n = 0;
    CHECK(!search_recent_add(&r, ""));
    CHECK(!search_recent_add(&r, "   "));
    CHECK(r.n == 0);

    CHECK(search_recent_add(&r, "  Batman "));
    CHECK(r.n == 1 && strcmp(r.term[0], "Batman") == 0);
    search_recent_add(&r, "Alien");
    search_recent_add(&r, "Dune");
    CHECK(r.n == 3 && strcmp(r.term[0], "Dune") == 0 && strcmp(r.term[2], "Batman") == 0);

    // A repeat moves to the front, whatever its case, and keeps the new spelling.
    CHECK(search_recent_add(&r, "batman"));
    CHECK(r.n == 3 && strcmp(r.term[0], "batman") == 0);
    CHECK(strcmp(r.term[1], "Dune") == 0 && strcmp(r.term[2], "Alien") == 0);
    CHECK(!search_recent_add(&r, "batman"));          // already first: no change

    // Capped: the oldest falls off.
    for (int i = 0; i < 20; i++) { char t[24]; snprintf(t, sizeof t, "term %d", i); search_recent_add(&r, t); }
    CHECK(r.n == SEARCH_RECENT_MAX);
    CHECK(strcmp(r.term[0], "term 19") == 0);
    CHECK(strcmp(r.term[SEARCH_RECENT_MAX - 1], "term 12") == 0);

    // Remove, including out of range.
    search_recent_remove(&r, 0);
    CHECK(r.n == SEARCH_RECENT_MAX - 1 && strcmp(r.term[0], "term 18") == 0);
    search_recent_remove(&r, r.n - 1);
    CHECK(r.n == SEARCH_RECENT_MAX - 2 && strcmp(r.term[r.n - 1], "term 13") == 0);
    search_recent_remove(&r, -1);
    search_recent_remove(&r, 99);
    CHECK(r.n == SEARCH_RECENT_MAX - 2);
    while (r.n) search_recent_remove(&r, 0);
    CHECK(r.n == 0);
}

static void test_file(void) {
    search_recent r; r.n = 0;
    search_recent_add(&r, "Alien");
    search_recent_add(&r, "The Matrix");
    search_recent_add(&r, "Amélie");
    char buf[1024];
    const int len = search_recent_format(&r, buf, sizeof buf);
    CHECK(len == (int)strlen(buf));
    CHECK(strcmp(buf, "Amélie\nThe Matrix\nAlien\n") == 0);

    search_recent back;
    search_recent_parse(&back, buf);
    CHECK(back.n == 3 && strcmp(back.term[0], "Amélie") == 0 && strcmp(back.term[2], "Alien") == 0);

    // CRLF, blanks, padding and duplicates.
    search_recent_parse(&back, "\r\n  Dune \r\n\r\nDUNE\nalien\n\n");
    CHECK(back.n == 2 && strcmp(back.term[0], "Dune") == 0 && strcmp(back.term[1], "alien") == 0);

    // Too many lines: the first SEARCH_RECENT_MAX kept.
    char many[512] = "";
    for (int i = 0; i < 12; i++) { char t[16]; snprintf(t, sizeof t, "t%d\n", i); strcat(many, t); }
    search_recent_parse(&back, many);
    CHECK(back.n == SEARCH_RECENT_MAX && strcmp(back.term[0], "t0") == 0);

    // A line longer than a term is cut, never mid-character.
    char longl[200];
    memset(longl, 'a', sizeof longl); longl[sizeof longl - 1] = '\0';
    memcpy(longl + SEARCH_TERM_MAX - 2, "\xC3\xA9", 2);         // é straddling the cut
    search_recent_parse(&back, longl);
    CHECK(back.n == 1 && (int)strlen(back.term[0]) <= SEARCH_TERM_MAX - 1);
    const unsigned char last = (unsigned char)back.term[0][strlen(back.term[0]) - 1];
    CHECK(last < 0x80);

    // Empty and NULL.
    search_recent_parse(&back, "");
    CHECK(back.n == 0);
    search_recent_parse(&back, NULL);
    CHECK(back.n == 0);
    CHECK(search_recent_format(&back, buf, sizeof buf) == 0 && buf[0] == '\0');

    // A small buffer stops at whole lines.
    search_recent_add(&back, "abcdef");
    search_recent_add(&back, "ghi");
    char small[8];
    search_recent_format(&back, small, sizeof small);
    CHECK(strcmp(small, "ghi\n") == 0);
}

static void test_filter(void) {
    CHECK(search_filter_match(SF_ALL, "Movie") && search_filter_match(SF_ALL, "Episode"));
    CHECK(search_filter_match(SF_MOVIES, "Movie") && !search_filter_match(SF_MOVIES, "Series"));
    CHECK(search_filter_match(SF_SHOWS, "Series") && !search_filter_match(SF_SHOWS, "Episode"));
    CHECK(search_filter_match(SF_EPISODES, "Episode") && !search_filter_match(SF_EPISODES, "Movie"));

    int counts[SF__COUNT] = { 10, 4, 0, 6 };            // no shows
    CHECK(search_filter_step(SF_ALL, +1, counts) == SF_MOVIES);
    CHECK(search_filter_step(SF_MOVIES, +1, counts) == SF_EPISODES);   // skips Shows
    CHECK(search_filter_step(SF_EPISODES, +1, counts) == SF_ALL);      // wraps
    CHECK(search_filter_step(SF_ALL, -1, counts) == SF_EPISODES);
    CHECK(search_filter_step(SF_EPISODES, -1, counts) == SF_MOVIES);

    int none[SF__COUNT] = { 0, 0, 0, 0 };
    CHECK(search_filter_step(SF_ALL, +1, none) == SF_ALL);
    CHECK(search_filter_step(SF_MOVIES, -1, none) == SF_ALL);
}

int main(void) {
    test_recent();
    test_file();
    test_filter();
    printf("search model: %d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}

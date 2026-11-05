// Copyright (C) 2026 jw-notes.  GPLv3, see LICENSE.
// Host test for source/net/http_range.h: what a range response has to say.
//
//   make -f Makefile.host test_http_range && ./test_http_range

#include "http_range.h"

#include <stdio.h>

static int s_checks = 0, s_failed = 0;
#define CHECK(cond) do { s_checks++; if (!(cond)) { s_failed++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

int main(void) {
    uint64_t t = 0;
    // The usual answer, with a size past 4 GB.
    CHECK(http_range_parse("HTTP/1.1 206 Partial Content\r\nContent-Type: video/x-matroska\r\n"
                           "Content-Range: bytes 1000-41600000000/41600000001\r\n\r\n", 1000, &t));
    CHECK(t == 41600000001ULL);
    // Names in any case.
    CHECK(http_range_parse("HTTP/1.1 206 x\r\ncontent-range: bytes 0-9/10\r\n\r\n", 0, &t) && t == 10);
    // Not the byte asked for: playing it would be the wrong picture.
    CHECK(!http_range_parse("HTTP/1.1 206 x\r\nContent-Range: bytes 0-9/10\r\n\r\n", 5, &t));
    // No size to go by.
    CHECK(!http_range_parse("HTTP/1.1 206 x\r\nContent-Range: bytes 0-9/*\r\n\r\n", 0, &t));
    CHECK(!http_range_parse("HTTP/1.1 206 x\r\n\r\n", 0, &t));
    // A 200 is the whole file: only an answer from the start.
    CHECK(http_range_parse("HTTP/1.1 200 OK\r\nContent-Length: 77\r\n\r\n", 0, &t) && t == 77);
    CHECK(!http_range_parse("HTTP/1.1 200 OK\r\nContent-Length: 77\r\n\r\n", 5, &t));
    CHECK(!http_range_parse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n", 0, &t));
    // Errors, redirects and garbage.
    CHECK(!http_range_parse("HTTP/1.1 302 Found\r\nLocation: http://x/\r\n\r\n", 0, &t));
    CHECK(!http_range_parse("HTTP/1.1 416 Range\r\nContent-Range: bytes */10\r\n\r\n", 20, &t));
    CHECK(!http_range_parse("hello", 0, &t));
    CHECK(!http_range_parse("", 0, &t));
    // A header's name inside a value is not the header.
    CHECK(!http_range_parse("HTTP/1.1 206 x\r\nX-Note: Content-Range: bytes 0-9/10\r\n\r\n", 0, &t));

    printf("http range: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}

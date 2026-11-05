// Copyright (C) 2026 jw-notes.  GPLv3, see LICENSE.
// Host test for source/ui/home_order.h: My Media's libraries in the viewer's order.
//
//   make -f Makefile.host test_home_order && ./test_home_order

#include "home_order.h"

#include <stdio.h>

static int s_checks = 0, s_failed = 0;
#define CHECK(cond) do { s_checks++; if (!(cond)) { s_failed++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const char *const K[4] = { "aaa", "bbb", "k7", "ddd" };

static bool same(const int *a, int w, int x, int y, int z) {
    return a[0] == w && a[1] == x && a[2] == y && a[3] == z;
}

int main(void) {
    int o[4];
    // Nothing saved: as they came.
    home_order_apply("", K, 4, o);
    CHECK(same(o, 0, 1, 2, 3));
    home_order_apply(NULL, K, 4, o);
    CHECK(same(o, 0, 1, 2, 3));
    // All saved.
    home_order_apply("ddd,k7,bbb,aaa", K, 4, o);
    CHECK(same(o, 3, 2, 1, 0));
    // A new library goes after the saved ones, in its own place.
    home_order_apply("k7,aaa", K, 4, o);
    CHECK(same(o, 2, 0, 1, 3));
    // A removed one, a prefix of a real key, and a repeat are passed over.
    home_order_apply("gone,dd,bbb,bbb", K, 4, o);
    CHECK(same(o, 1, 0, 2, 3));
    // Empty entries.
    home_order_apply(",,ddd,", K, 4, o);
    CHECK(same(o, 3, 0, 1, 2));

    // Saving round-trips.
    char out[64];
    const int want[4] = { 2, 0, 3, 1 };
    CHECK(home_order_format(K, want, 4, out, sizeof out));
    CHECK(strcmp(out, "k7,aaa,ddd,bbb") == 0);
    home_order_apply(out, K, 4, o);
    CHECK(same(o, 2, 0, 3, 1));
    // Too long leaves nothing rather than half a list.
    CHECK(!home_order_format(K, want, 4, out, 8));
    CHECK(out[0] == '\0');
    CHECK(home_order_format(K, want, 4, out, 15));
    CHECK(!home_order_format(K, want, 4, out, 14));

    printf("home order: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}

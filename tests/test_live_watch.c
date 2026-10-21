// Host test for source/player/core/live_watch.h: when a live stream has to be
// asked for again.  Times are microseconds on an arbitrary clock.
//
//   make -f Makefile.host test_live_watch && ./test_live_watch

#include "live_watch.h"
#include "live_buffer.h"

#include <stdio.h>
#include <string.h>

static int s_checks = 0, s_failed = 0;
#define CHECK(cond) do { s_checks++; if (!(cond)) { s_failed++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define S(n) ((uint64_t)(n) * 1000000ULL)

static void pausing(void) {
    printf("- pause\n");
    LiveWatch w; live_watch_init(&w, S(100));
    // Playing: nothing.
    CHECK(live_watch_step(&w, S(101), false, 10, false) == LIVE_EV_NONE);
    // Paused for 10 s exactly is still a short pause; past it is stale.
    CHECK(live_watch_step(&w, S(102), true, 10, false) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(112), false, 10, false) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(120), true, 10, false) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(131), true, 10, false) == LIVE_EV_NONE);      // still paused
    CHECK(live_watch_step(&w, S(131) + 1, false, 10, false) == LIVE_EV_RESUME_STALE);
    CHECK(live_watch_step(&w, S(132), false, 10, false) == LIVE_EV_NONE);     // once
}

static void stalls(void) {
    printf("- stall\n");
    LiveWatch w; live_watch_init(&w, S(0));
    // No picture yet is not a stall, however long.
    CHECK(live_watch_step(&w, S(60), false, 0, false) == LIVE_EV_NONE);
    // Pictures arriving: fine.
    CHECK(live_watch_step(&w, S(61), false, 1, false) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(70), false, 200, false) == LIVE_EV_NONE);
    // Frozen count: 15 s, not before.
    CHECK(live_watch_step(&w, S(85), false, 200, false) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(85) + 1, false, 200, false) == LIVE_EV_STALLED);
    // A pause is not a stall: the count stands still while paused.
    live_watch_opened(&w, S(100));
    CHECK(live_watch_step(&w, S(101), false, 5, false) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(102), true, 5, false) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(112), false, 5, false) == LIVE_EV_NONE);      // 10 s pause, not stale
    CHECK(live_watch_step(&w, S(120), false, 5, false) == LIVE_EV_NONE);      // 8 s since resuming
    CHECK(live_watch_step(&w, S(127) + 1, false, 5, false) == LIVE_EV_STALLED);
}

static void ended(void) {
    printf("- ended\n");
    LiveWatch w; live_watch_init(&w, S(0));
    CHECK(live_watch_step(&w, S(1), false, 3, true) == LIVE_EV_ENDED);
    // While paused the end waits for the resume.
    live_watch_opened(&w, S(2));
    CHECK(live_watch_step(&w, S(3), true, 3, true) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(4), false, 3, true) == LIVE_EV_ENDED);
}

static void looping(void) {
    printf("- loop\n");
    LiveWatch w; live_watch_init(&w, S(0));
    // Reopens a minute apart never loop.
    for (int i = 1; i <= 6; i++) {
        CHECK(!live_watch_looping(&w, S(60 * i)));
        live_watch_opened(&w, S(60 * i));
    }
    // Three in a row inside 10 s each do.
    uint64_t t = S(1000);
    live_watch_opened(&w, t);
    t += S(5); CHECK(!live_watch_looping(&w, t)); live_watch_opened(&w, t);
    t += S(5); CHECK(!live_watch_looping(&w, t)); live_watch_opened(&w, t);
    t += S(5); CHECK(live_watch_looping(&w, t));
    // A long run in between resets the count.
    live_watch_opened(&w, t);
    t += S(30); CHECK(!live_watch_looping(&w, t));
    live_watch_opened(&w, t);
    t += S(5);  CHECK(!live_watch_looping(&w, t));
}

// One TS packet carrying PCR base `pcr` (90 kHz) on `pid`.
static void pcr_packet(unsigned char *k, int pid, unsigned long long pcr, int disc) {
    memset(k, 0xff, 188);
    k[0] = 0x47; k[1] = (unsigned char)((pid >> 8) & 0x1f); k[2] = (unsigned char)pid;
    k[3] = 0x20; k[4] = 7; k[5] = (unsigned char)(0x10 | (disc ? 0x80 : 0));
    k[6] = (unsigned char)(pcr >> 25); k[7] = (unsigned char)(pcr >> 17);
    k[8] = (unsigned char)(pcr >> 9);  k[9] = (unsigned char)(pcr >> 1);
    k[10] = (unsigned char)((pcr & 1) << 7);
}

static void pcr(void) {
    live_pcr p;
    unsigned char k[188];
    live_pcr_reset(&p);
    CHECK(live_pcr_ms(&p) == 0);
    // Packets without a PCR change nothing.
    memset(k, 0, sizeof k); k[0] = 0x47; k[3] = 0x10;
    live_pcr_feed(&p, k);
    CHECK(live_pcr_ms(&p) == 0);
    // Four seconds on PID 0x100, starting near the 33-bit top.
    const unsigned long long base = (1ULL << 33) - 90000ULL * 10;
    for (int i = 0; i <= 40; i++) { pcr_packet(k, 0x100, base + 9000ULL * i, 0); live_pcr_feed(&p, k); }
    CHECK(live_pcr_ms(&p) == 4000);
    // Another PID's PCR is ignored.
    pcr_packet(k, 0x101, base + 90000ULL * 60, 0); live_pcr_feed(&p, k);
    CHECK(live_pcr_ms(&p) == 4000);
    // A wrap keeps the four seconds and counts on from the new value.
    pcr_packet(k, 0x100, 0, 0); live_pcr_feed(&p, k);
    pcr_packet(k, 0x100, 90000ULL * 2, 0); live_pcr_feed(&p, k);
    CHECK(live_pcr_ms(&p) == 6000);
    // A jump forward of a minute is a gap, not programme.
    pcr_packet(k, 0x100, 90000ULL * 62, 0); live_pcr_feed(&p, k);
    CHECK(live_pcr_ms(&p) == 6000);
    pcr_packet(k, 0x100, 90000ULL * 63, 0); live_pcr_feed(&p, k);
    CHECK(live_pcr_ms(&p) == 7000);
    // So is a flagged discontinuity, even a small one.
    pcr_packet(k, 0x100, 90000ULL * 65, 1); live_pcr_feed(&p, k);
    pcr_packet(k, 0x100, 90000ULL * 66, 0); live_pcr_feed(&p, k);
    CHECK(live_pcr_ms(&p) == 8000);
    // A short adaptation field or a lost sync byte is not read.
    pcr_packet(k, 0x100, 90000ULL * 90, 0); k[4] = 1; live_pcr_feed(&p, k);
    pcr_packet(k, 0x100, 90000ULL * 90, 0); k[0] = 0; live_pcr_feed(&p, k);
    CHECK(live_pcr_ms(&p) == 8000);
    live_pcr_reset(&p);
    CHECK(live_pcr_ms(&p) == 0);
}

int main(void) {
    CHECK(live_buffer_seconds(0) == 4 && live_buffer_seconds(1) == 8 && live_buffer_seconds(2) == 15);
    CHECK(live_buffer_seconds(-1) == 8 && live_buffer_seconds(3) == 8);
    CHECK(live_buffer_target(4000000u, 100000, 8) == 21277);
    CHECK(live_buffer_target(10000000u, 100000, 8) == 53192);
    CHECK(live_buffer_target(1500000u, 100000, 8) == 7979);
    CHECK(live_buffer_target(4000000u, 100000, 4) == 10639);
    CHECK(live_buffer_target(0, 100000, 8) == live_buffer_target(4000000u, 100000, 8));
    CHECK(live_buffer_target(10000000u, 1000, 8) == 900);
    CHECK(live_buffer_target(4000000u, 0, 8) == 0);
    CHECK(live_buffer_target(4000000u, -1, 8) == 0);
    CHECK(live_buffer_target(4000000u, 1, 8) == 1);
    CHECK(live_buffer_target(4294967295u, 100000, 15) == 90000);
    for (int cap = 1; cap < 100000; cap += 97) {
        const int target = live_buffer_target(4000000u, cap, 15);
        CHECK(target > 0 && target <= cap);
    }
    pcr();
    pausing();
    stalls();
    ended();
    looping();
    printf("live watch: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}

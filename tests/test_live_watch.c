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
    CHECK(live_watch_step(&w, S(101), false, 10, false, 0) == LIVE_EV_NONE);
    // Paused for 10 s exactly is still a short pause; past it is stale.
    CHECK(live_watch_step(&w, S(102), true, 10, false, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(112), false, 10, false, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(120), true, 10, false, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(131), true, 10, false, 0) == LIVE_EV_NONE);      // still paused
    CHECK(live_watch_step(&w, S(131) + 1, false, 10, false, 0) == LIVE_EV_RESUME_STALE);
    CHECK(live_watch_step(&w, S(132), false, 10, false, 0) == LIVE_EV_NONE);     // once
}

static void stalls(void) {
    printf("- stall\n");
    LiveWatch w; live_watch_init(&w, S(0));
    // No picture yet is not a stall, however long.
    CHECK(live_watch_step(&w, S(60), false, 0, false, 0) == LIVE_EV_NONE);
    // Pictures arriving: fine.
    CHECK(live_watch_step(&w, S(61), false, 1, false, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(70), false, 200, false, 0) == LIVE_EV_NONE);
    // Frozen count: 6 s, not before.
    CHECK(live_watch_step(&w, S(76), false, 200, false, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(76) + 1, false, 200, false, 0) == LIVE_EV_STALLED);
    // A pause is not a stall: the count stands still while paused.
    live_watch_opened(&w, S(100));
    CHECK(live_watch_step(&w, S(101), false, 5, false, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(102), true, 5, false, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(112), false, 5, false, 0) == LIVE_EV_NONE);      // 10 s pause, not stale
    CHECK(live_watch_step(&w, S(118), false, 5, false, 0) == LIVE_EV_NONE);      // 6 s since resuming
    CHECK(live_watch_step(&w, S(118) + 1, false, 5, false, 0) == LIVE_EV_STALLED);
}

// The picture stops while the sound plays on (a stitched ad break).
static void sound_on(void) {
    printf("- sound on\n");
    LiveWatch w; live_watch_init(&w, S(0));
    CHECK(live_watch_step(&w, S(1), false, 5, false, S(10)) == LIVE_EV_NONE);
    // The sound exactly 2 s on is not yet past the last picture.
    CHECK(live_watch_step(&w, S(3), false, 5, false, S(12)) == LIVE_EV_NONE);
    // Past it, but the picture has been still for only 2 s.
    CHECK(live_watch_step(&w, S(3), false, 5, false, S(12) + 1) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(4), false, 5, false, S(13)) == LIVE_EV_STALLED);
    // The sound stopped too (the network): the full 6 s.
    live_watch_init(&w, S(0));
    CHECK(live_watch_step(&w, S(1), false, 5, false, S(10)) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(4), false, 5, false, S(10)) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(7), false, 5, false, S(10)) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(7) + 1, false, 5, false, S(10)) == LIVE_EV_STALLED);
    // A clock that appears only after the last picture is no evidence of sound.
    live_watch_init(&w, S(0));
    CHECK(live_watch_step(&w, S(1), false, 5, false, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(2), false, 5, false, S(30)) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(5), false, 5, false, S(30)) == LIVE_EV_NONE);
    // A clock that went backwards (a new stream) starts again from there.
    live_watch_init(&w, S(0));
    CHECK(live_watch_step(&w, S(1), false, 5, false, S(30)) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(2), false, 5, false, S(1)) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(5), false, 5, false, S(2)) == LIVE_EV_NONE);
}

// A dead channel: two stall reopens a minute, 20 s apart, then it waits.
static void reconnect_limit(void) {
    printf("- reconnect limit\n");
    LiveWatch w; live_watch_init(&w, S(0));
    CHECK(live_watch_step(&w, S(1), false, 5, false, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(7) + 1, false, 5, false, 0) == LIVE_EV_STALLED);
    live_watch_opened(&w, S(8));
    CHECK(live_watch_step(&w, S(9), false, 5, false, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(15) + 1, false, 5, false, 0) == LIVE_EV_NONE);  // 8 s since the last
    CHECK(live_watch_step(&w, S(27), false, 5, false, 0) == LIVE_EV_NONE);      // not yet 20 s
    CHECK(live_watch_step(&w, S(28), false, 5, false, 0) == LIVE_EV_STALLED);
    live_watch_opened(&w, S(29));
    CHECK(live_watch_step(&w, S(30), false, 5, false, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(50), false, 5, false, 0) == LIVE_EV_NONE);      // two this minute
    CHECK(live_watch_step(&w, S(67) + 2, false, 5, false, 0) == LIVE_EV_STALLED); // a new minute
    // Another channel starts the count again.
    live_watch_opened(&w, S(68));
    live_watch_channel_changed(&w);
    CHECK(live_watch_step(&w, S(69), false, 5, false, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(75) + 1, false, 5, false, 0) == LIVE_EV_STALLED);
}

static void ended(void) {
    printf("- ended\n");
    LiveWatch w; live_watch_init(&w, S(0));
    CHECK(live_watch_step(&w, S(1), false, 3, true, 0) == LIVE_EV_ENDED);
    // While paused the end waits for the resume.
    live_watch_opened(&w, S(2));
    CHECK(live_watch_step(&w, S(3), true, 3, true, 0) == LIVE_EV_NONE);
    CHECK(live_watch_step(&w, S(4), false, 3, true, 0) == LIVE_EV_ENDED);
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
    sound_on();
    reconnect_limit();
    ended();
    looping();
    printf("live watch: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}

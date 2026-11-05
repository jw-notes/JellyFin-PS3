// Host test for source/player/core/scrub_motion.h: how a held scrub moves.
//
//   make -f Makefile.host test_scrub_motion && ./test_scrub_motion

#include "scrub_motion.h"

#include <stdio.h>

static int s_checks = 0, s_failed = 0;
#define CHECK(cond) do { s_checks++; if (!(cond)) { s_failed++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define EP 1340u      // a 22 minute episode
#define FILM 7200u    // a 2 hour film

static void speed(void) {
    printf("- speed\n");
    // Starts where a spot can be picked.
    CHECK(scrub_speed(0.0f, EP) == SCRUB_START_SECS_PER_SEC);
    CHECK(scrub_speed(-1.0f, EP) == SCRUB_START_SECS_PER_SEC);
    // Only ever climbs, by small amounts: no step anyone could see.
    float last = 0.0f;
    for (float t = 0.0f; t < 30.0f; t += 1.0f / 60.0f) {
        const float v = scrub_speed(t, FILM);
        CHECK(v >= last);
        if (last > 0.0f) CHECK(v - last <= last * 0.03f);
        last = v;
    }
    // And stops at the top.
    CHECK(scrub_speed(60.0f, FILM) == scrub_top_speed(FILM));
    CHECK(scrub_speed(60.0f, EP) == scrub_top_speed(EP));
    // The top: a title's length over 12 s, within limits.
    CHECK(scrub_top_speed(FILM) == 600.0f);
    CHECK(scrub_top_speed(EP) == SCRUB_TOP_MIN);
    CHECK(scrub_top_speed(0) == SCRUB_TOP_MIN);
    CHECK(scrub_top_speed(100000) == SCRUB_TOP_MAX);
}

// The same hold at any frame rate covers about the same ground.
static double held_for(float secs, float fps) {
    double pos = 0.0;
    const float dt = 1.0f / fps;
    for (float t = 0.0f; t < secs; t += dt) pos += (double)scrub_speed(t, FILM) * dt;
    return pos;
}

static void frame_rate(void) {
    printf("- frame rate\n");
    const double a = held_for(4.0f, 60.0f), b = held_for(4.0f, 24.0f);
    CHECK(a > 100.0 && a < 400.0);
    CHECK((a > b ? a - b : b - a) < a * 0.06);
}

static void clamp(void) {
    printf("- clamp\n");
    // Inside: unchanged.
    CHECK(scrub_clamp_offset(10000, 60000, EP) == 10000);
    CHECK(scrub_clamp_offset(-10000, 60000, EP) == -10000);
    // The start.
    CHECK(scrub_clamp_offset(-90000, 60000, EP) == -60000);
    // Short of the end.
    CHECK(scrub_clamp_offset(99999999, 60000, EP) == (int64_t)EP * 1000 - SCRUB_END_MARGIN_MS - 60000);
    // Already inside the margin: it stays put rather than going backwards.
    CHECK(scrub_clamp_offset(5000, (int64_t)EP * 1000 - 1000, EP) == 0);
    // Back from there still works.
    CHECK(scrub_clamp_offset(-5000, (int64_t)EP * 1000 - 1000, EP) == -5000);
    // No known length: no end to stop at.
    CHECK(scrub_clamp_offset(99999999, 60000, 0) == 99999999);
}

static void glide(void) {
    printf("- glide\n");
    // Forward and back: moves toward the target, never past it, and lands exactly.
    const int64_t targets[2] = { 10000, -10000 };
    for (int k = 0; k < 2; k++) {
        const int64_t target = targets[k];
        int64_t s = 0;
        int frames = 0;
        while (s != target && frames < 200) {
            const int64_t next = scrub_glide(s, target, 16683);
            const int64_t before = target > s ? target - s : s - target;
            const int64_t after = target > next ? target - next : next - target;
            CHECK(after < before);
            CHECK((target > 0) ? next <= target : next >= target);
            s = next;
            frames++;
        }
        CHECK(s == target);
        CHECK(frames > 3 && frames < 60);     // a glide, not a jump or a crawl
    }
    // On target it stays.
    CHECK(scrub_glide(500, 500, 16683) == 500);
    // A long gap between frames still does not overshoot.
    CHECK(scrub_glide(0, 1000, 10000000) <= 1000);
}

int main(void) {
    speed();
    frame_rate();
    clamp();
    glide();
    printf("scrub motion: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}

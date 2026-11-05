#pragma once
#include <stdint.h>

// -------------------------------------------------------------------------
//  How a held scrub moves
// -------------------------------------------------------------------------
//  Pure, so tests/test_scrub_motion.c compiles what the console runs.  The
//  position moves by elapsed time, not in steps, so it does not depend on the
//  frame rate.  It starts slowly enough to pick a spot and doubles as the hold
//  goes on, up to a top speed that crosses a title in about twelve seconds.

#define SCRUB_START_SECS_PER_SEC  20.0f
#define SCRUB_DOUBLE_EVERY_SECS   1.2f
#define SCRUB_TOP_MIN             120.0f
#define SCRUB_TOP_MAX             900.0f
#define SCRUB_CROSS_SECS          12.0f
// Stops this far short of the end: a scrub is not a request for the next episode.
#define SCRUB_END_MARGIN_MS       5000

static inline float scrub_top_speed(uint32_t total_secs) {
    const float top = (float)total_secs / SCRUB_CROSS_SECS;
    return top < SCRUB_TOP_MIN ? SCRUB_TOP_MIN : top > SCRUB_TOP_MAX ? SCRUB_TOP_MAX : top;
}

// Seconds of media per second of holding, `held_secs` into the hold.
static inline float scrub_speed(float held_secs, uint32_t total_secs) {
    const float top = scrub_top_speed(total_secs);
    float v = SCRUB_START_SECS_PER_SEC;
    for (float t = SCRUB_DOUBLE_EVERY_SECS; t <= held_secs && v < top; t += SCRUB_DOUBLE_EVERY_SECS)
        v *= 2.0f;
    // Between doublings it climbs in a line, so there is no visible step.
    const float into = held_secs > 0.0f ? held_secs : 0.0f;
    const int whole = (int)(into / SCRUB_DOUBLE_EVERY_SECS);
    v *= 1.0f + (into - (float)whole * SCRUB_DOUBLE_EVERY_SECS) / SCRUB_DOUBLE_EVERY_SECS;
    return v > top ? top : v;
}

// An offset from pos_ms kept between the start of the title and short of its end.
// total_secs 0 (unknown) has no end to stop at.
static inline int64_t scrub_clamp_offset(int64_t offset_ms, int64_t pos_ms, uint32_t total_secs) {
    if (offset_ms < -pos_ms) offset_ms = -pos_ms;
    if (total_secs > 0) {
        int64_t most = (int64_t)total_secs * 1000 - SCRUB_END_MARGIN_MS - pos_ms;
        if (most < 0) most = 0;
        if (offset_ms > most) offset_ms = most;
    }
    return offset_ms;
}

// Where the play head is drawn: it closes on the target over about 60 ms, so a
// 10 second tap glides instead of jumping, and lands exactly.
static inline int64_t scrub_glide(int64_t shown_ms, int64_t target_ms, uint64_t dt_us) {
    const int64_t gap = target_ms - shown_ms;
    const int64_t step = (int64_t)((double)gap * (double)dt_us / ((double)dt_us + 60000.0));
    const int64_t next = shown_ms + step;
    const int64_t left = target_ms - next;
    return (left > -20 && left < 20) ? target_ms : next;
}

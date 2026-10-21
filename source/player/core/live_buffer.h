#pragma once
#include <stdint.h>

// Live reserve lengths offered in Settings > Playback > Live TV Buffer.
#define LIVE_BUFFER_STEPS 3
#define LIVE_BUFFER_DEFAULT_STEP 1
static inline unsigned live_buffer_seconds(int step) {
    static const unsigned k_seconds[LIVE_BUFFER_STEPS] = { 4u, 8u, 15u };
    if (step < 0 || step >= LIVE_BUFFER_STEPS) step = LIVE_BUFFER_DEFAULT_STEP;
    return k_seconds[step];
}

// How long the wait may run past the reserve itself before playing anyway.
#define LIVE_BUFFER_GRACE_US 12000000ULL

// The packet ceiling for a reserve: the configured rate (audio and transport
// included) for the reserve's length, bounded by the existing ring allocation
// (no extra RAM). The wait normally ends earlier, on the stream's own clock.
static inline int live_buffer_target(unsigned bits_per_second, int capacity, unsigned seconds) {
    if (capacity <= 0) return 0;
    if (!bits_per_second) bits_per_second = 4000000u;
    uint64_t packets = ((uint64_t)bits_per_second * seconds + 1503u) / 1504u;
    const int maximum = capacity - capacity / 10;
    if (packets > (uint64_t)maximum) packets = (uint64_t)maximum;
    return packets > 0 ? (int)packets : 1;
}

// Seconds of programme received, from the transport stream's PCR. Bytes alone
// cannot say: a channel well under the quality ceiling (a 2 Mbps channel with
// a 4 Mbps ceiling) took twice as long to fill a byte target. A server that
// reads its source at native frame rate sends exactly one second per second,
// so that wait was all dead time.
typedef struct {
    int      pid;       // the PCR PID, -1 until the first PCR
    uint64_t first;     // 90 kHz PCR base at the start of the current run
    uint64_t last;
    uint32_t done_ms;   // runs closed by a discontinuity
} live_pcr;

static inline void live_pcr_reset(live_pcr *p) {
    p->pid = -1;
    p->first = p->last = 0;
    p->done_ms = 0;
}

static inline void live_pcr_feed(live_pcr *p, const uint8_t *k) {
    // Sync byte, adaptation field present, long enough for a PCR, PCR flag.
    if (k[0] != 0x47 || !(k[3] & 0x20) || k[4] < 7 || !(k[5] & 0x10)) return;
    const int pid = ((k[1] & 0x1f) << 8) | k[2];
    const uint64_t pcr = ((uint64_t)k[6] << 25) | ((uint64_t)k[7] << 17) |
                         ((uint64_t)k[8] << 9) | ((uint64_t)k[9] << 1) | (k[10] >> 7);
    if (p->pid < 0) {
        p->pid = pid;
        p->first = p->last = pcr;
        return;
    }
    if (pid != p->pid) return;
    // A flagged discontinuity, a wrap or a jump of more than ten seconds
    // closes the run instead of counting the gap as programme.
    if ((k[5] & 0x80) || pcr < p->last || pcr - p->last > 900000u) {
        p->done_ms += (uint32_t)((p->last - p->first) / 90u);
        p->first = pcr;
    }
    p->last = pcr;
}

static inline uint32_t live_pcr_ms(const live_pcr *p) {
    return p->pid < 0 ? 0 : p->done_ms + (uint32_t)((p->last - p->first) / 90u);
}

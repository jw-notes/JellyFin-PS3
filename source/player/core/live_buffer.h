#pragma once
#include <stdint.h>

#define LIVE_BUFFER_SECONDS 8u
#define LIVE_BUFFER_MAX_US 20000000ULL

// The rate includes audio and transport overhead. Reserve the same duration
// at each quality, bounded by the existing ring allocation (no extra RAM).
static inline int live_buffer_target(unsigned bits_per_second, int capacity) {
    if (capacity <= 0) return 0;
    if (!bits_per_second) bits_per_second = 4000000u;
    uint64_t packets = ((uint64_t)bits_per_second * LIVE_BUFFER_SECONDS + 1503u) / 1504u;
    const int maximum = capacity - capacity / 10;
    if (packets > (uint64_t)maximum) packets = (uint64_t)maximum;
    return packets > 0 ? (int)packets : 1;
}

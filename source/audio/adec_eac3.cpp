// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
//
// E-AC-3 decode path (vendored FFmpeg decoder) -- see adec_eac3.h for the module contract.

#include "adec_eac3.h"
#include "adec.h"
#include "adec_out.h"
#include "chan_map.h"
#include "eac3_api.h"
#include "plog.h"

#include <stdio.h>
#include <string.h>

// An E-AC-3 frame is at most 4096 bytes; an access unit with its dependent frames (7.1) about twice
// that.  The carry holds two units and the padding the decoder reads past the end.
#define EAC3_CARRY_BYTES (4 * 4096)

static eac3_dec *s_dec = NULL;
static u8   s_carry[EAC3_CARRY_BYTES + EAC3_API_PADDING];
static int  s_carry_len = 0;
static bool s_logged = false;
static u32  s_bad = 0;
static float s_inter[EAC3_API_MAX_SAMPLES * EAC3_API_MAX_CHANNELS];

bool adec_eac3_open(int out_channels) {
    adec_eac3_close();
    s_dec = eac3_api_open();
    if (!s_dec) {
        plog("adec_eac3: decoder open failed (out of memory)");
        return false;
    }
    adec_out_open(out_channels);
    s_carry_len = 0;
    s_logged = false;
    s_bad = 0;
    char b[48];
    snprintf(b, sizeof(b), "adec_eac3: open out_ch=%d", out_channels == 6 ? 6 : 2);
    plog(b);
    return true;
}

void adec_eac3_close(void) {
    if (s_dec) { eac3_api_close(s_dec); s_dec = NULL; }
    adec_out_close();
    s_carry_len = 0;
}

void adec_eac3_reset(void) {
    s_carry_len = 0;
    if (s_dec) eac3_api_flush(s_dec);
    adec_out_reset();
}

// FFmpeg's channel-mask bit -> the position code chan_map.h places it by.
static unsigned char position_of_bit(int bit) {
    switch (bit) {
    case 0:  return CH_POS_FRONT_LEFT;
    case 1:  return CH_POS_FRONT_RIGHT;
    case 2:  return CH_POS_FRONT_CENTER;
    case 3:  return CH_POS_LFE;
    case 4:  return CH_POS_BACK_LEFT;
    case 5:  return CH_POS_BACK_RIGHT;
    case 8:  return CH_POS_BACK_CENTER;
    case 9:  return CH_POS_SIDE_LEFT;
    case 10: return CH_POS_SIDE_RIGHT;
    default: return CH_POS_UNKNOWN;     // front-of-centre, top and wide channels: left out
    }
}

// One decoded unit: interleaved, placed, on to the output stage.
static void deliver(int samples) {
    const int ch = eac3_api_channels(s_dec);
    if (samples <= 0 || ch <= 0 || ch > EAC3_API_MAX_CHANNELS) return;
    if (samples > EAC3_API_MAX_SAMPLES) samples = EAC3_API_MAX_SAMPLES;
    // the planes are in ascending mask-bit order; mono has no mask bits to go by beyond FC
    unsigned char pos[EAC3_API_MAX_CHANNELS];
    const unsigned long long mask = eac3_api_channel_mask(s_dec);
    int k = 0;
    for (int bit = 0; bit < 64 && k < ch; bit++)
        if (mask & (1ULL << bit)) pos[k++] = position_of_bit(bit);
    while (k < ch) pos[k++] = CH_POS_UNKNOWN;
    for (int c = 0; c < ch; c++) {
        const float *p = eac3_api_plane(s_dec, c);
        if (!p) return;
        for (int i = 0; i < samples; i++) s_inter[(size_t)i * (size_t)ch + (size_t)c] = p[i];
    }
    const int rate = eac3_api_sample_rate(s_dec);
    if (!s_logged) {
        s_logged = true;
        char b[96];
        snprintf(b, sizeof(b), "adec_eac3: hz=%d ch=%d mask=0x%llx", rate, ch, mask);
        plog(b);
    }
    adec_out_push(s_inter, samples, ch, mask ? pos : NULL, rate);
}

static void decode_carry(void) {
    int off = 0;
    while (s_carry_len - off >= 16) {
        int dep = 0;
        const int fs = eac3_api_frame_size(s_carry + off, s_carry_len - off, &dep);
        if (fs <= 0) { off++; continue; }                        // not a header: rescan
        if (dep) { off += fs <= s_carry_len - off ? fs : 1; continue; }   // a dependent frame with no head: skip
        // the dependent frames after it belong to this unit; wait until the next header shows
        int unit = fs;
        bool complete = false;
        for (;;) {
            if (off + unit > s_carry_len) break;                 // the frame itself is not all here
            if (s_carry_len - off - unit < 16) break;            // cannot see what follows yet
            int dep2 = 0;
            const int fs2 = eac3_api_frame_size(s_carry + off + unit, s_carry_len - off - unit, &dep2);
            if (fs2 <= 0 || !dep2) { complete = true; break; }   // the next unit starts (or junk): done
            if (off + unit + fs2 > s_carry_len) break;           // its dependent frame is not all here
            unit += fs2;
        }
        if (!complete) break;                                    // wait for more of the stream
        // the decoder reads a little past the unit: what follows is the next frame, or the zero padding
        memset(s_carry + s_carry_len, 0, EAC3_API_PADDING);
        int samples = 0;
        const int used = eac3_api_decode(s_dec, s_carry + off, unit, &samples);
        if (used < 0) {
            if ((s_bad++ % 128) == 0) {
                char b[64];
                snprintf(b, sizeof(b), "adec_eac3: bad frame, total=%u", (unsigned)s_bad);
                plog(b);
            }
        } else {
            deliver(samples);
        }
        off += unit;
    }
    if (off > 0) {
        memmove(s_carry, s_carry + off, (size_t)(s_carry_len - off));
        s_carry_len -= off;
    }
}

void adec_eac3_decode_payload(const u8 *es, int len) {
    if (!s_dec || len <= 0) return;
    while (len > 0) {
        int space = EAC3_CARRY_BYTES - s_carry_len;
        if (space == 0) {
            plog("adec_eac3: carry overflow, resync");
            s_carry_len = 0;
            space = EAC3_CARRY_BYTES;
        }
        const int take = len < space ? len : space;
        memcpy(s_carry + s_carry_len, es, (size_t)take);
        s_carry_len += take;
        es += take;
        len -= take;
        decode_carry();
    }
}

// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
#pragma once

// Entry points onto the vendored FFmpeg AC-3 / E-AC-3 decoder (source/audio/eac3/, see
// PROVENANCE.md there).  Pure C, no PS3 headers: tests/test_eac3_decode.c compiles the same files.
//
// The decoder is compiled as one translation unit by eac3_api.c, which #includes the vendored .c
// files (their entry points are static, normally reached through an FFCodec table this app does not
// build); libavutil's MDCT and float DSP are compiled by eac3_tx.c, eac3_txf.c and eac3_util.c.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct eac3_dec eac3_dec;

// The most samples a channel one decode produces (six blocks of 256), and the most channels.
#define EAC3_API_MAX_SAMPLES  1536
#define EAC3_API_MAX_CHANNELS 8
// Zero bytes the decoder may read past the end of a frame; a caller's buffer must have them.
#define EAC3_API_PADDING      64

// NULL when out of memory.
eac3_dec *eac3_api_open(void);
void      eac3_api_close(eac3_dec *d);
// A seek: the decoder's overlap and state go.
void      eac3_api_flush(eac3_dec *d);

// The syncframe at the start of `buf`: its size in bytes, and whether it is an E-AC-3 dependent frame
// (one that extends the independent frame before it, as 7.1 does).  0 when `buf` does not start with a
// readable header (or holds fewer bytes than a header needs).
int  eac3_api_frame_size(const uint8_t *buf, int len, int *dependent);

// Decodes one access unit: an AC-3 or E-AC-3 independent frame and the dependent frames after it.
// `data` must be followed by EAC3_API_PADDING readable bytes.  Returns the bytes used (> 0) with
// *samples a channel decoded (0: none this time), or < 0 on an error.  The output stays valid until the
// next call: eac3_api_plane() for each of the channels.
int  eac3_api_decode(eac3_dec *d, const uint8_t *data, int size, int *samples);
int          eac3_api_channels(const eac3_dec *d);
int          eac3_api_sample_rate(const eac3_dec *d);
// FFmpeg's channel mask (bit 0 FL, 1 FR, 2 FC, 3 LFE, 4 BL, 5 BR, 9 SL, 10 SR, ...), the planes in
// ascending bit order.
uint64_t     eac3_api_channel_mask(const eac3_dec *d);
const float *eac3_api_plane(const eac3_dec *d, int ch);

#ifdef __cplusplus
}
#endif

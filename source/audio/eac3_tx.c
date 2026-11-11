// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
//
// libavutil's transform framework (MDCT / FFT), vendored unmodified in mlp/ff/libavutil/ (see
// eac3/PROVENANCE.md).  Its own translation unit, as in FFmpeg: tx.c, tx_float.c and the decoders
// share no static names this way.  The AC-3 / E-AC-3 decoder's IMDCTs run through it.
#include "mlp/ff/libavutil/internal.h"   // NULL_IF_CONFIG_SMALL, as libavutil's own build has it
#include "mlp/ff/libavutil/tx.c"

// The CPU-feature probe tx.c asks before choosing SIMD codelets: none here (config.h builds no
// assembly), so only the portable C transforms are ever chosen.
int av_get_cpu_flags(void) { return 0; }

// tx.c lists the double and 32-bit-integer transforms beside the float ones.  Nothing here asks for
// them (the AC-3 decoder's IMDCTs are float), so their lists are empty rather than ~150 KB of code.
const FFTXCodelet * const ff_tx_codelet_list_double_c[] = { NULL };
const FFTXCodelet * const ff_tx_codelet_list_int32_c[]  = { NULL };

// libavutil/mathematics.c's and mem.c's, which tx.c uses.
int64_t av_gcd(int64_t a, int64_t b)
{
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { const int64_t t = a % b; a = b; b = t; }
    return a;
}

void *av_fast_realloc(void *ptr, unsigned int *size, size_t min_size)
{
    if (min_size <= *size) return ptr;
    if (min_size > 0x7FFFFFFFu) { *size = 0; return NULL; }
    size_t grown = min_size + min_size / 16 + 32;
    if (grown > 0x7FFFFFFFu) grown = min_size;
    void *r = av_realloc(ptr, grown);
    if (!r) { *size = 0; return NULL; }   // as upstream: the old block is the caller's to free
    *size = (unsigned int)grown;
    return r;
}

// DCA-only additions to the shared FFmpeg compat layer (mlp_compat.c already
// supplies av_malloc/av_log/av_channel_layout_*/ff_get_buffer and friends).
#include <stdlib.h>
#include <string.h>
#include "mlp/ff/libavcodec/avcodec.h"
#include "mlp/ff/libavutil/mem.h"

// These two tables are REAL upstream data, not placeholders.  They used to be
// zero-filled stand-ins here, on the assumption that nothing reachable read
// them.  Both assumptions were wrong, and both wrecked decoding silently:
//
//   ff_log2_tab  backs av_log2_c(), which adds ff_log2_tab[v] after shifting
//                the value down to 8 bits.  Zero-filled, av_log2 returned only
//                the multiple-of-8 part: av_log2(0x60f) gave 8 instead of 10
//                and av_log2(0xff) gave 0 instead of 7.  dcadec.c derives
//                max_spkr from av_log2(ch_mask) and dcaadpcm.c derives
//                shift_bits from av_log2(max), so the speaker loop stopped
//                short and the XLL predictor was scaled wrong.
//
//   ff_inverse   backs FASTDIV(a,b), which dca_core.c:538/543 uses to
//                dequantize block codes.  Zero-filled, FASTDIV returned 0 for
//                every input, so the core substream decoded to rubbish -- and
//                because this disc's DTS-HD MA is RESIDUAL-encoded, the
//                lossless output is core+residual, which made the lossless
//                path rubbish too.  It was also declared uint64_t here while
//                upstream declares it uint32_t, so FASTDIV's indexing was
//                wrong even had the values been right.
//
// Vendoring the upstream files verbatim (rather than re-stubbing them) keeps
// this honest and kills the whole class of bug.  See dcahd/PROVENANCE.md.
// log2_tab.c is compiled in eac3_util.c now: the AC-3 / E-AC-3 decoder needs
// it too, and this decoder links that copy.
#include "mlp/ff/libavcodec/mathtables.c"

int av_channel_layout_custom_init(AVChannelLayout *ch, int nb) { (void)ch; (void)nb; return -1; }

// Faithful copies of libavutil/mem.c's ff_fast_malloc().  The earlier
// hand-written pair got two things wrong, and the second one silently broke
// every frame after the first:
//
//   * av_fast_mallocz() used to memset an already-large-enough buffer on
//     EVERY call.  Upstream returns early and does NOT re-zero; it zeroes
//     only when it actually (re)allocates.  dca_core.c:784 keeps the core's
//     subband samples in such a buffer, and the ADPCM predictor carries that
//     history ACROSS frames -- so wiping it per frame destroyed inter-frame
//     prediction.  Frame 1 decoded bit-exactly (nothing to carry yet) and
//     every frame after it drifted, which is precisely what the fixture
//     showed before this was fixed.
//
//   * both used to allocate exactly min_size.  Upstream rounds up to
//     min_size + min_size/16 + 32, and some callers rely on that slack --
//     dca_xll.c's chs_alloc_msb_band_data() lays out ndecisamples of history
//     BEFORE the region it sized, which only fits because of the padding.
//
// Keeping the growth rule identical also keeps the realloc pattern identical,
// so buffer addresses and reuse behave as upstream expects.
static int jf_fast_malloc(void *ptr, unsigned int *size, size_t min_size,
                          int zero_realloc)
{
    void **p = (void **)ptr;
    if (min_size <= (size_t)*size)
        return 0;                    /* big enough already — leave contents */
    min_size = min_size + min_size / 16 + 32;
    av_free(*p);
    *p = zero_realloc ? av_mallocz(min_size) : av_malloc(min_size);
    *size = *p ? (unsigned int)min_size : 0;
    return 1;
}

void av_fast_malloc(void *ptr, unsigned int *size, size_t min_size)
{
    jf_fast_malloc(ptr, size, min_size, 0);
}
void av_fast_mallocz(void *ptr, unsigned int *size, size_t min_size)
{
    jf_fast_malloc(ptr, size, min_size, 1);
}

// --- subsystems this decoder shares ----------------------------------------
// av_tx (FFmpeg's MDCT/FFT framework) and the float DSP are reached only from
// the DTS core's FLOAT output path, which AV_CODEC_FLAG_BITEXACT keeps it off
// (see dcahd_api.c): DTS-HD MA decodes on the fixed-point path.  They used to
// be stubbed here; the AC-3 / E-AC-3 decoder needs the real ones, so they are
// now compiled for it (eac3_tx.c, eac3_txf.c, eac3_util.c) and this decoder
// links against those too.  synth_filter.c IS compiled, because a CORE-ONLY
// DTS stream really does run ff_dca_core_filter_fixed and needs its fixed QMF
// -- only an XLL stream skips the core filter entirely.  DTS Express (LBR) is
// lossy and never carries the lossless data.

void ff_dca_lbr_init_tables(void) { }
// (av_log2 is eac3_util.c's now: both decoders use it.)

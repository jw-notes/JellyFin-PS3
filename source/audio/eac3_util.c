// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
//
// The rest of libavutil the AC-3 / E-AC-3 decoder uses, vendored unmodified in mlp/ff/libavutil/
// (see eac3/PROVENANCE.md): the float DSP, and the lagged Fibonacci generator (with the MD5 its
// seeding uses) that dithers zero-bit mantissas.  The DTS-HD decoder links against the float DSP
// here too.
#include "mlp/ff/libavutil/float_dsp.c"
#include "mlp/ff/libavutil/float_scalarproduct.c"
#include "mlp/ff/libavutil/lfg.c"
#include "mlp/ff/libavutil/md5.c"
// ff_log2_tab, behind av_log2_c(): real upstream data (see dcahd_compat.c for what a stand-in cost).
// Both FFmpeg decoders here use it.
#include "mlp/ff/libavutil/log2_tab.c"

#include <math.h>

// libavutil/intmath.c's: both FFmpeg decoders here use it.  (It used to live in dcahd_compat.c,
// when only the DTS-HD decoder needed it.)
int av_log2(unsigned v) { int n = 0; while (v >>= 1) n++; return n; }

// libavutil/mathematics.c's: the modified Bessel function of the first kind, order 0, which the
// Kaiser-Bessel-derived window (kbdwin.c) is built from.  The power series, summed until a term no
// longer changes the double; called a few hundred times, once, when the decoder opens.
double av_bessel_i0(double x)
{
    const double q = x * x / 4.0;
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 500; k++) {
        term *= q / ((double)k * (double)k);
        const double next = sum + term;
        if (next == sum) break;
        sum = next;
    }
    return sum;
}

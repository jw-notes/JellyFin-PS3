// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
//
// Host test for source/audio/eac3_api.c: the vendored FFmpeg AC-3 / E-AC-3 decoder, frame by frame
// as the player feeds it, against ffmpeg's own decode of the same file (float, interleaved).
//
//   make -f Makefile.host tones51.eac3 tones51.eac3.ref test_eac3_decode
//   ./test_eac3_decode tones51.eac3 tones51.eac3.ref

#include "eac3_api.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_checks = 0, s_failed = 0;
#define CHECK(cond) do { s_checks++; if (!(cond)) { s_failed++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static unsigned char *load(const char *path, long *n) {
    FILE *f = fopen(path, "rb");
    if (!f) { printf("missing %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *p = (unsigned char *)malloc((size_t)*n + EAC3_API_PADDING);
    if (p && fread(p, 1, (size_t)*n, f) != (size_t)*n) { free(p); p = NULL; }
    if (p) memset(p + *n, 0, EAC3_API_PADDING);
    fclose(f);
    return p;
}

// Decodes the whole stream as the player does: one access unit (an independent frame and the dependent
// frames after it) at a time.  Returns the samples a channel, interleaved into `out` (cap floats).
static long decode_all(eac3_dec *d, const unsigned char *es, long n, float *out, long cap, int *ch_out, int *rate_out,
                       unsigned long long *mask_out, int *bad) {
    long off = 0, frames = 0;
    *bad = 0;
    while (off < n) {
        int dep = 0;
        int fs = eac3_api_frame_size(es + off, (int)(n - off), &dep);
        if (fs <= 0) { off++; continue; }
        long unit = fs;
        for (;;) {                              // dependent frames ride with their independent frame
            int dep2 = 0;
            const int fs2 = off + unit < n ? eac3_api_frame_size(es + off + unit, (int)(n - off - unit), &dep2) : 0;
            if (fs2 <= 0 || !dep2) break;
            unit += fs2;
        }
        if (off + unit > n) break;
        int samples = 0;
        const int used = eac3_api_decode(d, es + off, (int)unit, &samples);
        if (used < 0) { (*bad)++; off += unit; continue; }
        const int ch = eac3_api_channels(d);
        *ch_out = ch; *rate_out = eac3_api_sample_rate(d); *mask_out = eac3_api_channel_mask(d);
        for (int i = 0; i < samples; i++)
            for (int c = 0; c < ch; c++) {
                const long at = (frames + i) * ch + c;
                if (at < cap) out[at] = eac3_api_plane(d, c)[i];
            }
        frames += samples;
        off += unit;
    }
    return frames;
}

int main(int argc, char **argv) {
    if (argc < 3) { printf("usage: %s file.eac3 file.ref [file.ac3 file.ac3.ref]\n", argv[0]); return 2; }
    for (int pair = 1; pair + 1 < argc; pair += 2) {
        long n = 0, rn = 0;
        unsigned char *es = load(argv[pair], &n);
        unsigned char *rb = load(argv[pair + 1], &rn);
        CHECK(es && rb);
        if (!es || !rb) return 1;
        const float *ref = (const float *)rb;
        const long ref_n = rn / (long)sizeof(float);
        printf("-- %s (%ld bytes), reference %ld floats\n", argv[pair], n, ref_n);

        eac3_dec *d = eac3_api_open();
        CHECK(d != NULL);
        const long cap = ref_n + 8 * 1536;
        float *out = (float *)calloc((size_t)cap, sizeof(float));
        int ch = 0, rate = 0, bad = 0;
        unsigned long long mask = 0;
        const long frames = decode_all(d, es, n, out, cap, &ch, &rate, &mask, &bad);
        printf("   %ld samples x %d channels at %d Hz, mask 0x%llx, %d bad\n", frames, ch, rate, mask, bad);
        CHECK(bad == 0);
        CHECK(rate == 48000);
        CHECK(ch >= 2 && ch <= EAC3_API_MAX_CHANNELS);
        CHECK(frames * ch == ref_n);                   // every sample ffmpeg produced, no more
        // the same numbers ffmpeg computes (the same code; only float evaluation order can differ)
        double worst = 0;
        long over = 0;
        const long cmp = frames * ch < ref_n ? frames * ch : ref_n;
        for (long i = 0; i < cmp; i++) {
            const double e = fabs((double)out[i] - (double)ref[i]);
            if (e > worst) worst = e;
            if (e > 1e-4) over++;
        }
        printf("   worst difference from ffmpeg %.3g, %ld samples over 1e-4\n", worst, over);
        CHECK(worst < 1e-4);
        // and it is sound, not silence
        double energy = 0;
        for (long i = 0; i < cmp; i++) energy += (double)out[i] * out[i];
        CHECK(energy > 1.0);

        // a seek: flushed, then the same stream again from its middle decodes cleanly
        eac3_api_flush(d);
        int dep = 0, half = (int)(n / 2), fs = 0;
        while (half < n && (fs = eac3_api_frame_size(es + half, (int)(n - half), &dep)) <= 0) half++;
        CHECK(half < n);
        int bad2 = 0, ch2 = 0, rate2 = 0; unsigned long long m2 = 0;
        const long f2 = decode_all(d, es + half, n - half, out, cap, &ch2, &rate2, &m2, &bad2);
        CHECK(bad2 == 0 && f2 > 0 && ch2 == ch);

        // hostile input: damaged and truncated frames are refused or concealed, never a crash
        unsigned char *junk = (unsigned char *)malloc((size_t)n + EAC3_API_PADDING);
        memcpy(junk, es, (size_t)n + EAC3_API_PADDING);
        unsigned int seed = 12345;
        for (long i = 0; i < n; i += 97) { seed = seed * 1103515245u + 12345u; junk[i] ^= (unsigned char)(seed >> 16); }
        eac3_api_flush(d);
        int bad3 = 0, ch3 = 0, rate3 = 0; unsigned long long m3 = 0;
        fflush(stderr);
        FILE *quiet = freopen("/dev/null", "w", stderr);    // the decoder reports every damaged block
        decode_all(d, junk, n, out, cap, &ch3, &rate3, &m3, &bad3);
        for (int cut = 1; cut < 200; cut += 17) {
            int s = 0;
            eac3_api_decode(d, es, cut, &s);           // a frame cut short
        }
        (void)quiet;
        CHECK(eac3_api_frame_size(es, 3, &dep) == 0);   // too short to read a header
        unsigned char nosync[32] = { 0 };
        CHECK(eac3_api_frame_size(nosync, sizeof nosync, &dep) == 0);
        free(junk);
        free(out);
        eac3_api_close(d);
        free(es); free(rb);
    }
    printf("eac3_decode: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}

// Player stats — live playback instrumentation.  Public surface: player_stats.h.
//
// The whole file is compiled out when ENABLE_PLAYER_STATS is 0; the header
// turns every hook into an inline no-op, so no call site needs an #if.

#include "player_stats.h"

#if ENABLE_PLAYER_STATS

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include <ppu-types.h>
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>

#include "adec.h"        // PCM_RING_HIGHWATER, adec_get_codec/adec_output_channels
#include "adec_dts.h"    // adec_dts_saw_extension() — "dts" vs "dts-hd" tag
#include "audio.h"       // audio_output_channels — negotiated program width
#include "meminfo.h"     // meminfo_get
#include "player_rsx.h"  // rsx_draw_overlay_quad
#include "rsxutil.h"     // display_width / display_height
#include "statsovl.h"    // runtime on/off
#include "timing.h"      // timing_get_us, timing_vblank_period_us, avsync_*
#include "overscan.h"
#include "plog.h"
#include "ui.h"          // cpu_rt_begin/end, drawTTF, drawRect, ttf_text_width
#include "ui_visuals.h"  // XMB_* palette
#include "i18n.h"        // TR
#include "video.h"       // vdec_es_bytes, vdec_stream_info, jbuf_*
#include "stream.h"      // stream_rx_stats
#include "audio_bitstream.h"   // audio_passthrough_active
#include "player_internal.h"   // PlayerState, player_stream_request, decode_ring_fill

// =========================================================================
//  Sample window sizing
// =========================================================================
//  The reference implementation this borrows its math from (VshFpsCounter)
//  keeps a session-wide histogram that never resets.  That is right for a
//  5-minute game benchmark and wrong here: our sessions are 2-hour movies.
//  With an unbounded pool a single hitch during startup buffering pins the
//  1% low for the rest of the film, and by the 90-minute mark no amount of
//  real stuttering can move the number — the metric stops reporting on the
//  present and starts reporting on ancient history.
//
//  So the histogram is a SLIDING WINDOW over the last STAT_WINDOW presented
//  frames, kept exact by decrementing the outgoing sample's bin as the new
//  one is added.  Fixed memory, O(1) per frame, no growth over a long movie.
//
//  STAT_WINDOW = 8192 presented frames.  At 24fps that is ~5.7 minutes —
//  long enough that the tails are meaningful (81 samples in the 1% tail, 8
//  in the 0.1% tail) and short enough that the reading still describes the
//  scene you are watching.  Smaller windows starve the 0.1% low: at 4096 it
//  is the 4th-worst frame, which is noise.
//
//  Bin width is 250us, NOT the reference's 1ms.  That project measures games
//  whose frame times sprawl across 16-50ms, where 1ms bins are fine.  Ours
//  sit on two tight spikes (16.68ms vblank, 41.71ms at 24fps), and 1ms bins
//  would quantise the 1% low to +/-0.6fps at 24fps — coarser than the effect
//  being hunted.  256 bins x 250us covers 0-63.75ms; bin 255 is the overflow
//  catch-all for anything slower.
#define STAT_WINDOW   8192
#define STAT_BINS     256
#define STAT_BIN_US   250
#define STAT_OVERFLOW (STAT_BINS - 1)

// Overlay recompose interval.  The numbers do not need to update at 60Hz to
// be readable, and a recompose costs a full text layout pass.
#define STAT_REFRESH_US 250000ULL

// Overlay panel geometry, in pixels at 720p; ovl_layout() scales it by the
// display (s_k) once, before the buffers are allocated.  The width is rounded
// up to a multiple of 16: bind_texture() sets pitch = width * 4, and RSX
// linear textures need a 64-byte-aligned pitch.
#define ST_PAD       12
#define ST_LINE      19
#define ST_PX        14.0f
#define ST_HDR       16.0f
#define ST_LABEL_W   96      // the label column
#define ST_ROWS_MAX  21      // every row the panel can have, with room to spare

// Samples of the byte counters for the network and video rates: one per
// recompose (~250 ms), so eight cover about two seconds.
#define RATE_SAMPLES 8

// =========================================================================
//  State
// =========================================================================
// Written by the vblank handler (RSX interrupt context) and read by the
// display thread — volatile so neither side caches them in a register.  The
// handler only ever INCREMENTS these; nothing clears them behind its back,
// so a reader can never observe a torn or rewound value.  64-bit loads and
// stores are single instructions on the Cell PPU, so no tearing there either.
static volatile u64 s_vblanks       = 0;
static volatile u64 s_vb_last_us    = 0;
static volatile u32 s_missed_vsync  = 0;
static volatile u32 s_fifo_samples  = 0;
static volatile u32 s_fifo_busy     = 0;
static u64          s_vb_nominal_us = 16683;   // refreshed on reset

// The RSX control register block.  gcmGetControlRegister() is resolved ONCE
// on the display thread and cached; the vblank handler only dereferences the
// pointer, never calls into libgcm.
static gcmControlRegister *s_ctrl = NULL;

// Display-thread only (presented-frame cadence).
static u64 s_ft_last_us   = 0;
static u64 s_ft_ema_us    = 0;
static u64 s_vb_base      = 0;    // hardware vblank count at session start
static u64 s_vb_at_frame  = 0;    // hardware vblank count at the previous frame
static u32 s_frames_shown = 0;
static u32 s_hold2 = 0, s_hold3 = 0, s_hold_other = 0, s_long_holds = 0;

// Sliding sample window + its histogram.
static u32 s_win_ft_us[STAT_WINDOW];
static u32 s_hist[STAT_BINS];
static int s_win_head  = 0;
static int s_win_count = 0;

// Main memory, sampled on the display thread (see on_frame_shown).
#define RAM_SAMPLE_US 500000ULL
#define RAM_LOW_KB    (20 * 1024)   // below this the free figure turns accent
static u32 s_ram_total_kb = 0;
static u32 s_ram_avail_kb = 0;
static u32 s_ram_min_kb   = 0xFFFFFFFFu;
static u64 s_ram_next_us  = 0;

// Audio thread.
static volatile u32 s_audio_blocks  = 0;
static volatile u32 s_audio_starves = 0;
static volatile u32 s_pcm_avail     = 0;
static volatile u32 s_dma_ahead     = 0;
static volatile u32 s_dma_total     = 0;
// Per-channel output peak, 1/32768 units, peak-hold with decay so a 5.3 ms
// DMA block does not flicker in a 250 ms recompose.  Slot order is the PS3
// CellAudio one: 0=FL 1=FR 2=FC 3=LFE 4=SL 5=SR 6=BL 7=BR.
static volatile u32 s_ch_peak[8]    = {0,0,0,0,0,0,0,0};
static volatile int s_ch_count      = 0;

// Overlay buffers.
static u32 *s_ovl_stage   = NULL;
static u32 *s_ovl_tex     = NULL;
static u32  s_ovl_tex_off = 0;
static u64  s_ovl_next_us = 0;
static u32  s_ovl_w = 0, s_ovl_h = 0;   // set once by ovl_layout()
static float s_k = 1.0f;                  // display scale of the layout

// The playback the panel describes (player_stats_bind).
static const PlayerState *s_ps = NULL;

struct RateSample { u64 t_us, rx, es; };
static RateSample s_rate[RATE_SAMPLES];
static int        s_rate_n = 0;

static inline int bin_of(u32 ft_us) {
    u32 b = ft_us / STAT_BIN_US;
    return (b >= STAT_OVERFLOW) ? STAT_OVERFLOW : (int)b;
}

// =========================================================================
//  Hooks
// =========================================================================

void player_stats_reset(void) {
    s_vblanks = 0; s_vb_last_us = 0; s_missed_vsync = 0;
    s_fifo_samples = 0; s_fifo_busy = 0;
    s_ft_last_us = 0; s_ft_ema_us = 0;
    s_vb_base = gcmGetVBlankCount();
    s_vb_at_frame = s_vb_base;
    s_frames_shown = 0;
    s_hold2 = s_hold3 = s_hold_other = s_long_holds = 0;
    s_win_head = 0; s_win_count = 0;
    memset(s_hist, 0, sizeof(s_hist));
    s_audio_blocks = s_audio_starves = 0;
    s_pcm_avail = s_dma_ahead = s_dma_total = 0;
    for (int i = 0; i < 8; i++) s_ch_peak[i] = 0;
    s_ch_count = 0;
    s_ram_total_kb = s_ram_avail_kb = 0;
    s_ram_min_kb = 0xFFFFFFFFu;
    s_ram_next_us = 0;
    s_ovl_next_us = 0;
    s_rate_n = 0;

    s_vb_nominal_us = (u64)timing_vblank_period_us();
    s_ctrl = gcmGetControlRegister();
}

// RSX interrupt context.  Integer only — no floats, no allocation, no
// logging, no libgcm calls.  Everything expensive is deferred to the 250ms
// recompose on the display thread.
void player_stats_on_vblank(void) {
    u64 now  = timing_get_us();
    u64 prev = s_vb_last_us;
    s_vb_last_us = now;
    s_vblanks++;

    if (prev != 0 && s_vb_nominal_us != 0) {
        u64 d = now - prev;
        // A vblank interval at or past 1.5x nominal means the handler did not
        // run on one or more edges — the display pipeline stalled through
        // them.  Round to the nearest whole period and count the ones lost.
        if (d >= s_vb_nominal_us + s_vb_nominal_us / 2) {
            u32 periods = (u32)((d + s_vb_nominal_us / 2) / s_vb_nominal_us);
            if (periods > 1) s_missed_vsync += periods - 1;
        }
    }

    // RSX command-buffer occupancy.  get == put means the GPU has drained
    // everything the CPU queued; get != put means work is still outstanding.
    // Sampling that once per vblank gives the fraction of vblanks that ended
    // with the FIFO non-empty.
    //
    // This is FIFO BACKLOG, not true GPU utilisation.  The RSX exposes no
    // busy counter to a PSL1GHT process — there is no equivalent of a
    // performance-monitor read — so a real "GPU busy %" is not obtainable
    // here.  Reported and labelled as what it actually measures; the overlay
    // prints "fifo", not "gpu".
    {
        gcmControlRegister *c = s_ctrl;
        if (c != NULL) {
            s_fifo_samples++;
            if (c->get != c->put) s_fifo_busy++;
        }
    }
}

// Display thread — called when a decoded frame is actually presented.
void player_stats_on_frame_shown(void) {
    u64 now = timing_get_us();

    // Vblanks this frame was held.  Taken from the HARDWARE counter
    // (gcmGetVBlankCount, safe to call off the display thread) rather than
    // the handler's own tally: if handler dispatch is ever delayed or an edge
    // is skipped, the hardware count is still exact, and the cadence figures
    // below are the whole point of this metric.  Diffed rather than cleared —
    // a vblank landing between a read and a clear would lose a count.
    u64 vc   = gcmGetVBlankCount();
    u32 held = (u32)(vc - s_vb_at_frame);
    s_vb_at_frame = vc;

    if (s_ft_last_us != 0) {
        u32 ft_us = (u32)(now - s_ft_last_us);

        // Sliding window: when full, the head slot holds the oldest sample,
        // so drop it from the histogram before overwriting.
        if (s_win_count == STAT_WINDOW)
            s_hist[bin_of(s_win_ft_us[s_win_head])]--;
        s_win_ft_us[s_win_head] = ft_us;
        s_hist[bin_of(ft_us)]++;
        s_win_head = (s_win_head + 1) & (STAT_WINDOW - 1);
        if (s_win_count < STAT_WINDOW) s_win_count++;

        // Integer EMA, same 9:1 weighting timing.cpp uses for the A/V diff.
        s_ft_ema_us = (s_ft_ema_us == 0) ? ft_us : (s_ft_ema_us * 9 + ft_us) / 10;

        // 2:3 pulldown cadence.  24fps on a 59.94Hz panel must alternate
        // 3,2,3,2,... — anything else is the pulldown failing to hold.
        if      (held == 2) s_hold2++;
        else if (held == 3) s_hold3++;
        else if (held != 0) {
            s_hold_other++;
            if (held > 3) s_long_holds++;
        }
    }

    s_ft_last_us = now;
    s_frames_shown++;

    // Main-memory sample, throttled to ~2Hz.  meminfo_get() is an LV2 syscall
    // (352), so it must NOT go anywhere near the vblank handler; the display
    // thread is fine, and the codebase already calls it from log lines.
    // Sampled HERE rather than in player_stats_get() so the low-water mark
    // keeps tracking while the overlay is switched off — memory pressure on
    // the 1080p path is worth catching whether or not anyone is looking.
    // Note this only covers steady-state playback: no frames present during
    // vdec_open's big transient alloc, so that dip belongs to vdec.cpp's own
    // meminfo log line, not to this figure.
    if (now >= s_ram_next_us) {
        s_ram_next_us = now + RAM_SAMPLE_US;
        u32 total = 0, avail = 0;
        if (meminfo_get(&total, &avail)) {
            s_ram_total_kb = total / 1024;
            s_ram_avail_kb = avail / 1024;
            if (s_ram_avail_kb < s_ram_min_kb) s_ram_min_kb = s_ram_avail_kb;
        }
    }

    // Keep the handler's nominal period current.  timing_init() only learns
    // the real display refresh once fps detection runs, which is AFTER
    // player_stats_reset() — on a 50Hz PAL set the cached 59.94Hz default
    // would otherwise skew the missed-vsync threshold for the whole session.
    // A 64-bit store is a single instruction here, so the handler can never
    // read a half-updated value.
    s_vb_nominal_us = (u64)timing_vblank_period_us();
}

// Audio thread — one call per DMA block written.
void player_stats_on_audio_write(int pcm_avail_pairs, int dma_ahead,
                                 int dma_total, bool starved) {
    s_audio_blocks++;
    if (starved) s_audio_starves++;
    s_pcm_avail = (u32)(pcm_avail_pairs < 0 ? 0 : pcm_avail_pairs);
    s_dma_ahead = (u32)(dma_ahead < 0 ? 0 : dma_ahead);
    s_dma_total = (u32)(dma_total < 0 ? 0 : dma_total);
}

// Audio thread — per-channel peak of the block just DMA'd.
void player_stats_on_audio_levels(const int *peaks, int count) {
    if (!peaks) return;
    if (count < 0) count = 0;
    if (count > 8) count = 8;
    s_ch_count = count;
    for (int c = 0; c < count; c++) {
        u32 pk  = (u32)(peaks[c] < 0 ? 0 : peaks[c]);
        u32 cur = s_ch_peak[c];
        // Rise instantly, fall ~3%/block (~0.2 s to decay a full-scale peak
        // to the noise floor) so a dialogue line stays visible long enough
        // to read off the overlay.
        s_ch_peak[c] = (pk > cur) ? pk : (cur - (cur >> 5));
    }
}

// =========================================================================
//  Aggregation (display thread, once per recompose)
// =========================================================================

void player_stats_get(PlayerStats *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));

    // Hardware truth for the count; s_vblanks (handler-observed) is only used
    // for the interval timing above.  A gap between the two would itself mean
    // the handler is missing edges.
    { u64 hw = gcmGetVBlankCount(); out->vblanks = (hw > s_vb_base) ? hw - s_vb_base : 0; }
    out->frames_shown = s_frames_shown;
    out->missed_vsync = s_missed_vsync;
    out->hold2        = s_hold2;
    out->hold3        = s_hold3;
    out->hold_other   = s_hold_other;
    out->long_holds   = s_long_holds;

    out->fps_ema  = (s_ft_ema_us > 0) ? 1000000.0f / (float)s_ft_ema_us : 0.0f;
    int count     = s_win_count;
    out->window_samples = (u32)count;

    if (count > 0) {
        // Single pass for mean / min / max / sum-of-squares.  Milliseconds
        // as double keeps the variance well-conditioned.
        double sum = 0.0, sum_sq = 0.0;
        u32 mn = 0xFFFFFFFFu, mx = 0;
        for (int i = 0; i < count; i++) {
            u32 v = s_win_ft_us[i];
            double ms = (double)v / 1000.0;
            sum    += ms;
            sum_sq += ms * ms;
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        double mean = sum / (double)count;
        out->ft_mean_ms = (float)mean;
        out->ft_min_ms  = (float)mn / 1000.0f;
        out->ft_max_ms  = (float)mx / 1000.0f;
        out->ft_cur_ms  = (float)s_win_ft_us[(s_win_head - 1 + STAT_WINDOW)
                                             & (STAT_WINDOW - 1)] / 1000.0f;

        double var = (sum_sq / (double)count) - (mean * mean);
        out->ft_stdev_ms = (var > 0.0) ? sqrtf((float)var) : 0.0f;

        // 1% / 0.1% lows: walk the histogram low-to-high until the cumulative
        // count crosses the percentile, giving the frame time that 99% (99.9%)
        // of the window beat.  The bin's UPPER edge is reported, so the fps is
        // the conservative reading.
        //
        // NOTE for 2:3 pulldown content: the presented-interval distribution
        // is bimodal BY DESIGN (33.4ms and 50.1ms alternating at 24fps), and
        // the 50ms frames are 40% of the window.  A healthy 24fps movie
        // therefore reads ~20fps here, and that is correct, not a fault.  The
        // signal is a reading BELOW that baseline; for a straight yes/no on
        // cadence, read hold2/hold3/hold_other instead.
        if (count >= 10) {
            u32 t99  = (u32)(((u64)99  * count + 99)  / 100);
            u32 t999 = (u32)(((u64)999 * count + 999) / 1000);
            u32 cum = 0;
            u32 p99_us = 0, p999_us = 0;
            for (int b = 0; b < STAT_BINS; b++) {
                cum += s_hist[b];
                if (!p99_us  && cum >= t99)  p99_us  = (u32)(b + 1) * STAT_BIN_US;
                if (!p999_us && cum >= t999) p999_us = (u32)(b + 1) * STAT_BIN_US;
                if (p99_us && p999_us) break;
            }
            out->fps_1_low  = p99_us  ? 1000000.0f / (float)p99_us  : 0.0f;
            out->fps_01_low = p999_us ? 1000000.0f / (float)p999_us : 0.0f;
        }
    }

    {
        u32 n = s_fifo_samples;
        out->rsx_fifo_busy_pct =
            n ? (100.0f * (float)s_fifo_busy / (float)n) : -1.0f;
    }

    out->ram_total_kb     = s_ram_total_kb;
    out->ram_avail_kb     = s_ram_avail_kb;
    out->ram_min_avail_kb = (s_ram_min_kb == 0xFFFFFFFFu) ? 0 : s_ram_min_kb;

    out->audio_blocks  = s_audio_blocks;
    out->audio_starves = s_audio_starves;
    out->ch_count = s_ch_count;
    for (int i = 0; i < 8; i++) out->ch_peak[i] = s_ch_peak[i];
    out->pcm_ring_pct  = 100.0f * (float)s_pcm_avail / (float)PCM_RING_HIGHWATER;
    if (out->pcm_ring_pct > 999.0f) out->pcm_ring_pct = 999.0f;
    out->dma_ring_pct  = s_dma_total
        ? (100.0f * (float)s_dma_ahead / (float)s_dma_total) : 0.0f;

    // A/V delta is already tracked by timing.cpp's EMA — read it, do not
    // recompute it.  Duplicating that logic would give a second number that
    // drifts from the one actually steering avsync_biased_period().
    out->avsync_us     = avsync_get_smoothed_diff();
    out->avsync_locked = avsync_is_locked();
}

// =========================================================================
//  Overlay
// =========================================================================

void player_stats_bind(const PlayerState *ps) {
    s_ps = ps;
    s_rate_n = 0;
}

// The panel is sized by the display, which is fixed at boot: a 1080p screen
// gets bigger text than a 480i one, and the buffers are allocated once.
static void ovl_layout(void) {
    if (display_height >= 1080)     s_k = 1.25f;
    else if (display_height >= 720) s_k = 1.0f;
    else                            s_k = 0.75f;
    s_ovl_w = ((u32)(500.0f * s_k) + 15u) & ~15u;   // pitch: a multiple of 16 pixels
    // Rows, the pads, and the small gaps above the heading and each section.
    s_ovl_h = (u32)(ST_PAD * s_k) * 2 + (u32)(ST_LINE * s_k) * ST_ROWS_MAX + (u32)(24 * s_k);
}

void player_stats_overlay_alloc(void) {
    if (s_ovl_stage && s_ovl_tex) return;
    if (!statsovl_shown()) return;     // never shown: pay nothing

    if (!s_ovl_w) ovl_layout();
    u32 bytes = s_ovl_w * s_ovl_h * 4;
    if (!s_ovl_stage) s_ovl_stage = (u32*)memalign(128, bytes);
    if (!s_ovl_tex) {
        s_ovl_tex = (u32*)rsxMemalign(64, bytes);
        if (s_ovl_tex) rsxAddressToOffset(s_ovl_tex, &s_ovl_tex_off);
    }
    if (s_ovl_stage) memset(s_ovl_stage, 0, bytes);
    if (s_ovl_tex)   memset((void*)s_ovl_tex, 0, bytes);
    if (!s_ovl_stage || !s_ovl_tex) plog("stats_ovl: alloc FAILED");
    else                            plog("stats_ovl: buffers ready");
}

// ---- rates ----------------------------------------------------------------
// Network and video bitrate are differences of byte counters over the last
// couple of seconds, sampled at each recompose.
static void rate_sample(u64 now) {
    u64 rxb = 0, rxw = 0; u32 rxc = 0;
    stream_rx_stats(&rxb, &rxw, &rxc);
    RateSample *r;
    if (s_rate_n < RATE_SAMPLES) r = &s_rate[s_rate_n++];
    else {
        memmove(&s_rate[0], &s_rate[1], sizeof(RateSample) * (RATE_SAMPLES - 1));
        r = &s_rate[RATE_SAMPLES - 1];
    }
    r->t_us = now; r->rx = rxb; r->es = vdec_es_bytes();
}

// Bits per second over the window, -1 while there is not yet a window.
static double rate_bps(bool video) {
    if (s_rate_n < 2) return -1.0;
    const RateSample *a = &s_rate[0], *b = &s_rate[s_rate_n - 1];
    if (b->t_us <= a->t_us) return -1.0;
    const u64 d = video ? b->es - a->es : b->rx - a->rx;
    return (double)d * 8.0 * 1e6 / (double)(b->t_us - a->t_us);
}

static void fmt_mbps(char *out, int cap, double bps) {
    if (bps < 0.0)          snprintf(out, cap, "...");
    else if (bps >= 1e6)    snprintf(out, cap, "%.1f Mbps", bps / 1e6);
    else                    snprintf(out, cap, "%.0f kbps", bps / 1e3);
}

// "23.976", "25", "59.94": a frame or refresh rate fraction, as people write it.
static void fmt_rate(char *out, int cap, u32 num, u32 den) {
    if (!num || !den) { snprintf(out, cap, "?"); return; }
    if (num % den == 0) { snprintf(out, cap, "%u", num / den); return; }
    const double v = (double)num / (double)den;
    if (num == 24000 && den == 1001) snprintf(out, cap, "%.3f", v);   // 23.976
    else                             snprintf(out, cap, "%.2f", v);   // 29.97, 59.94
}

static const char *channels_name(int n) {
    switch (n) {
    case 1:  return "Mono";
    case 2:  return "Stereo";
    case 6:  return "5.1";
    case 8:  return "7.1";
    default: return NULL;
    }
}

static const char *h264_profile_name(int idc) {
    switch (idc) {
    case 66:  return "Baseline";
    case 77:  return "Main";
    case 88:  return "Extended";
    case 100: return "High";
    case 110: return "High 10";
    case 122: return "High 4:2:2";
    case 244: return "High 4:4:4";
    default:  return NULL;
    }
}

static const char *container_name(LocalContainer c) {
    switch (c) {
    case LM_MKV:  return "MKV";
    case LM_MP4:  return "MP4";
    case LM_M2TS: return "M2TS";
    default:      return "TS";
    }
}

static const char *audio_codec_name(void) {
    switch (adec_get_codec()) {
    case ADEC_CODEC_AC3:    return "AC3";
    case ADEC_CODEC_EAC3:   return "E-AC3";
    case ADEC_CODEC_DTS:
        // "DTS-HD MA" only when the lossless extension is really decoded;
        // otherwise the extension is present and the core is what plays.
        return adec_dts_is_lossless() ? "DTS-HD MA"
             : adec_dts_saw_extension() ? "DTS-HD (core)" : "DTS";
    case ADEC_CODEC_TRUEHD: return "TrueHD";
    case ADEC_CODEC_AAC:    return "AAC";
    case ADEC_CODEC_FLAC:   return "FLAC";
    case ADEC_CODEC_PCM:    return "PCM";
    default:                return "MP3";
    }
}

// ---- layout ----------------------------------------------------------------

static int s_y = 0;   // baseline of the next row while composing

static int kpx(float v) { return (int)(v * s_k + 0.5f); }

// Cuts `s` from the right until it fits `w` pixels, on a UTF-8 boundary.
static void fit_text(char *s, float px, int w) {
    int len = (int)strlen(s);
    while (len > 1 && ttf_text_width(s, px) > w) {
        do { len--; } while (len > 1 && ((u8)s[len] & 0xC0) == 0x80);
        s[len] = 0;
    }
}

// One "label   value" row: the label in faint ink at the margin, the value
// at a fixed column so the values make a straight edge.
static void stat_row(const char *label, const char *value, u32 color) {
    const float px  = ST_PX * s_k;
    const int   col = kpx(ST_PAD + ST_LABEL_W);
    char v[128];
    snprintf(v, sizeof v, "%s", value);
    fit_text(v, px, (int)s_ovl_w - col - kpx(ST_PAD));
    drawTTF((u32)kpx(ST_PAD + 4), (u32)s_y, label, px, XMB_TEXT_FAINT);
    drawTTF((u32)col, (u32)s_y, v, px, color);
    s_y += kpx(ST_LINE);
}

// A section heading ("Video", "Audio", "Stream") with a hairline under it.
static void stat_section(const char *name) {
    s_y += kpx(4);
    drawTTF((u32)kpx(ST_PAD + 4), (u32)s_y, name, ST_PX * s_k, XMB_WHITE, true);
    s_y += kpx(ST_LINE);
}

static void stats_compose(const PlayerStats *s) {
    cpu_rt_begin(s_ovl_stage, s_ovl_w, s_ovl_h);
    // A full wipe: the panel's height follows its rows, which can change.
    memset(s_ovl_stage, 0, s_ovl_w * s_ovl_h * 4);

    const PlayerState *ps = s_ps;
    char v[160], a[48], b[48], c[48];
    s_y = kpx(ST_PAD);
    const int top = 0;

    drawTTF((u32)kpx(ST_PAD + 4), (u32)s_y, TR("Playback Statistics"), ST_HDR * s_k, XMB_WHITE, true);
    {
        const char *hide = TR("R3: Hide");
        const int hw = ttf_text_width(hide, ST_PX * s_k);
        drawTTF((u32)((int)s_ovl_w - kpx(ST_PAD) - hw), (u32)(s_y + kpx(1)), hide, ST_PX * s_k, XMB_TEXT_DIM);
    }
    s_y += kpx(ST_LINE) + kpx(4);

    // ---- how it is playing ----
    StreamRequest rq;
    bool have_rq = false;
    if (ps && !ps->local) { player_stream_request(ps, &rq); have_rq = true; }
    if (ps && ps->local && ps->local->net)
        snprintf(v, sizeof v, "%s,  %s", TR("Direct Play"), container_name(ps->local->container));
    else if (ps && ps->local)
        snprintf(v, sizeof v, "%s,  %s",
                 strcmp(ps->local->label, "Offline") == 0 ? TR("Offline") : TR("File on this console"),
                 container_name(ps->local->container));
    else if (have_rq && rq.vbitrate == 0 && !rq.no_video_copy)
        snprintf(v, sizeof v, "%s,  TS", TR("Direct Stream"));
    else if (have_rq) {
        fmt_mbps(a, sizeof a, (double)rq.vbitrate);
        snprintf(v, sizeof v, TR("Converted by the server to %s,  TS"), a);
    } else snprintf(v, sizeof v, "-");
    if (ps && ps->live) {
        char t[160];
        snprintf(t, sizeof t, "%s,  %s", TR("Live TV"), v);
        snprintf(v, sizeof v, "%s", t);
    }
    stat_row(TR("Playing"), v, XMB_TEXT);

    // ---- video ----
    stat_section(TR("Video"));
    {
        int prof = 0, lvl = 0, w = 0, h = 0;
        vdec_stream_info(&prof, &lvl, &w, &h);
        const char *pn = h264_profile_name(prof);
        if (pn && lvl) snprintf(v, sizeof v, "H.264  %s  %d.%d", pn, lvl / 10, lvl % 10);
        else           snprintf(v, sizeof v, "H.264");
        stat_row(TR("Codec"), v, XMB_TEXT);

        if (w <= 0 || h <= 0) { w = (int)jbuf_fw(); h = (int)jbuf_fh(); }
        snprintf(v, sizeof v, "%d x %d,  %s  %u x %u", w, h, TR("shown at"),
                 (unsigned)display_width, (unsigned)display_height);
        stat_row(TR("Resolution"), v, XMB_TEXT);

        u32 fn = 0, fd = 0, dn = 0, dd = 0;
        timing_content_fps(&fn, &fd);
        timing_display_rate(&dn, &dd);
        fmt_rate(a, sizeof a, fn, fd);
        fmt_rate(b, sizeof b, dn, dd);
        snprintf(c, sizeof c, "%.1f", (double)s->fps_ema);
        snprintf(v, sizeof v, TR("%s in the file,  %s shown,  %s Hz"), a, c, b);
        stat_row(TR("Frame rate"), v, XMB_TEXT);

        // The PS3's decoder does not drop pictures; what can go wrong is a
        // picture held on screen too long (a missed refresh, or a stall).
        snprintf(v, sizeof v, TR("%u held too long,  %u missed refreshes"),
                 (unsigned)s->long_holds, (unsigned)s->missed_vsync);
        stat_row(TR("Late frames"), v,
                 (s->long_holds || s->missed_vsync) ? XMB_ACCENT : XMB_TEXT);

        fmt_mbps(v, sizeof v, rate_bps(true));
        stat_row(TR("Bitrate"), v, XMB_TEXT);
    }

    // ---- audio ----
    stat_section(TR("Audio"));
    {
        const JFStream *track = NULL;
        if (ps && ps->have_tracks && ps->tracks.n_audio > 0) {
            int i = ps->cur_audio >= 0 ? ps->cur_audio : ps->tracks.default_audio;
            if (i >= 0 && i < ps->tracks.n_audio) track = &ps->tracks.audio[i];
        }
        snprintf(v, sizeof v, "%s", track && track->label[0] ? track->label : audio_codec_name());
        stat_row(TR("Track"), v, XMB_TEXT);

        if (audio_passthrough_active()) {
            snprintf(v, sizeof v, "%s,  %s", audio_codec_name(), TR("sent to the receiver"));
        } else {
            const int in = adec_output_channels(), out = audio_output_channels();
            const char *ni = channels_name(in), *no = channels_name(out);
            if (ni) snprintf(a, sizeof a, "%s", ni); else snprintf(a, sizeof a, "%d ch", in);
            if (no) snprintf(b, sizeof b, "%s", no); else snprintf(b, sizeof b, "%d ch", out);
            snprintf(v, sizeof v, TR("%s %s decoded,  PCM %s to the PS3"), audio_codec_name(), a, b);
        }
        stat_row(TR("Output"), v, XMB_TEXT);

        if (s->audio_starves) {
            snprintf(v, sizeof v, TR("%u gaps in the sound"), (unsigned)s->audio_starves);
            stat_row(TR("Underruns"), v, XMB_ACCENT);
        }
    }

    // ---- subtitles ----
    stat_section(TR("Subtitles"));
    {
        if (!ps || ps->cur_sub < 0 || ps->cur_sub >= ps->tracks.n_subs) {
            snprintf(v, sizeof v, "%s", TR("Off"));
        } else {
            const JFStream *st = &ps->tracks.subs[ps->cur_sub];
            const char *how = ps->sub_is_pgs  ? TR("drawn by the PS3, PGS")
                            : ps->sub_is_text ? TR("drawn by the PS3, text")
                            :                   TR("burned in by the server");
            snprintf(v, sizeof v, "%s  (%s)", st->label[0] ? st->label : st->codec, how);
        }
        stat_row(TR("Track"), v, XMB_TEXT);
    }

    // ---- stream ----
    stat_section(TR("Stream"));
    {
        int rn = 0, rc = 0;
        decode_ring_fill(&rn, &rc);
        if (rc > 0)
            snprintf(v, sizeof v, TR("%d frames ready,  read-ahead %d%%"),
                     jbuf_count(), (int)((s64)rn * 100 / rc));
        else
            snprintf(v, sizeof v, TR("%d frames ready"), jbuf_count());
        stat_row(TR("Buffer"), v, XMB_TEXT);

        if (!ps || !ps->local || ps->local->net) {
            fmt_mbps(v, sizeof v, rate_bps(false));
            stat_row(TR("Network"), v, XMB_TEXT);
        }

        snprintf(v, sizeof v, "%+.0f ms%s", (double)s->avsync_us / 1000.0,
                 s->avsync_locked ? "" : TR(",  settling"));
        stat_row(TR("A/V sync"), v, s->avsync_locked ? XMB_TEXT : XMB_ACCENT);

        if (s->ram_total_kb) {
            snprintf(v, sizeof v, TR("%u MB free of %u"),
                     (unsigned)(s->ram_avail_kb / 1024), (unsigned)(s->ram_total_kb / 1024));
            stat_row(TR("Memory"), v,
                     (s->ram_avail_kb < RAM_LOW_KB) ? XMB_ACCENT : XMB_TEXT);
        }
    }

    // Backdrop and frame, sized to the rows just drawn.  The text is already
    // in the buffer, so the backdrop goes UNDER it: only pixels the text did
    // not touch (alpha 0) take the dim colour.
    int bottom = s_y + kpx(ST_PAD) - kpx(ST_LINE) + kpx(ST_PX + 6);
    if (bottom > (int)s_ovl_h) bottom = (int)s_ovl_h;
    {
        const u32 dim = (u32)190 << 24;
        for (int y = top; y < bottom; y++) {
            u32 *row = s_ovl_stage + y * s_ovl_w;
            for (u32 x = 0; x < s_ovl_w; x++) {
                const u32 p = row[x];
                const u32 pa = p >> 24;
                if (pa == 0) row[x] = dim;
                else if (pa < 255) {
                    // Text edge over the backdrop: blend toward black by
                    // what the glyph left uncovered.
                    const u32 out_a = pa + (190 * (255 - pa)) / 255;
                    const u32 r = ((p >> 16) & 0xFF) * pa / out_a;
                    const u32 g = ((p >>  8) & 0xFF) * pa / out_a;
                    const u32 bl = ( p        & 0xFF) * pa / out_a;
                    row[x] = (out_a << 24) | (r << 16) | (g << 8) | bl;
                }
            }
        }
    }
    drawRect(0, 0, s_ovl_w, 1, XMB_HAIRLINE);
    drawRect(0, (u32)(bottom - 1), s_ovl_w, 1, XMB_HAIRLINE);
    drawRect(0, 0, 1, (u32)bottom, XMB_HAIRLINE);
    drawRect(s_ovl_w - 1, 0, 1, (u32)bottom, XMB_HAIRLINE);
    drawRect(0, 0, (u32)kpx(3), (u32)bottom, XMB_ACCENT);

    cpu_rt_end();

    memcpy((void*)s_ovl_tex, s_ovl_stage, s_ovl_w * s_ovl_h * 4);
}

void player_stats_render_overlay(void) {
    // Runtime switch: off means zero rendering cost.  No compose, no text
    // layout, no draw call — the counters above keep running so the panel is
    // already populated the moment it is switched on.
    if (!statsovl_shown()) return;

    if (!s_ovl_stage || !s_ovl_tex) {
        player_stats_overlay_alloc();          // enabled mid-session
        if (!s_ovl_stage || !s_ovl_tex) return;
    }

    // Recompose at ~4Hz.  The GPU quad below still goes out every frame —
    // it is a few FIFO words, unlike the text layout it presents.
    u64 now = timing_get_us();
    if (now >= s_ovl_next_us) {
        s_ovl_next_us = now + STAT_REFRESH_US;
        rate_sample(now);
        PlayerStats s;
        player_stats_get(&s);
        stats_compose(&s);
    }

    // Top-RIGHT, inside the overscan-safe area so a CRT does not eat it.
    // Right rather than left because the HUD puts the paused item title at
    // top-left and the two would overlap.
    int px = (int)display_width - (int)s_ovl_w - kpx(24) - overscan_x();
    int py = overscan_y() + kpx(24);
    if (px < 0) px = 0;
    if (py + (int)s_ovl_h > (int)display_height) py = (int)display_height - (int)s_ovl_h;
    if (py < 0) py = 0;
    rsx_draw_overlay_quad(s_ovl_tex_off, s_ovl_w, s_ovl_h, px, py, (int)s_ovl_w, (int)s_ovl_h);
}

#endif  // ENABLE_PLAYER_STATS

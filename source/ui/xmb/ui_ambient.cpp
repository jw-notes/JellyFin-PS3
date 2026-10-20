// The ambient screensaver ("film-reel" mode).  State and motion are
// render/experience.h (amb_*), host-tested; this file draws them.
//
// After a few idle minutes on the XMB the interface dissolves under a dark
// cover, the cover lifts part way so the JellyWave shows again -- dimmed,
// otherwise exactly as it always draws -- and one poster at a time drifts
// slowly across the middle of the screen, with a soft glow in its own colour
// (render/art_colour.h), a faint reflection, the lockup and clock, and one
// line of metadata.  Any button dissolves it back; that press does nothing
// else.
//
// COST.  While it runs, the XMB's own phases are not drawn at all, so the
// frame is cheaper than a normal one: the wave, one full-screen blended quad,
// one glow fan, two textured quads and a few cached text runs.  The artwork is
// what Home already has in its thumbnail cache, at the size Home caches it,
// so nothing new is fetched or decoded.  No allocation, no framebuffer read,
// no extra rsxSync.  The JellyWave renderer is not touched.
//
// ARTWORK MODE (Settings > Display > Screensaver) shows the same items as
// still, full-screen backdrops instead, crossfading from one to the next,
// with the title, episode, year / rating / runtime, the file's video and audio
// and the synopsis at the bottom left.  Backdrops are fetched one at a time on
// a worker (detail_media_load, the detail page's loader) into two VRAM slots
// of their own; the first starts a few seconds BEFORE the screensaver does,
// so it opens on artwork.  A slide is only shown once its backdrop is in
// VRAM, and items with none are skipped -- it never falls back to a classic
// slide unless no item has a backdrop at all.  The synopsis and file details
// are api_facts' (the Home and peek cache).
//
// Only with the spine gate on.  The idle time is jellyfin_screensaver_delay.txt
// (seconds, 30-300; Settings > Display > Screensaver Delay), else the older
// jellyfin_ambient.txt (minutes), else 3 minutes.  jellyfin_screensaver.txt
// holds the mode.

#include <stdio.h>
#include <string.h>

#include "ui_internal.h"
#include "ui_render_internal.h"
#include "ui_card_gpu.h"
#include "ui_wave.h"
#include "ui_art.h"
#include "experience.h"
#include "depth.h"          // depth_mix_q
#include "ui_depth.h"       // depth_fit_text
#include "thumbnail_cache.h"
#include "timing.h"
#include "plog.h"
#include "jf_paths.h"
#include "detail_media.h"
#include "api_facts.h"
#include "line_break.h"

#include <sys/thread.h>

#define AMB_FILE  "jellyfin_ambient.txt"
#define MODE_FILE "jellyfin_screensaver.txt"
#define DELAY_FILE "jellyfin_screensaver_delay.txt"
#define AMB_MAX   12

static amb_state  s_amb;
static bool       s_amb_ready = false;
static AmbientArt s_art[AMB_MAX];
static int        s_n = 0;
static int        s_last_phase = AMB_OFF;
static amb_frame  s_f;
static bool       s_art_full = false;  // this frame drew artwork mode (text follows suit)

static int                s_mode = AMB_MODE_ARTWORK;    // until one is chosen
static unsigned long long s_idle_cfg_us = 0;   // the Screensaver Delay

static const int k_delays[] = { 30, 60, 120, 180, 240, 300 };   // seconds
#define N_DELAYS ((int)(sizeof k_delays / sizeof k_delays[0]))
static int s_delay_s = 180;

static void ambient_init_once(void) {
    if (s_amb_ready) return;
    s_amb_ready = true;
    int minutes = 3;
    FILE *fp = fopen(jf_data_path(AMB_FILE), "r");
    if (fp) {
        if (fscanf(fp, "%d", &minutes) != 1) minutes = 3;
        fclose(fp);
    }
    if (minutes < 0) minutes = 0;
    if (minutes > 120) minutes = 120;
    s_delay_s = minutes * 60;
    fp = fopen(jf_data_path(DELAY_FILE), "r");
    if (fp) {
        int secs = 0;
        if (fscanf(fp, "%d", &secs) == 1) s_delay_s = secs;
        fclose(fp);
    }
    if (s_delay_s < k_delays[0]) s_delay_s = k_delays[0];
    if (s_delay_s > k_delays[N_DELAYS - 1]) s_delay_s = k_delays[N_DELAYS - 1];
    fp = fopen(jf_data_path(MODE_FILE), "r");
    if (fp) {
        int m = AMB_MODE_CLASSIC;
        if (fscanf(fp, "%d", &m) == 1 && m >= 0 && m < AMB_MODE__COUNT) s_mode = m;
        fclose(fp);
    }
    s_idle_cfg_us = (unsigned long long)s_delay_s * 1000000ull;
    amb_init(&s_amb, timing_get_us(), s_mode == AMB_MODE_OFF ? 0 : s_idle_cfg_us);
    char b[80];
    snprintf(b, sizeof b, "ambient: idle %d s, mode %d", s_delay_s, s_mode);
    plog(b);
}

const char *ambient_delay_label(void) {
    ambient_init_once();
    static char b[24];
    if (s_delay_s < 60) snprintf(b, sizeof b, TR("%d sec"), s_delay_s);
    else                snprintf(b, sizeof b, TR("%d min"), s_delay_s / 60);
    return b;
}

void ambient_delay_step(int dir) {
    ambient_init_once();
    int i = 0;
    while (i < N_DELAYS - 1 && k_delays[i] < s_delay_s) i++;
    i = (i + (dir < 0 ? N_DELAYS - 1 : 1)) % N_DELAYS;
    s_delay_s = k_delays[i];
    FILE *fp = fopen(jf_data_path(DELAY_FILE), "w");
    if (fp) { fprintf(fp, "%d\n", s_delay_s); fclose(fp); }
    s_idle_cfg_us = (unsigned long long)s_delay_s * 1000000ull;
    if (s_mode != AMB_MODE_OFF) s_amb.idle_us = s_idle_cfg_us;
    s_amb.t_input = timing_get_us();
}

int ambient_mode(void) { ambient_init_once(); return s_mode; }

const char *ambient_mode_label(void) {
    switch (ambient_mode()) {
    case AMB_MODE_ARTWORK: return TR("Artwork");
    case AMB_MODE_OFF:     return TR("Off");
    default:               return TR("Classic");
    }
}

void ambient_mode_step(int dir) {
    ambient_init_once();
    s_mode = (s_mode + (dir < 0 ? AMB_MODE__COUNT - 1 : 1)) % AMB_MODE__COUNT;
    FILE *fp = fopen(jf_data_path(MODE_FILE), "w");
    if (fp) { fprintf(fp, "%d\n", s_mode); fclose(fp); }
    // From the next idle wait on; nothing is showing while Settings is.
    s_amb.idle_us = s_mode == AMB_MODE_OFF ? 0 : s_idle_cfg_us;
    s_amb.t_input = timing_get_us();
}

// --- artwork mode: the backdrop worker ---------------------------------------
//
// One fetch at a time.  The render thread starts it (bd_start), the worker
// fills s_job_bm and sets s_job_done, and the render thread joins it, uploads
// the bitmap to a VRAM slot and frees it (bd_poll).  The worker touches only
// the s_job_* fields.

#define BD_W 960
#define BD_H 540

static sys_ppu_thread_t s_job_tid;
static bool             s_job_running = false;   // render thread only
static volatile bool    s_job_done    = false;   // worker -> render thread
static bool             s_job_ok      = false;
static int              s_job_idx     = -1;
static unsigned         s_job_gen     = 0;      // s_bd_gen when it started
static unsigned         s_bd_gen      = 0;      // bumped by bd_reset: a new slide list
static char             s_job_id[64];
static Bitmap           s_job_bm;

// Per slide: which of the two VRAM slots holds its backdrop (-1 none), or
// that it has none to show.
static int  s_bd_slot[AMB_MAX];
static bool s_bd_none[AMB_MAX];
static int  s_slot_idx[2] = { -1, -1 };          // the slide each slot holds
// Artwork mode has a slide list and backdrops: from the prefetch shortly
// before the screensaver starts until it has closed again.
static bool s_live = false;

static void bd_reset(void) {
    s_bd_gen++;
    for (int i = 0; i < AMB_MAX; i++) { s_bd_slot[i] = -1; s_bd_none[i] = false; }
    s_slot_idx[0] = s_slot_idx[1] = -1;
}

static void bd_thread(void *arg) {
    (void)arg;
    s_job_ok = detail_media_load(s_job_id, "Backdrop", BD_W, BD_H, 0.2f, &s_job_bm);
    __sync_synchronize();
    s_job_done = true;
    sysThreadExit(0);
}

static void bd_start(int idx) {
    if (s_job_running || idx < 0 || idx >= s_n || !s_art[idx].img_id) return;
    snprintf(s_job_id, sizeof s_job_id, "%s", s_art[idx].img_id);
    s_job_idx  = idx;
    s_job_gen  = s_bd_gen;
    s_job_done = false;
    s_job_ok   = false;
    static char name[] = "jf_amb_art";
    if (sysThreadCreate(&s_job_tid, bd_thread, NULL, 1600, 64 * 1024, THREAD_JOINABLE, name) == 0)
        s_job_running = true;
    else
        s_bd_none[idx] = true;          // no worker: that slide stays classic
}

// Collect a finished fetch.  `keep` is the slide on screen, whose slot must
// not be overwritten.  Called every frame, also after the screensaver has
// closed, so a fetch that outlives it is still joined and freed.
static void bd_poll(int keep) {
    if (!s_job_running || !s_job_done) return;
    u64 ret;
    sysThreadJoin(s_job_tid, &ret);
    s_job_running = false;
    const int idx = s_job_idx;
    if (!s_job_ok) {
        if (s_job_gen == s_bd_gen && idx >= 0 && idx < AMB_MAX) s_bd_none[idx] = true;
        return;
    }
    if (s_job_gen != s_bd_gen) { detail_media_free(&s_job_bm); return; }   // an older list
    if (s_live && idx >= 0 && idx < s_n) {
        const int slot = (keep >= 0 && s_slot_idx[0] == keep) ? 1 : 0;
        if (s_slot_idx[slot] >= 0) s_bd_slot[s_slot_idx[slot]] = -1;
        if (ui_gpu_tex_upload(GPU_TEX_AMB0 + slot, &s_job_bm)) {
            s_slot_idx[slot] = idx;
            s_bd_slot[idx]   = slot;
        } else {
            s_slot_idx[slot] = -1;
            s_bd_none[idx]   = true;
        }
    }
    detail_media_free(&s_job_bm);
}

// --- artwork mode: the slideshow ---------------------------------------------
//
// Its own timeline rather than amb_slide_at's: a slide appears only once its
// backdrop is in VRAM, and the next crossfades in over it only once that one
// is loaded too, so there is never a gap or a classic slide in between.

#define ART_HOLD_US     14000000ull   // a backdrop's time on screen
#define ART_FADE_US      1600000ull   // the crossfade to the next
#define ART_PREFETCH_US  5000000ull   // how long before the screensaver to start

static int                s_cur = -1, s_nxt = -1;
static unsigned long long s_cur_t = 0;   // when s_cur arrived
static unsigned long long s_xf_t  = 0;   // crossfade start, 0 = none
static bool               s_cur_first = false;   // s_cur fades in from black

// The next slide with a backdrop that may still come, from `from` on; -1 when
// none is left.
static int art_candidate(int from) {
    for (int k = 0; k < s_n; k++) {
        const int i = ((from % s_n) + s_n + k) % s_n;
        if (!s_bd_none[i]) return i;
    }
    return -1;
}

// Artwork mode can show something (or will, once the first backdrop lands).
static bool art_usable(void) {
    return s_mode == AMB_MODE_ARTWORK && s_live && s_n > 0 && art_candidate(0) >= 0;
}

static void art_begin(void) {
    s_n = xmb_home_ambient_art(s_art, AMB_MAX);
    bd_reset();
    s_cur = s_nxt = -1;
    s_cur_t = s_xf_t = 0;
    s_live = true;
}

static void art_tick(unsigned long long now) {
    if (!s_live || s_n <= 0) return;
    if (s_cur < 0) {
        const int c = art_candidate(0);
        if (c < 0) return;
        if (s_bd_slot[c] >= 0) { s_cur = c; s_cur_t = now; s_cur_first = true; return; }
        if (!s_job_running) bd_start(c);
        return;
    }
    if (s_xf_t) {
        if (now - s_xf_t >= ART_FADE_US) {
            s_cur = s_nxt; s_nxt = -1; s_cur_t = now; s_xf_t = 0; s_cur_first = false;
        }
        return;
    }
    const int n = art_candidate(s_cur + 1);
    if (n < 0 || n == s_cur) return;             // only one has a backdrop: it stays
    // From halfway through, have the next one ready; at the end, fade to it.
    if (s_bd_slot[n] < 0) {
        if (now - s_cur_t >= ART_HOLD_US / 2 && !s_job_running) bd_start(n);
        return;
    }
    if (now - s_cur_t >= ART_HOLD_US) { s_nxt = n; s_xf_t = now; }
}

// 0..1, eased.
static float art_ease(float x) {
    x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
    return x * x * (3.0f - 2.0f * x);
}

// The opacity of the slide on screen and of the one fading in.
static void art_alphas(unsigned long long now, float *a_cur, float *a_nxt) {
    *a_cur = s_cur < 0 ? 0.0f
           : s_cur_first ? art_ease((float)(now - s_cur_t) / (float)ART_FADE_US) : 1.0f;
    *a_nxt = s_xf_t ? art_ease((float)(now - s_xf_t) / (float)ART_FADE_US) : 0.0f;
}

static bool any_button(void) {
    return btn_cur.up || btn_cur.down || btn_cur.left || btn_cur.right ||
           btn_cur.cross || btn_cur.circle || btn_cur.square || btn_cur.triangle ||
           btn_cur.start || btn_cur.select ||
           btn_cur.l1 || btn_cur.r1 || btn_cur.l2 || btn_cur.r2;
}

bool ambient_update(bool allowed) {
    ambient_init_once();
    const unsigned long long now = timing_get_us();
    // A modal (the update popup, overscan calibration) counts as activity:
    // the screensaver never starts over one.
    // A gap since the last XMB frame means another screen had the display
    // (detail, music, a film): that was activity, not idleness.
    static unsigned long long s_prev = 0;
    const bool away = s_prev && now - s_prev > 1000000ull;
    s_prev = now;
    const int swallow = amb_tick(&s_amb, now, any_button() || !allowed || away);
    // Artwork: build the list and start the first backdrop a few seconds
    // before the screensaver is due, so it opens on artwork.  Activity in the
    // meantime calls it off.
    if (s_mode == AMB_MODE_ARTWORK && s_amb.phase == AMB_OFF && s_amb.idle_us) {
        const unsigned long long idle = now - s_amb.t_input;
        if (!s_live && idle + ART_PREFETCH_US >= s_amb.idle_us) art_begin();
        else if (s_live && idle + ART_PREFETCH_US < s_amb.idle_us) s_live = false;
    }
    if (s_amb.phase != s_last_phase) {
        if (s_amb.phase == AMB_IN || s_amb.phase == AMB_ON) {
            if (s_mode == AMB_MODE_ARTWORK) { if (!s_live) art_begin(); }
            else if (s_amb.phase == AMB_ON) s_n = xmb_home_ambient_art(s_art, AMB_MAX);
            if (s_amb.phase == AMB_ON) {
                char b[48];
                snprintf(b, sizeof b, "ambient: on, %d items, mode %d", s_n, s_mode);
                plog(b);
            }
        } else if (s_amb.phase == AMB_OFF) {
            s_live = false;
            plog("ambient: off");
        }
        s_last_phase = s_amb.phase;
    }
    amb_eval(&s_amb, now, &s_f);
    bd_poll(s_cur);
    if (s_mode == AMB_MODE_ARTWORK) art_tick(now);
    return swallow != 0;
}

bool ambient_draw_ui(void) { return s_f.draw_ui; }

static inline u8 a8(float a) {
    a = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
    return (u8)(a * 255.0f + 0.5f);
}

// Where the current slide's poster sits this frame, screen px.
typedef struct { int x, y, w, h; float a; int idx; } AmbPoster;

static bool ambient_poster(AmbPoster *p) {
    if (s_n <= 0) return false;
    amb_slide sl;
    amb_slide_at(timing_get_us() - s_amb.t_on, s_n, &sl);
    if (sl.idx < 0) return false;
    const float bw = 232.0f * sl.scale, bh = 348.0f * sl.scale;
    const float cx = 430.0f + sl.dx, cy = 330.0f + sl.dy;
    p->x = XMB_OX + UIS_W((int)(cx - bw * 0.5f));
    p->y = XMB_OY + UIS_H((int)(cy - bh * 0.5f));
    p->w = UIS_W((int)bw);
    p->h = UIS_H((int)bh);
    p->a = sl.a * s_f.art_a;
    p->idx = sl.idx;
    return true;
}

// Artwork mode: the backdrop on screen, still and filling it, the next one
// crossfading in over it, under the scrims that keep the clock and the text
// readable.  Black until the first backdrop is in.
static void ambient_artwork_gpu(int W, int H) {
    const unsigned long long now = timing_get_us();
    float ac, an;
    art_alphas(now, &ac, &an);
    const float art = s_f.art_a;
    ui_rect_gpu_draw(0, 0, W, H, 0x00000000, a8(s_f.cover > art ? s_f.cover : art));
    if (s_cur >= 0 && s_bd_slot[s_cur] >= 0)
        ui_gpu_tex_draw_a(GPU_TEX_AMB0 + s_bd_slot[s_cur], 0, 0, W, H, a8(art * ac));
    if (s_xf_t && s_nxt >= 0 && s_bd_slot[s_nxt] >= 0)
        ui_gpu_tex_draw_a(GPU_TEX_AMB0 + s_bd_slot[s_nxt], 0, 0, W, H, a8(art * an));
    // Left and bottom for the text, top for the clock.
    const u8 sl_a = a8(0.86f * art), sb_a = a8(0.92f * art), st_a = a8(0.55f * art);
    ui_rect_gpu_gradient(0, 0, (W * 2) / 3, H, 0x00000000, sl_a, 0, sl_a, 0);
    ui_rect_gpu_gradient(0, (H * 2) / 5, W, H - (H * 2) / 5, 0x00000000, 0, 0, sb_a, sb_a);
    ui_rect_gpu_gradient(0, 0, W, H / 6, 0x00000000, st_a, st_a, 0, 0);
    ui_card_gpu_end();
}

// GPU phase, in place of the XMB's own (only when !ambient_draw_ui()).
void ambient_gpu(void) {
    if (!s_f.draw_ambient) return;
    const int W = (int)display_width, H = (int)display_height;
    s_art_full = art_usable();
    if (s_art_full) { ambient_artwork_gpu(W, H); return; }
    ui_rect_gpu_draw(0, 0, W, H, 0x00000000, a8(s_f.cover));

    AmbPoster p;
    if (ambient_poster(&p)) {
        const AmbientArt *a = &s_art[p.idx];
        // Ask for this slide and the next so the change never waits on a
        // decode; the cache ages out anything no longer asked for.
        thumb_request(a->img_id, a->src_w, a->src_h);
        const AmbientArt *nx = &s_art[(p.idx + 1) % s_n];
        thumb_request(nx->img_id, nx->src_w, nx->src_h);

        const art_palette pal =
            ui_art_palette(a->img_id, thumb_get(a->img_id, a->src_w, a->src_h));
        const u32 g = pal.glow;
        wave_draw_glow_gpu(p.x + p.w / 2, p.y + p.h / 2, p.w, (p.h * 3) / 4,
                           (u8)(g >> 16), (u8)(g >> 8), (u8)g, a8(0.30f * p.a));
        u32 off = 0, pitch = 0;
        if (thumb_gpu_texture(a->img_id, a->src_w, a->src_h, &off, &pitch)) {
            const u8 pa = a8(p.a);
            ui_card_gpu_draw_ex(off, (u32)a->src_w, (u32)a->src_h, pitch,
                                p.x, p.y, p.w, p.h, 0.0f, 1.0f, pa);
            // A faint reflection: the bottom sixth, mirrored.
            const int rh = p.h / 6;
            ui_card_gpu_draw_ex(off, (u32)a->src_w, (u32)a->src_h, pitch,
                                p.x, p.y + p.h + UIS_H(4), p.w, rh,
                                1.0f, 1.0f - (float)rh / (float)p.h, a8(0.14f * p.a));
        }
    }
    ui_card_gpu_end();
}

// --- artwork mode: the information ------------------------------------------

static int art_char_w(const char *ch, void *ctx) {
    return ttf_text_width(ch, *(const float *)ctx);
}

// The synopsis broken into lines once per item and size.
#define ART_SYN_LINES 3
static char s_syn[ART_SYN_LINES][200];
static int  s_syn_n = -1;
static char s_syn_key[64];

static void art_wrap(const char *id, const char *text, float px, int max_w) {
    if (s_syn_n >= 0 && strcmp(s_syn_key, id) == 0) return;
    snprintf(s_syn_key, sizeof s_syn_key, "%s", id);
    s_syn_n = 0;
    const char *p = text;
    while (*p && s_syn_n < ART_SYN_LINES) {
        while (*p == ' ' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        int len = lb_fit(p, max_w, (int)sizeof s_syn[0] - 4, art_char_w, &px);
        if (len <= 0) break;
        const char *nl = (const char *)memchr(p, '\n', (size_t)len);
        if (nl) len = (int)(nl - p);
        memcpy(s_syn[s_syn_n], p, (size_t)len);
        s_syn[s_syn_n][len] = '\0';
        p += len;
        // The last line that fits ends in an ellipsis when there is more.
        if (s_syn_n == ART_SYN_LINES - 1 && *p) {
            while (len > 0 && s_syn[s_syn_n][len - 1] == ' ') len--;
            snprintf(s_syn[s_syn_n] + len, sizeof s_syn[0] - (size_t)len, "\xE2\x80\xA6");
        }
        s_syn_n++;
    }
}

// "2014  ·  PG-13  ·  2h 8m"
static void art_meta(char *b, size_t cap, const AmbientArt *a, const ItemFacts *fx, bool have) {
    const char *parts[3]; int n = 0;
    char rt[16] = "";
    if (a->year && a->year[0]) parts[n++] = a->year;
    const char *rating = have && fx->rating[0] ? fx->rating : a->rating;
    if (rating && rating[0]) parts[n++] = rating;
    if (have && fx->runtime_secs >= 60) {
        const unsigned m = fx->runtime_secs / 60;
        if (m >= 60) snprintf(rt, sizeof rt, "%uh %um", m / 60, m % 60);
        else         snprintf(rt, sizeof rt, "%um", m);
        parts[n++] = rt;
    }
    b[0] = '\0';
    size_t len = 0;
    for (int i = 0; i < n && len < cap; i++)
        len += (size_t)snprintf(b + len, cap - len, "%s%s", i ? "  \xC2\xB7  " : "", parts[i]);
}

// One slide's text block at opacity t.  Bottom left, laid out upward from a
// fixed baseline so a short synopsis does not leave a gap at the bottom.
static void art_text_block(int idx, float t) {
    if (idx < 0 || idx >= s_n || t < 0.04f) return;
    const AmbientArt *a = &s_art[idx];
    const char *id = a->item_id && a->item_id[0] ? a->item_id : a->img_id;
    ItemFacts fx;
    const bool have = id && facts_get(id, &fx, NULL);
    if (!have && id) facts_request(id);

    const int x = XMB_OX + UIS_W(72);
    const int max_w = UIS_W(640);
    const float spx = UIS_TF(15.0f);
    const int lh = UIS_H(22);
    int y = XMB_OY + UIS_H(640);           // the bottom of the block

    if (have && fx.overview[0]) {
        art_wrap(id, fx.overview, spx, max_w);
        y -= s_syn_n * lh;
        for (int i = 0; i < s_syn_n; i++)
            drawTTF((u32)x, (u32)(y + i * lh), s_syn[i], spx,
                    depth_mix_q(0x00000000, XMB_TEXT, t * 0.88f));
        y -= UIS_H(14);
    }
    if (have && (fx.video[0] || fx.audio[0])) {
        char tech[96], fit[96];
        snprintf(tech, sizeof tech, "%s%s%s", fx.video,
                 fx.video[0] && fx.audio[0] ? "  \xC2\xB7  " : "", fx.audio);
        const float px = UIS_TF(11.0f);
        depth_fit_text(fit, sizeof fit, tech, px, UI_FACE_SPEC, max_w);
        y -= UIS_H(20);
        drawTTF_face((u32)x, (u32)y, fit, px,
                     depth_mix_q(0x00000000, fx.lossless ? XMB_ACCENT_ALT : XMB_TEXT_DIM, t),
                     UI_FACE_SPEC);
    }
    char meta[96];
    art_meta(meta, sizeof meta, a, &fx, have);
    if (meta[0]) {
        y -= UIS_H(26);
        drawTTF((u32)x, (u32)y, meta, UIS_TF(15.0f), depth_mix_q(0x00000000, XMB_TEXT_DIM, t));
    }
    if (a->episode && a->episode[0]) {
        char fit[160];
        depth_fit_text(fit, sizeof fit, a->episode, UIS_TF(19.0f), UI_FACE_DISPLAY, max_w);
        y -= UIS_H(30);
        drawTTF_face((u32)x, (u32)y, fit, UIS_TF(19.0f),
                     depth_mix_q(0x00000000, XMB_TEXT, t * 0.95f), UI_FACE_DISPLAY);
    }
    char line[160];
    const float tpx = UIS_TF(46.0f);
    depth_fit_text(line, sizeof line, a->title ? a->title : "", tpx, UI_FACE_DISPLAY, max_w);
    y -= UIS_H(60);
    drawTTF_face((u32)x, (u32)y, line, tpx, depth_mix_q(0x00000000, XMB_TEXT, t),
                 UI_FACE_DISPLAY);
    xmb_draw_eyebrow(x, y - UIS_H(22), a->eyebrow ? a->eyebrow : "",
                     depth_mix_q(0x00000000, XMB_ACCENT_ALT, t * 0.9f));
}

// The outgoing slide's text fades out over the first half of the crossfade
// and the incoming one's in over the second, so the two never overlap.
static void ambient_artwork_text(void) {
    float ac, an;
    art_alphas(timing_get_us(), &ac, &an);
    const float art = s_f.art_a;
    if (s_xf_t) {
        art_text_block(s_cur, art * (1.0f - (an * 2.0f > 1.0f ? 1.0f : an * 2.0f)));
        art_text_block(s_nxt, art * (an * 2.0f - 1.0f > 0.0f ? an * 2.0f - 1.0f : 0.0f));
    } else {
        art_text_block(s_cur, art * ac);
    }
}

// Text phase (inside the text-run window).
void ambient_text(void) {
    if (!s_f.draw_ambient) return;
    xmb_draw_topbar();     // the lockup and the clock stay: it is still the XMB
    if (s_art_full) { ambient_artwork_text(); return; }
    AmbPoster p;
    if (!ambient_poster(&p)) return;
    const AmbientArt *a = &s_art[p.idx];
    const float t = p.a;
    const int x = XMB_OX + UIS_W(640);
    const int max_w = (int)display_width - XMB_ITEM_PAD - x;
    if (max_w <= 0 || t < 0.04f) return;
    xmb_draw_eyebrow(x, XMB_OY + UIS_H(282), a->eyebrow ? a->eyebrow : "",
                     depth_mix_q(0x00000000, XMB_ACCENT_ALT, t * 0.9f));
    char line[160];
    const float tpx = UIS_TF(30.0f);
    depth_fit_text(line, sizeof line, a->title ? a->title : "", tpx, UI_FACE_DISPLAY, max_w);
    drawTTF_face((u32)x, (u32)(XMB_OY + UIS_H(306)), line, tpx,
                 depth_mix_q(0x00000000, XMB_TEXT, t), UI_FACE_DISPLAY);
    if (a->meta && a->meta[0])
        drawTTF((u32)x, (u32)(XMB_OY + UIS_H(352)), a->meta, UIS_TF(14.0f),
                depth_mix_q(0x00000000, XMB_TEXT_DIM, t));
}

// After the frame's text flush, while the UI is dissolving in or out: one
// blended quad over everything, FIFO-ordered after the text runs, so no
// fence is needed.
void ambient_cover_over_ui(void) {
    if (!s_f.draw_ui || s_f.cover <= 0.002f) return;
    ui_rect_gpu_draw(0, 0, (int)display_width, (int)display_height,
                     0x00000000, a8(s_f.cover));
    ui_card_gpu_end();
}

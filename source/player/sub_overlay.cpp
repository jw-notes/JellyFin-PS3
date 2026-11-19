// Subtitle overlay -- see sub_overlay.h.

#include "sub_overlay.h"

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <ppu-types.h>
#include <rsx/rsx.h>

#include "player_rsx.h"  // rsx_draw_overlay_quad
#include "rsxutil.h"     // display_width / display_height
#include "ui.h"          // cpu_rt_begin/end, drawBitmapRectAlpha
#include "ui_visuals.h"  // drawTTF_face, ttf_text_width_face, ttf_need_text
#include "subfont.h"
#include "subcolor.h"
#include "rtl_text.h"    // right-to-left subtitle lines
#include "plog.h"
#include "meminfo.h"

// Longest single rendered line.  Cues are capped well below this in
// subtitles.cpp; this is only the on-stack row buffer.
#define SUB_ROW_MAX 208

// The band is the screen's width and a quarter of its height (at least four
// lines with their outline): room for ordinary dialogue, text or PGS.
// Anything taller is cut at the band's bottom edge.
#define BAND_H_DEN 4

static u32 *s_stage   = NULL;   // main-RAM compose target (straight alpha)
static u32 *s_tex     = NULL;   // RSX-local texture the GPU samples
static u32  s_tex_off = 0;
static u32  s_bw = 0, s_bh = 0;

// Rows of the band the last compose drew, so the next one wipes only those.
static int  s_used_y0 = 0, s_used_y1 = 0;
// Where the band sits on screen.
static int  s_band_y = 0;

// Everything the band's pixels depend on.  Zeroed before filling so padding
// compares clean.
struct SubKey {
    u32 kind;                // 0 none, 1 text, 2 pgs
    u32 hash;                // text: FNV-1a of the cue; pgs: the generation
    u32 fill, outline;
    s32 ow, face;
    s32 x, y, w, h;          // pgs placement
};
static SubKey s_key;
static bool   s_key_valid = false;

void sub_overlay_alloc(void) {
    if (s_stage && s_tex) return;
    // Pitch is width * 4 and RSX linear textures need it 64-byte aligned:
    // the width must be a multiple of 16.
    s_bw = (display_width + 15u) & ~15u;
    s_bh = display_height / BAND_H_DEN;
    const u32 four_lines = (display_height >= 720 ? 40u : 30u) * 4u + 16u;
    if (s_bh < four_lines) s_bh = four_lines;
    s_bh = (s_bh + 1u) & ~1u;
    const u32 bytes = s_bw * s_bh * 4;
    if (!s_stage) s_stage = (u32*)memalign(128, bytes);
    if (!s_tex) {
        s_tex = (u32*)rsxMemalign(64, bytes);
        if (s_tex) rsxAddressToOffset(s_tex, &s_tex_off);
    }
    char b[96];
    if (!s_stage || !s_tex) {
        u32 total = 0, avail = 0;
        meminfo_get(&total, &avail);
        snprintf(b, sizeof b, "sub_ovl: alloc FAILED (%u KB; stage=%d tex=%d, free=%uKB)",
                 (unsigned)(bytes / 1024), s_stage != NULL, s_tex != NULL, (unsigned)(avail / 1024));
        plog(b);
        return;
    }
    memset(s_stage, 0, bytes);
    memset((void*)s_tex, 0, bytes);
    s_used_y0 = s_used_y1 = 0;
    s_key_valid = false;
    snprintf(b, sizeof b, "sub_ovl: buffers ready %ux%u", (unsigned)s_bw, (unsigned)s_bh);
    plog(b);
}

// Only sub_overlay_alloc() allocates, at the start of a playback: once the
// decoder and the read-ahead ring are in, main memory has no room left.
static bool ensure_buffers(void) {
    return s_stage && s_tex;
}

// Wipe what the last compose drew, run `draw` into the band, record the rows
// it can have touched, and upload the union of old and new rows.
static void compose_begin(void) {
    if (s_used_y1 > s_used_y0)
        memset(s_stage + (u32)s_used_y0 * s_bw, 0,
               (u32)(s_used_y1 - s_used_y0) * s_bw * 4);
    cpu_rt_begin(s_stage, s_bw, s_bh);
}

static void compose_end(int y0, int y1) {
    cpu_rt_end();
    if (y0 < 0) y0 = 0;
    if (y1 > (int)s_bh) y1 = (int)s_bh;
    if (y1 < y0) y1 = y0;
    int up0 = s_used_y0, up1 = s_used_y1;
    if (up1 <= up0) { up0 = y0; up1 = y1; }
    else {
        if (y1 > y0 && y0 < up0) up0 = y0;
        if (y1 > y0 && y1 > up1) up1 = y1;
    }
    if (up1 > up0)
        memcpy((void*)(s_tex + (u32)up0 * s_bw), s_stage + (u32)up0 * s_bw,
               (u32)(up1 - up0) * s_bw * 4);
    s_used_y0 = y0;
    s_used_y1 = y1;
}

static void draw_band(void) {
    if (s_used_y1 > s_used_y0)
        rsx_draw_overlay_quad(s_tex_off, s_bw, s_bh, 0, s_band_y, (int)s_bw, (int)s_bh);
}

static void band_place(int y) {
    const int max_y = (int)display_height - (int)s_bh;
    if (y > max_y) y = max_y;
    if (y < 0) y = 0;
    s_band_y = y;
}

void sub_overlay_none(void) {
    // The band's texture keeps its last words; drawing nothing is enough.
    // The key is dropped so the same cue coming back is composed again.
    s_key_valid = false;
    s_key.kind  = 0;
}

void sub_overlay_text(const char *line) {
    if (!line || !*line) { sub_overlay_none(); return; }
    if (!ensure_buffers()) return;

    u32 h = 2166136261u;
    for (const char *q = line; *q; q++) h = (h ^ (u8)*q) * 16777619u;

    // A line in a script no loaded face covers (Korean, Chinese, Hebrew,
    // Arabic) reads that face in, once per new line.
    if (!s_key_valid || s_key.kind != 1 || s_key.hash != h) ttf_need_text(line);

    // Fill, outline and outline weight all come from the chosen look; they
    // move together (see subcolor.h).
    SubKey key;
    memset(&key, 0, sizeof key);
    key.kind    = 1;
    key.hash    = h;
    key.fill    = subcolor_fill();
    key.outline = subcolor_outline();
    key.ow      = subcolor_outline_px(display_height);
    key.face    = subfont_face();

    if (!s_key_valid || memcmp(&key, &s_key, sizeof key) != 0) {
        s_key = key;
        s_key_valid = true;

        // Bottom-centred, one line above the other, inside the title-safe
        // area so an overscanning CRT or plasma does not clip the text.
        //
        // Open Sans bold: the faces people associate with subtitles (Arial,
        // Helvetica, Tiresias) are proprietary and cannot ship in a GPLv3
        // package; a humanist sans with a large x-height at semibold or
        // bolder is what reads at a distance.
        const int W  = (int)display_width;
        const int px = (display_height >= 720) ? 32 : 22;
        const int lh = px + 8;
        const int ow = key.ow;
        int nlines = 1;
        for (const char *q = line; *q; q++) if (*q == '\n') nlines++;
        const int y_screen = (int)display_height - (int)(display_height / 12) - nlines * lh;
        band_place(y_screen - ow - 2);
        int y = y_screen - s_band_y;

        compose_begin();
        const int top = y - ow;
        const char *p2 = line;
        char row[SUB_ROW_MAX];
        while (*p2) {
            const char *nl = strchr(p2, '\n');
            int len = nl ? (int)(nl - p2) : (int)strlen(p2);
            if (len > SUB_ROW_MAX - 1) len = SUB_ROW_MAX - 1;
            memcpy(row, p2, len); row[len] = '\0';

            // Hebrew, Arabic, Persian: shaped and put in the order it is drawn.
            char vis[SUB_ROW_MAX * 2];
            const char *draw = rtl_visual_line(row, vis, (int)sizeof vis) ? vis : row;

            const int tw = ttf_text_width_face(draw, (float)px, key.face);
            int x = (W - tw) / 2;
            if (x < ow) x = ow;
            // Outline in every direction: white on a bright scene is
            // unreadable without one.  The glyph cache rasterises each glyph
            // once; the repeats are blits.
            for (int dy = -ow; dy <= ow; dy++)
                for (int dx = -ow; dx <= ow; dx++)
                    if ((dx || dy) && y + dy >= 0)
                        drawTTF_face((u32)(x + dx), (u32)(y + dy), draw,
                                     (float)px, key.outline, key.face);
            if (y >= 0)
                drawTTF_face((u32)x, (u32)y, draw, (float)px, key.fill, key.face);

            y += lh;
            if (!nl) break;
            p2 = nl + 1;
        }
        // Glyphs can reach below their line box (descenders); a full extra
        // line of margin is cheap and certain.
        compose_end(top, y + lh + ow);
    }
    draw_band();
}

void sub_overlay_pgs(const PgsBitmap *bmp, u32 gen) {
    if (!bmp || !bmp->rgba || bmp->frame_w <= 0 || bmp->frame_h <= 0 ||
        bmp->width <= 0 || bmp->height <= 0) { sub_overlay_none(); return; }
    if (!ensure_buffers()) return;

    // Position scales from the PG stream's own authored space (typically
    // 1920x1080) into the display; SIZE does not -- the bitmap is drawn at
    // its decoded pixel size, so an authored-1080p bitmap is oversized at a
    // lower output resolution.
    const int dx = (int)((s64)bmp->x * (s64)display_width  / bmp->frame_w);
    const int dy = (int)((s64)bmp->y * (s64)display_height / bmp->frame_h);

    SubKey key;
    memset(&key, 0, sizeof key);
    key.kind = 2;
    key.hash = gen;
    key.x = dx; key.y = dy; key.w = bmp->width; key.h = bmp->height;

    if (!s_key_valid || memcmp(&key, &s_key, sizeof key) != 0) {
        s_key = key;
        s_key_valid = true;
        band_place(dy);
        const int ly = dy - s_band_y;
        compose_begin();
        if (dx >= 0 && ly >= 0)
            drawBitmapRectAlpha(bmp->rgba, (u32)bmp->width,
                                0, 0, (u32)bmp->width, (u32)bmp->height,
                                (u32)dx, (u32)ly);
        compose_end(ly, ly + bmp->height);
    }
    draw_band();
}

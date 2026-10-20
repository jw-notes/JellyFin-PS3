// Search tab rendering — search field, OSK (CPU key cells + RSX labels)
// and the results list below it.

#include "i18n.h"
#include <stdio.h>
#include <string.h>

#include "ui_render_internal.h"
#include "thumbnail_cache.h"
#include "timing.h"

#define OSK_FIELD_H UIS_H(40)

// -------------------------------------------------------
// Search layout
// -------------------------------------------------------
// The keyboard is tall enough that on anything below 1080p there was no room
// left under it for results: vis_rows came out 0, the draw loop never ran a
// single iteration, and the list was invisible — while the input handler
// happily kept moving the selection through items nobody could see (the
// "results not visible underneath the keyboard" report).  Measured before the
// fix: 0 rows at 480 NTSC, and 0 at 720p as soon as any overscan was set.
//
// So the keyboard COLLAPSES once focus moves into the results: while typing
// it is drawn in full with however many rows fit beneath it, and the moment
// the user presses down into the list it folds away to just the search field,
// handing the whole content area to the results (3+ rows at every mode down
// to 480i).  Up from the first row, or circle, brings it straight back.

// The header line above the list: the filter chips, or "Recent searches".
#define SR_HEAD_H UIS_H(38)

static bool sr_has_head(void) {
    return xmb_search_has_filters() || xmb_search_showing_recent();
}

static int sr_list_top(void) {
    if (g_search_focus_results)
        return XMB_CONTENT_Y + 8 + OSK_FIELD_H + 16;
    return OSK_Y0 + (OSK_ROWS_N + 1) * OSK_STEP_Y + 20;
}

int xmb_search_results_y(void) {
    return sr_list_top() + (sr_has_head() ? SR_HEAD_H : 0);
}

// The filter chips: "All 24", "Movies 9", ... left-aligned over the list.
// Chips with nothing in them are drawn faint (L2 / R2 skip them).
#define SR_CHIP_PX   UIS_TF(15.0f)
#define SR_CHIP_H    UIS_H(28)
#define SR_CHIP_PAD  UIS_W(14)
#define SR_CHIP_GAP  UIS_W(10)

static const char *sr_chip_name(int f) {
    switch (f) {
    case 1:  return TR("Movies");
    case 2:  return TR("Shows");
    case 3:  return TR("Episodes");
    default: return TR("All");
    }
}

static void sr_chip_label(int f, char *b, int cap) {
    snprintf(b, (size_t)cap, "%s  %d", sr_chip_name(f), g_search_filter_n[f]);
}

// Walks the chips, calling `each` with each one's box; CPU and text phases
// share it so the two cannot disagree on where a chip is.
typedef void (*sr_chip_fn)(int f, int x, int y, int w, const char *label);
static void sr_chips_walk(sr_chip_fn each) {
    int x = ((int)display_width - XMB_LIST_W) / 2;
    const int y = sr_list_top() + (SR_HEAD_H - SR_CHIP_H) / 2 - UIS_H(4);
    for (int f = 0; f < 4; f++) {
        char label[48];
        sr_chip_label(f, label, sizeof label);
        const int w = ttf_text_width(label, SR_CHIP_PX) + 2 * SR_CHIP_PAD;
        each(f, x, y, w, label);
        x += w + SR_CHIP_GAP;
    }
}

static void sr_chip_cpu(int f, int x, int y, int w, const char *label) {
    (void)label;
    const bool on = f == g_search_filter;
    drawRect((u32)x, (u32)y, (u32)w, (u32)SR_CHIP_H, on ? XMB_ACCENT : XMB_PANEL);
    if (!on) {
        drawRect((u32)x, (u32)y, (u32)w, 1, XMB_HAIRLINE);
        drawRect((u32)x, (u32)(y + SR_CHIP_H - 1), (u32)w, 1, XMB_HAIRLINE);
    }
}

static void sr_chip_text(int f, int x, int y, int w, const char *label) {
    (void)w;
    const bool on = f == g_search_filter;
    const u32 col = on ? XMB_KEY_LABEL_SEL
                  : g_search_filter_n[f] ? XMB_TEXT : XMB_TEXT_FAINT;
    drawTTF((u32)(x + SR_CHIP_PAD), (u32)(y + (SR_CHIP_H - (int)SR_CHIP_PX) / 2 - UIS_H(2)),
            label, SR_CHIP_PX, col, on);
}

// Rows that actually fit.  The single source of truth for both the renderer
// and the scroll arithmetic — they used to disagree, the handler assuming a
// fixed 6 while the renderer measured the screen.
int xmb_search_vis_rows(void) {
    int r = ((int)display_height - XMB_BOTTOM_PAD - xmb_search_results_y())
            / XMB_ROW_STRIDE;
    return r < 0 ? 0 : r;
}

void xmb_cpu_draw_osk(void) {
    int W = (int)display_width;
    int total_w = 10 * OSK_STEP_X - OSK_GAP;
    int osk_x0  = (W - total_w) / 2;

    // Search field: dark well with an accent underline.
    drawRect((u32)osk_x0, (u32)(XMB_CONTENT_Y + UIS_H(8)),
             (u32)total_w, OSK_FIELD_H, XMB_TRACK);
    drawRect((u32)osk_x0, (u32)(XMB_CONTENT_Y + UIS_H(8) + OSK_FIELD_H - UIS_H(2)),
             (u32)total_w, UIS_H(2), XMB_ACCENT);

    if (g_search_focus_results) return;   // collapsed: field only, no keys

    for (int r = 0; r <= OSK_ROWS_N; r++) {
        if (r == OSK_ROWS_N) {
            int space_w = 5 * OSK_STEP_X - OSK_GAP;
            int sy = OSK_Y0 + r * OSK_STEP_Y;
            bool sp_sel = (r == g_osk_row && g_osk_col == 0);
            drawRect((u32)osk_x0, (u32)sy, (u32)space_w, OSK_KEY_H,
                     sp_sel ? XMB_KEY_SEL : XMB_KEY_NORMAL);
            int bsx = osk_x0 + space_w + OSK_GAP;
            bool bs_sel = (r == g_osk_row && g_osk_col == 1);
            drawRect((u32)bsx, (u32)sy, OSK_KEY_W, OSK_KEY_H,
                     bs_sel ? XMB_KEY_SEL : XMB_KEY_NORMAL);
            int clx = bsx + OSK_KEY_W + OSK_GAP;
            bool cl_sel = (r == g_osk_row && g_osk_col == 2);
            drawRect((u32)clx, (u32)sy, OSK_KEY_W, OSK_KEY_H,
                     cl_sel ? XMB_KEY_SEL : XMB_KEY_NORMAL);
        } else {
            const char **rows = g_osk_sym ? OSK_SYMBOLS : OSK_LETTERS;
            int rlen = (int)strlen(rows[r]);
            if (r == OSK_ROWS_N - 1) rlen++;
            int ry = OSK_Y0 + r * OSK_STEP_Y;
            for (int c = 0; c < rlen; c++) {
                bool sel = (r == g_osk_row && c == g_osk_col);
                drawRect((u32)(osk_x0 + c * OSK_STEP_X), (u32)ry,
                         OSK_KEY_W, OSK_KEY_H,
                         sel ? XMB_KEY_SEL : XMB_KEY_NORMAL);
            }
        }
    }
}

// One key label, centered in a key cell, dark when the key is selected
// (selected keys are white).
static void osk_key_label(int kx, int kw, int ry, const char *lbl, bool sel) {
    const float px = UIS_TF(20.0f);
    int lw = ttf_text_width(lbl, px);
    int ty = ry + (OSK_KEY_H - (int)px) / 2 - UIS_H(2);
    drawTTF((u32)(kx + (kw - lw) / 2), (u32)(ty > 0 ? ty : 0), lbl, px,
            sel ? XMB_KEY_LABEL_SEL : XMB_TEXT);
}

void xmb_rsx_draw_osk(void) {
    int W = (int)display_width;
    int total_w = 10 * OSK_STEP_X - OSK_GAP;
    int osk_x0  = (W - total_w) / 2;

    u64 us = timing_get_us();
    bool cursor = ((us / 500000) & 1) == 0;

    // Search field content: magnifier glyph, then typed text or ghost prompt.
    {
        int fx = osk_x0 + UIS_W(14);
        int fy = XMB_CONTENT_Y + UIS_H(8);
        drawIcon((u32)fx, (u32)(fy + (OSK_FIELD_H - UIS_H(20)) / 2), ICON_SEARCH, UIS_TF(20.0f),
                 XMB_TEXT_FAINT);
        int tx = fx + UIS_W(30);
        int ty = fy + (OSK_FIELD_H - UIS_H(18)) / 2 - UIS_H(2);
        if (g_search_buf[0]) {
            char disp[68];
            snprintf(disp, sizeof(disp), "%s%s", g_search_buf, cursor ? "_" : "");
            drawTTF((u32)tx, (u32)ty, disp, UIS_TF(18), XMB_TEXT);
        } else {
            if (cursor) drawTTF((u32)tx, (u32)ty, "_", UIS_TF(18), XMB_TEXT);
            drawTTF((u32)(tx + UIS_W(14)), (u32)ty, TR("Search your library"), UIS_TF(17),
                    XMB_TEXT_FAINT);
        }
    }

    // -1 skips the key rows entirely while the results are focused; the
    // search field above and the results below still draw.
    const int key_rows = g_search_focus_results ? -1 : OSK_ROWS_N;
    for (int r = 0; r <= key_rows; r++) {
        int ry = OSK_Y0 + r * OSK_STEP_Y;
        if (r == OSK_ROWS_N) {
            int space_w = 5 * OSK_STEP_X - OSK_GAP;
            int bsx = osk_x0 + space_w + OSK_GAP;
            int clx = bsx + OSK_KEY_W + OSK_GAP;
            osk_key_label(osk_x0, space_w, ry, "Space",
                          r == g_osk_row && g_osk_col == 0);
            {   // Backspace: Material icon, centered in the key.
                bool sel = (r == g_osk_row && g_osk_col == 1);
                drawIcon((u32)(bsx + (OSK_KEY_W - UIS_W(22)) / 2),
                         (u32)(ry + (OSK_KEY_H - UIS_H(22)) / 2), ICON_BACKSPACE, UIS_TF(22.0f),
                         sel ? XMB_KEY_LABEL_SEL : XMB_TEXT);
            }
            osk_key_label(clx, OSK_KEY_W, ry, "Clear",
                          r == g_osk_row && g_osk_col == 2);
        } else {
            const char **rows = g_osk_sym ? OSK_SYMBOLS : OSK_LETTERS;
            const char  *row  = rows[r];
            int base_len = strlen(row);

            for (int c = 0; c < base_len; c++) {
                char label[2] = { row[c], '\0' };
                osk_key_label(osk_x0 + c * OSK_STEP_X, OSK_KEY_W, ry, label,
                              r == g_osk_row && c == g_osk_col);
            }
            if (r == OSK_ROWS_N - 1) {
                osk_key_label(osk_x0 + base_len * OSK_STEP_X, OSK_KEY_W, ry,
                              g_osk_sym ? "ABC" : "#+=",
                              r == g_osk_row && g_osk_col == base_len);
            }
        }
    }

    int results_y = xmb_search_results_y();
    int count     = g_search_results_count;
    int vis_r     = xmb_search_vis_rows();
    if (xmb_search_has_filters()) {
        sr_chips_walk(sr_chip_text);
    } else if (xmb_search_showing_recent()) {
        const int lx = ((int)display_width - XMB_LIST_W) / 2;
        const int hy = sr_list_top() + UIS_H(6);
        drawTTF((u32)lx, (u32)hy, TR("Recent searches"), UIS_TF(15.0f), XMB_TEXT_DIM, true);
        const int n = xmb_search_recent_count();
        for (int i = 0; i < vis_r; i++) {
            const int idx = g_search_scroll + i;
            if (idx >= n) break;
            const int iy = results_y + i * XMB_ROW_STRIDE;
            const bool sel = g_search_focus_results && idx == g_search_sel;
            drawIcon((u32)(lx + UIS_W(16)), (u32)(iy + (XMB_ROW_H - UIS_H(20)) / 2), ICON_SEARCH,
                     UIS_TF(20.0f), sel ? XMB_ACCENT : XMB_TEXT_FAINT);
            drawTTF((u32)(lx + UIS_W(52)), (u32)(iy + (XMB_ROW_H - UIS_H(19)) / 2 - UIS_H(2)),
                    xmb_search_recent_term(idx), UIS_TF(19), sel ? XMB_WHITE : XMB_TEXT, sel);
        }
    }
    {
        int sr_list_x = ((int)display_width - XMB_LIST_W) / 2;
        int sr_tx     = sr_list_x + UIS_W(16) + XMB_THUMB_W + UIS_W(16);
        for (int i = 0; i < vis_r; i++) {
            int idx = g_search_scroll + i;
            if (idx >= count) break;
            const XMBItem *it = &g_search_results[idx];
            int iy = results_y + i * XMB_ROW_STRIDE;
            bool sel = g_search_focus_results && idx == g_search_sel;
            drawTTF((u32)sr_tx, (u32)(iy + UIS_H(18)), it->name, UIS_TF(19),
                    sel ? XMB_WHITE : XMB_TEXT, sel);
            xmb_draw_meta((u32)sr_tx, (u32)(iy + UIS_H(46)), it, UIS_TF(14));
        }
    }
    if (count == 0 && g_search_buf[0] && xmb_search_in_flight()) {
        // The query is out on its worker thread.  This server takes 2.5-8.4 s
        // to answer a search (HANDOFF.md), and saying "No results" for that
        // whole window is indistinguishable from the search being broken --
        // which is exactly what it looked like before.
        char msg[96];
        snprintf(msg, sizeof(msg), TR("Searching for \"%s\"..."), g_search_buf);
        int mw = ttf_text_width(msg, UIS_TF(16));
        drawTTF((u32)(((int)display_width - mw) / 2), (u32)(results_y + UIS_H(10)),
                msg, UIS_TF(16), XMB_TEXT_DIM);
    } else if (count == 0 && g_search_buf[0]) {
        char msg[96];
        snprintf(msg, sizeof(msg), TR("No results for \"%s\""), g_search_buf);
        int mw = ttf_text_width(msg, UIS_TF(16));
        drawTTF((u32)(((int)display_width - mw) / 2), (u32)(results_y + UIS_H(10)),
                msg, UIS_TF(16), XMB_TEXT_FAINT);
    } else if (count > 0 && vis_r == 0) {
        // Hits exist but the keyboard leaves no room to list them (every mode
        // below 1080p).  Without this the screen looks identical to "no
        // results" and there is nothing to suggest pressing Down — which is
        // exactly how the invisible list went unnoticed.  One line fits in
        // the ~60px under the keyboard even at 480i.
        char msg[64];
        snprintf(msg, sizeof(msg), "%d result%s \xC2\xB7 press Down to browse",
                 count, count == 1 ? "" : "s");
        int mw = ttf_text_width(msg, UIS_TF(15));
        drawTTF((u32)(((int)display_width - mw) / 2), (u32)(results_y + UIS_H(6)),
                msg, UIS_TF(15), XMB_ACCENT);
    }
}

// CPU draws for search results list (selection highlight + scaled thumbs).
void xmb_cpu_draw_search_results(void) {
    int results_y = xmb_search_results_y();
    int count     = g_search_results_count;
    int vis_r     = xmb_search_vis_rows();
    int list_x = ((int)display_width - XMB_LIST_W) / 2;
    if (xmb_search_has_filters()) sr_chips_walk(sr_chip_cpu);
    if (xmb_search_showing_recent() && g_search_focus_results) {
        const int i = g_search_sel - g_search_scroll;
        if (i >= 0 && i < vis_r) {
            const int iy = results_y + i * XMB_ROW_STRIDE;
            drawRect((u32)list_x, (u32)iy, (u32)XMB_LIST_W, (u32)XMB_ROW_H, XMB_PANEL_HI);
            drawRect((u32)(list_x - UIS_W(4)), (u32)iy, UIS_W(3), (u32)XMB_ROW_H, XMB_ACCENT);
        }
        return;
    }
    for (int i = 0; i < vis_r; i++) {
        int idx = g_search_scroll + i;
        if (idx >= count) break;
        int iy = results_y + i * XMB_ROW_STRIDE;
        if (g_search_focus_results && idx == g_search_sel) {
            drawRect((u32)list_x, (u32)iy,
                     (u32)XMB_LIST_W, (u32)XMB_ROW_H, XMB_PANEL_HI);
            drawRect((u32)(list_x - UIS_W(4)), (u32)iy,
                     UIS_W(3), (u32)XMB_ROW_H, XMB_ACCENT);
        }
        xmb_cpu_blit_thumb_scaled(g_search_results[idx].id,
                                  list_x + UIS_W(16),
                                  iy + (XMB_ROW_H - XMB_THUMB_H) / 2,
                                  XMB_THUMB_W, XMB_THUMB_H);
    }
}

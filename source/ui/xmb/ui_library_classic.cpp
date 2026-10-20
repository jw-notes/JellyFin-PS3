// Classic library pages share Home's header, with a full-width card grid.
// Item actions, pagination, A-Z and series drill-down retain their existing model.
#include <stdio.h>
#include <string.h>
#include "ui_internal.h"
#include "ui_spine.h"
#include "ui_sfx.h"
#include "classic_grid.h"

static int s_header = -1; // -1 grid, otherwise Home / Search / Settings

bool xmb_classic_library(int tab) {
    if (ui_layout() != UI_LAYOUT_CLASSIC || tab < XMB_TAB_LIB0 || tab >= XMB_TAB_COUNT) return false;
    const XMBTabKind kind = xmb_kind(tab);
    return kind == TABKIND_MOVIES || kind == TABKIND_TV ||
           kind == TABKIND_BOXSETS || kind == TABKIND_GENERIC ||
           kind == TABKIND_MUSIC || kind == TABKIND_PLAYLISTS;
}

bool xmb_classic_media(int tab) {
    return ui_layout() == UI_LAYOUT_CLASSIC && tab >= XMB_TAB_LIB0 &&
           tab < XMB_TAB_COUNT && xmb_kind(tab) == TABKIND_LOCAL;
}

void xmb_classic_library_enter(int tab) {
    s_header = -1;
    if (xmb_classic_media(tab)) { spine_set_level(SPINE_L2); return; }
    if (tab < XMB_TAB_LIB0 || tab >= XMB_TAB_COUNT) return;
    const XMBTabKind kind = xmb_kind(tab);
    if (kind != TABKIND_MOVIES && kind != TABKIND_TV &&
        kind != TABKIND_BOXSETS && kind != TABKIND_GENERIC &&
        kind != TABKIND_MUSIC && kind != TABKIND_PLAYLISTS) return;
    if (xmb_classic_library(tab)) spine_set_level(SPINE_L2);
    // The remembered focus can come from the other layout's column count.
    // Align its window after spine_focus_enter restores it, in either mode.
    GridGeom gg;
    xmb_grid_geom(tab, &gg);
    if (g_sel < 0) g_sel = 0;
    int row = g_scroll_top > 0 ? g_scroll_top / gg.cols : 0;
    const int focus_row = g_sel / gg.cols;
    if (focus_row < row) row = focus_row;
    const int rows = gg.vis / gg.cols;
    if (focus_row >= row + rows) row = focus_row - rows + 1;
    g_scroll_top = row * gg.cols;
}

int xmb_classic_library_header_focus(int tab) {
    return (xmb_classic_library(tab) || xmb_classic_media(tab)) ? s_header : -1;
}

int xmb_classic_library_y0(int tab) {
    return XMB_OY + UIS_H(xmb_kind(tab) == TABKIND_MUSIC ? 180 : 156);
}

bool xmb_classic_grid_geom(int tab, GridGeom *gg) {
    if (!xmb_classic_library(tab)) return false;
    const bool music = g_music_depth > 0 || xmb_kind(tab) == TABKIND_MUSIC || xmb_kind(tab) == TABKIND_PLAYLISTS;
    const bool portrait = !music && g_tv_depth != 2 && xmb_tab_uses_portrait(tab);
    const ClassicGrid g = classic_grid_make((int)display_width, (int)display_height,
        XMB_OX + UIS_W(64), xmb_classic_library_y0(tab), XMB_BOTTOM_PAD,
        music ? XMB_MUSIC_TEXT_H : XMB_CARD_TEXT_H, XMB_CARD_GAP_X, UIS_H(10), portrait, music);
    gg->portrait = portrait;
    gg->cols = g.cols; gg->vis = g.cols * g.rows;
    gg->card_w = g.card_w; gg->card_h = g.card_h;
    gg->stride = g.stride; gg->grid_w = g.grid_w; gg->x0 = g.x0;
    return true;
}

void xmb_classic_library_heading(int tab) {
    GridGeom gg; const XMBItem *items;
    int count, sel, scroll, y0, start, total; bool more;
    if (!xmb_grid_view(tab, &gg, &items, &count, &sel, &scroll, &y0, &more, &start, &total)) return;
    const int x = gg.x0, y = XMB_OY + UIS_H(86);
    // Keep the library name visible while opening a show's seasons/episodes.
    drawTTF_clipped((u32)x, (u32)y, g_tabs[tab].label, UIS_TF(24), XMB_TEXT,
                    (int)display_width - 2 * x - UIS_W(210), UI_FACE_BOLD);
    if (g_music_depth > 0)
        xmb_draw_breadcrumb(x, XMB_OY + UIS_H(122), g_music_parent_name,
            g_music_sub_count > 0 && strcmp(g_music_sub_items[0].type, "Audio") == 0 ? TR("Songs") : TR("Albums"), NULL);
    else if (xmb_kind(tab) == TABKIND_MUSIC)
        xmb_draw_music_subtabs(x, XMB_OY + UIS_H(122), g_music_subtab, g_music_header && s_header < 0);
    else if (g_tv_depth == 1)
        xmb_draw_breadcrumb(x, XMB_OY + UIS_H(122), g_tv_series_name, TR("Seasons"), NULL);
    else if (g_tv_depth > 1)
        xmb_draw_breadcrumb(x, XMB_OY + UIS_H(122), g_tv_series_name, g_tv_season_name, TR("Episodes"));
    else if (g_col_depth > 0)
        xmb_draw_breadcrumb(x, XMB_OY + UIS_H(122), g_col_name, TR("Movies"), NULL);
    else if (g_tab_name_filter[tab][0])
        drawTTF((u32)x, (u32)(XMB_OY + UIS_H(122)), g_tab_name_filter[tab], UIS_TF(15), XMB_TEXT_DIM);
    if (g_tv_depth > 0) xmb_season_toast_draw();
    if (count > 0) {
        if (total < start + count) total = start + count;
        int last = start + scroll + gg.vis;
        if (last > start + count) last = start + count;
        char position[64];
        snprintf(position, sizeof position, "%d - %d / %d", start + scroll + 1, last, total);
        const float px = UIS_TF(14);
        int tw = ttf_text_width(position, px);
        drawTTF((u32)(display_width - gg.x0 - tw), (u32)(y + UIS_H(9)), position, px, XMB_TEXT_DIM);
    }
}

void xmb_classic_library_hints(void) {
    if (s_header >= 0) {
        static const Hint h[] = {{'E', TRN("Nav")}, {'X', TRN("Select")}, {'C', TRN("Back")}};
        draw_hints_bar(h, 3);
    } else {
        static const Hint h[] = {{'E', TRN("Nav")}, {'X', TRN("Open")},
                                 {'T', TRN("Details")}, {'C', TRN("Back")}};
        draw_hints_bar(h, 4);
    }
}

bool xmb_classic_library_input(int tab) {
    if (s_header >= 0) {
        if (BTN_REPEAT(left) && s_header > 0) { --s_header; ui_sfx_play(SFX_CURSOR); }
        if (BTN_REPEAT(right) && s_header < 2) { ++s_header; ui_sfx_play(SFX_CURSOR); }
        if (BTN_PRESSED(down) || BTN_PRESSED(circle)) { s_header = -1; init_btns(); }
        else if (BTN_PRESSED(cross)) {
            const int to = s_header == 0 ? XMB_TAB_HOME : s_header == 1 ? XMB_TAB_SEARCH : XMB_TAB_SETTINGS;
            xmb_switch_tab(to);
            if (to != XMB_TAB_HOME) spine_set_level(SPINE_L2);
            init_btns();
        }
        return false;
    }
    if (!g_jumpbar_active) {
        const bool sub = g_tv_depth > 0 || g_col_depth > 0 || g_music_depth > 0;
        if (!sub && BTN_PRESSED(circle)) {
            xmb_switch_tab(XMB_TAB_HOME);
            init_btns();
            return false;
        }
        if (xmb_classic_media(tab)) {
            if (BTN_PRESSED(up) && xmb_media_at_top()) { s_header = 0; ui_sfx_play(SFX_CURSOR); return false; }
            return xmb_input_media();
        }
        const bool music_root = xmb_kind(tab) == TABKIND_MUSIC && g_music_depth == 0;
        if (music_root && g_music_header) {
            if (BTN_PRESSED(up)) { s_header = 0; ui_sfx_play(SFX_CURSOR); return false; }
            return xmb_handle_input_browse();
        }
        GridGeom gg;
        xmb_grid_geom(tab, &gg);
        const int sel = g_tv_depth > 0 ? g_tv_sub_sel : g_col_depth > 0 ? g_col_sub_sel : g_music_depth > 0 ? g_music_sub_sel : g_sel;
        const int start = g_tv_depth > 0 ? g_tv_sub_start : g_col_depth > 0 ? g_col_sub_start : g_music_depth > 0 ? 0 : g_tab_start[tab];
        if (!music_root && BTN_PRESSED(up) && sel < gg.cols && start == 0) {
            s_header = 0;
            ui_sfx_play(SFX_CURSOR);
            return false;
        }
    }
    return xmb_handle_input_browse();
}

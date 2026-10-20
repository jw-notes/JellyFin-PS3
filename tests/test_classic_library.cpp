// Real Classic library geometry/input, with only platform and browse I/O stubbed.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui_internal.h"
#include "ui_spine.h"
#include "ui_sfx.h"

u32 display_width = 1280, display_height = 720;
int g_uis_pct = 0;
static float inset;
float overscan_frac() { return inset; }
int overscan_x() { return (int)(display_width * inset); }
int overscan_y() { return (int)(display_height * inset); }
XMBTab g_tabs[XMB_TAB_COUNT];
ButtonState btn_cur, btn_prev;
int g_active_tab = XMB_TAB_LIB0, g_sel, g_scroll_top, g_tab_start[XMB_TAB_COUNT];
int g_tv_depth, g_tv_sub_sel, g_tv_sub_start;
int g_col_depth, g_col_sub_sel, g_col_sub_start;
bool g_jumpbar_active;
int g_music_depth, g_music_sub_sel;
bool g_music_header;
static bool media_top = true;
bool xmb_media_at_top() { return media_top; }

static int layout = UI_LAYOUT_CLASSIC, delegated, switched = -1, level;
bool xmb_input_media() { ++delegated; return false; }
int ui_layout() { return layout; }
XMBTabKind xmb_kind(int tab) { return g_tabs[tab].kind; }
bool btn_nav_repeat(bool held, int) { return held; }
void init_btns() { memset(&btn_cur, 0, sizeof btn_cur); memset(&btn_prev, 0, sizeof btn_prev); }
void ui_sfx_play(int) {}
void spine_set_level(int value) { level = value; }
void xmb_switch_tab(int tab) { switched = g_active_tab = tab; xmb_classic_library_enter(tab); }
bool xmb_handle_input_browse() { ++delegated; return false; }
bool xmb_tab_uses_portrait(int tab) { return !(xmb_kind(tab) == TABKIND_TV && g_tv_depth == 2); }
void xmb_grid_geom(int tab, GridGeom *gg) {
    if (!xmb_classic_grid_geom(tab, gg)) { gg->cols = 5; gg->vis = 10; }
}

static void reset() {
    init_btns(); g_active_tab = XMB_TAB_LIB0;
    g_tv_depth = g_col_depth = g_music_depth = 0; g_music_header = false; g_sel = g_scroll_top = 0;
    g_tab_start[g_active_tab] = 0; g_jumpbar_active = false;
    switched = -1; delegated = 0;
    xmb_classic_library_enter(g_active_tab);
}

int main() {
    g_tabs[XMB_TAB_LIB0].kind = TABKIND_MOVIES;
    assert(xmb_classic_library(XMB_TAB_LIB0));
    assert(!xmb_classic_library(XMB_TAB_HOME));
    layout = UI_LAYOUT_XMB;
    assert(!xmb_classic_library(XMB_TAB_LIB0));
    layout = UI_LAYOUT_CLASSIC;
    const XMBTabKind supported[] = {TABKIND_TV, TABKIND_BOXSETS, TABKIND_GENERIC, TABKIND_MUSIC, TABKIND_PLAYLISTS};
    for (unsigned i = 0; i < sizeof supported / sizeof supported[0]; ++i) {
        g_tabs[XMB_TAB_LIB0].kind = supported[i];
        assert(xmb_classic_library(XMB_TAB_LIB0));
    }
    g_tabs[XMB_TAB_LIB0].kind = TABKIND_LOCAL;
    assert(xmb_classic_media(XMB_TAB_LIB0));
    assert(!xmb_classic_library(XMB_TAB_LIB0));
    g_tabs[XMB_TAB_LIB0].kind = TABKIND_MOVIES;
    const int sizes[][2] = {{720,480}, {720,576}, {1280,720}, {1920,1080}};
    const int scales[] = {0,75,100,125};
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; ++i)
        for (unsigned k = 0; k < sizeof scales / sizeof scales[0]; ++k)
            for (int calibrated = 0; calibrated < 2; ++calibrated)
                for (int episodes = 0; episodes < 4; ++episodes) {
                    display_width = sizes[i][0]; display_height = sizes[i][1];
                    g_uis_pct = scales[k]; inset = calibrated ? 0.05f : 0.0f;
                    g_tabs[XMB_TAB_LIB0].kind = episodes >= 2 ? (episodes == 2 ? TABKIND_MUSIC : TABKIND_PLAYLISTS) : TABKIND_MOVIES;
                    g_tv_depth = episodes == 1 ? 2 : 0;
                    GridGeom gg;
                    assert(xmb_classic_grid_geom(XMB_TAB_LIB0, &gg));
                    assert(gg.cols > 0 && gg.cols <= 12 && (gg.vis == gg.cols * 2 || gg.vis == gg.cols));
                    assert(gg.card_w > 0 && gg.card_h > 0);
                    assert(gg.x0 >= overscan_x());
                    assert(gg.x0 + gg.grid_w <= (int)display_width - overscan_x());
                    assert(xmb_classic_library_y0(XMB_TAB_LIB0) + (gg.vis / gg.cols - 1) * gg.stride + gg.card_h +
                           (episodes >= 2 ? XMB_MUSIC_TEXT_H : XMB_CARD_TEXT_H)
                           <= (int)display_height - XMB_BOTTOM_PAD);
                    assert(gg.portrait == (episodes == 0));
                    if (episodes >= 2) assert(gg.card_w == gg.card_h);
                }
    display_width = 1280; display_height = 720; g_uis_pct = 0; inset = 0;
    g_tabs[XMB_TAB_LIB0].kind = TABKIND_MOVIES;
    reset(); assert(level == SPINE_L2);
    g_sel = 18; g_scroll_top = 10; xmb_classic_library_enter(g_active_tab);
    GridGeom remembered; xmb_grid_geom(g_active_tab, &remembered);
    assert(g_scroll_top % remembered.cols == 0);
    assert(g_sel >= g_scroll_top && g_sel < g_scroll_top + remembered.vis);
    layout = UI_LAYOUT_XMB;
    g_scroll_top = 7; xmb_classic_library_enter(g_active_tab);
    assert(g_scroll_top % 5 == 0 && g_sel >= g_scroll_top && g_sel < g_scroll_top + 10);
    layout = UI_LAYOUT_CLASSIC; reset();
    btn_cur.up = 1; xmb_classic_library_input(g_active_tab);
    assert(xmb_classic_library_header_focus(g_active_tab) == 0 && delegated == 0);
    init_btns(); btn_cur.right = 1; xmb_classic_library_input(g_active_tab);
    assert(xmb_classic_library_header_focus(g_active_tab) == 1);
    init_btns(); btn_cur.cross = 1; xmb_classic_library_input(g_active_tab);
    assert(switched == XMB_TAB_SEARCH);
    reset(); btn_cur.up = 1; xmb_classic_library_input(g_active_tab);
    init_btns(); btn_cur.right = 1; xmb_classic_library_input(g_active_tab);
    init_btns(); btn_cur.right = 1; xmb_classic_library_input(g_active_tab);
    init_btns(); btn_cur.cross = 1; xmb_classic_library_input(g_active_tab);
    assert(switched == XMB_TAB_SETTINGS);
    reset(); btn_cur.up = 1; xmb_classic_library_input(g_active_tab);
    init_btns(); btn_cur.down = 1; xmb_classic_library_input(g_active_tab);
    assert(xmb_classic_library_header_focus(g_active_tab) == -1);
    reset(); btn_cur.circle = 1; xmb_classic_library_input(g_active_tab);
    assert(switched == XMB_TAB_HOME && delegated == 0);
    reset(); g_tv_depth = 1; btn_cur.circle = 1; xmb_classic_library_input(g_active_tab);
    assert(switched == -1 && delegated == 1); // Seasons still owns Back.
    reset(); g_jumpbar_active = true; btn_cur.circle = 1; xmb_classic_library_input(g_active_tab);
    assert(delegated == 1 && switched == -1); // A-Z owns Cancel.
    reset(); g_tab_start[g_active_tab] = 100; btn_cur.up = 1; xmb_classic_library_input(g_active_tab);
    assert(delegated == 1 && xmb_classic_library_header_focus(g_active_tab) == -1);
    reset(); g_sel = 20; btn_cur.up = 1; xmb_classic_library_input(g_active_tab);
    assert(delegated == 1); // Ordinary grid navigation is unchanged.
    g_tabs[XMB_TAB_LIB0].kind = TABKIND_MUSIC;
    reset(); g_music_header = true; btn_cur.up = 1; xmb_classic_library_input(g_active_tab);
    assert(xmb_classic_library_header_focus(g_active_tab) == 0 && delegated == 0);
    init_btns(); btn_cur.down = 1; xmb_classic_library_input(g_active_tab);
    assert(xmb_classic_library_header_focus(g_active_tab) == -1 && g_music_header);
    init_btns(); btn_cur.right = 1; xmb_classic_library_input(g_active_tab);
    assert(delegated == 1);
    reset(); btn_cur.up = 1; xmb_classic_library_input(g_active_tab);
    assert(delegated == 1 && xmb_classic_library_header_focus(g_active_tab) == -1);
    reset(); g_music_depth = 1; btn_cur.circle = 1; xmb_classic_library_input(g_active_tab);
    assert(delegated == 1 && switched == -1);
    reset(); g_music_depth = 1; g_music_sub_sel = 20; btn_cur.up = 1; xmb_classic_library_input(g_active_tab);
    assert(delegated == 1 && xmb_classic_library_header_focus(g_active_tab) == -1);
    reset(); btn_cur.circle = 1; xmb_classic_library_input(g_active_tab);
    assert(switched == XMB_TAB_HOME);
    g_tabs[XMB_TAB_LIB0].kind = TABKIND_LOCAL;
    reset(); btn_cur.up = 1; xmb_classic_library_input(g_active_tab);
    assert(xmb_classic_library_header_focus(g_active_tab) == 0 && delegated == 0);
    reset(); media_top = false; btn_cur.up = 1; xmb_classic_library_input(g_active_tab);
    assert(delegated == 1 && xmb_classic_library_header_focus(g_active_tab) == -1);
    reset(); btn_cur.cross = 1; xmb_classic_library_input(g_active_tab);
    assert(delegated == 1);
    reset(); btn_cur.circle = 1; xmb_classic_library_input(g_active_tab);
    assert(switched == XMB_TAB_HOME);
    puts("Classic library: layout bounds, art shapes, header navigation and Back/paging routing passed");
}

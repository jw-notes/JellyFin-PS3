// ui_home_classic.inc -- the Classic layout's Home (Settings > Display > Layout).
//
// Included by ui_home.cpp after ui_home_stage.inc, so it shares Home's rows,
// their loading and the open / peek actions.  The layout follows Jellyfin's
// web client with its default home sections:
//
//   My Media            wide library tiles
//   Continue Watching   wide stills, with progress
//   Next Up             wide stills
//   Downloaded          (when there is something on this console)
//   Latest <library>    posters (square for music), in library order
//
// under a header with Home / Search / Settings.  The rows scroll vertically and
// each scrolls on its own horizontally, both eased; the focused card has the
// selection ring.  No wave: the theme's gradient alone (wave_set_plain).
//
// The header is the Classic layout's top level: L1 / R1 move between its three
// pages anywhere on them, and Search and Settings keep it (in place of the XMB
// lockup and category row) with their usual contents below.  A library still
// opens its own screen; backing out of anything returns here (ui_xmb.cpp).

#define CLASSIC_FILE "jellyfin_layout.txt"

static int  s_layout = -1;                     // UI_LAYOUT_*, -1 = not read yet

int ui_layout(void) {
    if (s_layout < 0) {
        // Classic unless the viewer picked a layout: the file is only
        // written when the setting is changed.
        s_layout = UI_LAYOUT_CLASSIC;
        FILE *f = fopen(jf_data_path(CLASSIC_FILE), "r");
        if (f) {
            int v = 0;
            if (fscanf(f, "%d", &v) == 1 && v >= 0 && v < UI_LAYOUT__COUNT) s_layout = v;
            fclose(f);
        }
    }
    return s_layout;
}

const char *ui_layout_label(void) {
    return ui_layout() == UI_LAYOUT_CLASSIC ? TR("Classic") : TR("XMB");
}

void ui_layout_step(int dir) {
    (void)dir;                                 // two choices: either way flips
    s_layout = ui_layout() == UI_LAYOUT_CLASSIC ? UI_LAYOUT_XMB : UI_LAYOUT_CLASSIC;
    FILE *f = fopen(jf_data_path(CLASSIC_FILE), "w");
    if (f) { fprintf(f, "%d\n", s_layout); fclose(f); }
}

bool xmb_classic_active(int tab) {
    return ui_layout() == UI_LAYOUT_CLASSIC && tab == XMB_TAB_HOME;
}

// Which header page a tab is: 0 Home, 1 Search, 2 Settings, -1 none.
static int classic_page(int tab) {
    if (tab == XMB_TAB_HOME) return 0;
    if (tab == XMB_TAB_SEARCH) return 1;
    if (tab == XMB_TAB_SETTINGS) return 2;
    if (xmb_classic_library(tab) || xmb_classic_media(tab) || xmb_classic_livetv(tab)) return 0;
    return -1;
}

bool xmb_classic_chrome(int tab) {
    return ui_layout() == UI_LAYOUT_CLASSIC && classic_page(tab) >= 0;
}

// --- rows -------------------------------------------------------------------------------

enum { CS_LAND, CS_PORT, CS_SQUARE, CS_LIB };
#define CR_LIB   (-1)                          // the My Media row
#define CR_MAX   10
#define CR_SLOTS (HOME_ROWS_N + 1)             // per-row state: HR_* rows, then My Media

typedef struct {
    int  src;                                  // HR_* or CR_LIB
    int  shape;                                // CS_*
    int  n;                                    // items (0 while loading)
    bool loading;
    char title[80];
} CRow;

static CRow  s_cr[CR_MAX];
static int   s_ncr = 0;
static int   s_lib_tab[XMB_TAB_COUNT];         // My Media: the tabs it shows
static int   s_nlib = 0;
static int   s_cfocus = 0;                     // index into s_cr; -1 = the header
static int   s_chdr = 1;                       // header focus: 0 Home, 1 Search, 2 Settings
static int   s_ccol_lib = 0;                   // My Media's focused tile
static spine_approach s_cv;                    // vertical scroll, px
static spine_approach s_ch[CR_SLOTS];          // each row's horizontal scroll, px
static bool  s_cfresh = true;

static int cslot(int src) { return src == CR_LIB ? HOME_ROWS_N : src; }
static int *ccol(int src) { return src == CR_LIB ? &s_ccol_lib : &s_row_col[src]; }

static bool lib_kind(XMBTabKind k) {
    return k == TABKIND_MOVIES || k == TABKIND_TV || k == TABKIND_MUSIC ||
           k == TABKIND_BOXSETS || k == TABKIND_PLAYLISTS || k == TABKIND_GENERIC ||
           k == TABKIND_LIVETV;
}

static void cadd(int src, int shape, const char *title) {
    if (s_ncr >= CR_MAX) return;
    CRow *r = &s_cr[s_ncr];
    r->src = src;
    r->shape = shape;
    if (src == CR_LIB) { r->n = s_nlib; r->loading = false; }
    else { r->n = s_rows[src].count; r->loading = !s_rows[src].loaded; }
    // A finished row with nothing in it is not shown, as on the web.
    if (!r->n && !r->loading) return;
    snprintf(r->title, sizeof r->title, "%s", title);
    s_ncr++;
}

static void crows_build(void) {
    s_nlib = 0;
    for (int t = 0; t < XMB_TAB_COUNT; t++)
        if (g_tabs[t].enabled && ((g_tabs[t].library_id[0] && lib_kind(g_tabs[t].kind)) || g_tabs[t].kind == TABKIND_LOCAL || g_tabs[t].kind == TABKIND_LIVETV))
            s_lib_tab[s_nlib++] = t;
    s_ncr = 0;
    if (s_nlib) cadd(CR_LIB, CS_LIB, TR("My Media"));
    cadd(HR_CONTINUE, CS_LAND, TR("Continue Watching"));
    cadd(HR_NEXTUP, CS_LAND, TR("Next Up"));
    cadd(HR_DOWNLOADED, CS_PORT, TR("Downloaded"));
    // "Latest <library>" in library order; Home samples one library per kind.
    bool movies = false, shows = false, music = false;
    for (int i = 0; i < s_nlib; i++) {
        const XMBTab *t = &g_tabs[s_lib_tab[i]];
        char title[80];
        snprintf(title, sizeof title, TR("Latest %s"), t->label);
        if (t->kind == TABKIND_MOVIES && !movies) { movies = true; cadd(HR_MOVIES, CS_PORT, title); }
        else if (t->kind == TABKIND_TV && !shows)  { shows = true;  cadd(HR_SHOWS, CS_PORT, title); }
        else if (t->kind == TABKIND_MUSIC && !music) { music = true; cadd(HR_MUSIC, CS_SQUARE, title); }
    }
    if (s_cfocus >= s_ncr) s_cfocus = s_ncr - 1;
    if (s_cfocus < -1) s_cfocus = -1;
    if (s_ncr == 0) s_cfocus = -1;
}

// --- geometry (authored at 1280x720) ---------------------------------------------------

static void cshape(int shape, int *w, int *h) {
    switch (shape) {
    case CS_PORT:   *w = UIS_W(140); *h = UIS_H(210); break;
    case CS_SQUARE: *w = UIS_W(168); *h = UIS_H(168); break;
    case CS_LIB:    *w = UIS_W(256); *h = UIS_H(144); break;
    default:        *w = UIS_W(256); *h = UIS_H(144); break;
    }
}

#define C_X0       (XMB_OX + UIS_W(64))
#define C_TOP      (XMB_OY + UIS_H(96))             // first row's title
#define C_GAP      UIS_W(18)
#define C_TITLE_H  UIS_H(34)
#define C_LABEL_H  UIS_H(48)
#define C_ROW_GAP  UIS_H(12)

static int crow_h(const CRow *r) {
    int w, h;
    cshape(r->shape, &w, &h);
    return C_TITLE_H + h + C_LABEL_H + C_ROW_GAP;
}

static int cstep(const CRow *r) { int w, h; cshape(r->shape, &w, &h); return w + C_GAP; }

// How many cards a row shows: its items, or placeholders while it loads.
static int cshown(const CRow *r) { return r->loading && !r->n ? 6 : r->n; }

static int crow_y(int i) {
    int y = C_TOP;
    for (int k = 0; k < i; k++) y += crow_h(&s_cr[k]);
    return y - (int)(s_cv.value + 0.5f);
}

// The image behind card c of row r: id, kind and the size it is cached at.
static bool ccard(const CRow *r, int c, const char **id, ThumbImg *img, int *sw, int *sh) {
    cshape(r->shape, sw, sh);
    *img = THUMB_IMG_PRIMARY;
    if (r->src == CR_LIB) {
        if (c >= s_nlib) return false;
        *id = g_tabs[s_lib_tab[c]].library_id;
        return (*id)[0] != 0;
    }
    if (c >= s_rows[r->src].count) return false;
    const XMBItem *it = &s_rows[r->src].items[c];
    *id = it->id;
    if (r->shape == CS_LAND) {
        // A film has a wide "Thumb" when the server has one; an episode's own
        // still is already wide.
        if (it->has_thumb) *img = THUMB_IMG_THUMB;
        else if (strcmp(it->type, "Episode") != 0) *id = q_img_id(r->src, c);
    } else {
        *id = q_img_id(r->src, c);
    }
    return true;
}

// Per frame: the rows, the scroll targets, and the images they need.
static void classic_tick(void) {
    home_init_once();
    home_step_load();
    crows_build();
    const unsigned long long dt = spine_frame_dt_us();
    const int snap = s_cfresh;
    s_cfresh = false;

    // Vertical: the focused row's title sits at the top of the content area.
    float vt = 0.0f;
    for (int k = 0; k < s_cfocus; k++) vt += (float)crow_h(&s_cr[k]);
    depth_follow(&s_cv, vt, snap, 200, dt);

    const int avail = (int)display_width - C_X0 - XMB_OX - UIS_W(32);
    for (int i = 0; i < s_ncr; i++) {
        const CRow *r = &s_cr[i];
        int *col = ccol(r->src);
        if (*col >= r->n) *col = r->n > 0 ? r->n - 1 : 0;
        if (*col < 0) *col = 0;
        // Keep one card visible to the left of the focus once it has moved on.
        const int step = cstep(r);
        float ht = (float)((*col > 1 ? *col - 1 : 0) * step);
        const float max_off = (float)(cshown(r) * step - avail);
        if (ht > max_off) ht = max_off;
        if (ht < 0.0f) ht = 0.0f;
        depth_follow(&s_ch[cslot(r->src)], ht, snap, 180, dt);

        // Ask for what is (nearly) on screen.
        const int y = crow_y(i);
        if (y > (int)display_height || y + crow_h(r) < C_TOP - UIS_H(200)) continue;
        for (int c = 0; c < r->n; c++) {
            const int x = C_X0 + c * step - (int)s_ch[cslot(r->src)].value;
            if (x > (int)display_width + step || x < -2 * step) continue;
            const char *id = ""; ThumbImg img; int sw, sh;
            if (ccard(r, c, &id, &img, &sw, &sh)) {
                thumb_request(id, sw, sh, img);
                if (i == s_cfocus && c == *col && r->src != CR_LIB) {
                    int cw, ch; cshape(r->shape, &cw, &ch);
                    depth_note_focus_rect(x, y + C_TITLE_H, cw, ch);
                    depth_note_focus_card(id, sw, sh, img);
                }
            }
        }
    }
}

// --- drawing -------------------------------------------------------------------------------

static const char *const k_hdr[3] = { TRN("Home"), TRN("Search"), TRN("Settings") };

// The header tabs, centred in the top row: their boxes, for both phases.
static void chdr_boxes(int x[3], int w[3], int *y, int *h) {
    const float px = UIS_TF(17.0f);
    const int pad = UIS_W(18), gap = UIS_W(10);
    int total = 0;
    for (int i = 0; i < 3; i++) { w[i] = ttf_text_width(tr(k_hdr[i]), px, true) + 2 * pad; total += w[i]; }
    total += 2 * gap;
    int cx = ((int)display_width - total) / 2;
    for (int i = 0; i < 3; i++) { x[i] = cx; cx += w[i] + gap; }
    *y = XMB_OY + UIS_H(14);
    *h = UIS_H(34);
}

// The header, on the page `tab` is: the focused tab filled (Home's header
// focus), the current page underlined.
void xmb_classic_header_gpu(int tab) {
    if (!ui_card_gpu_ready()) return;
    const int page = classic_page(tab);
    const int lib_focus = xmb_classic_library_header_focus(tab);
    const bool focus = (tab == XMB_TAB_HOME && s_cfocus == -1) || lib_focus >= 0;
    const int focused = lib_focus >= 0 ? lib_focus : s_chdr;
    int x[3], w[3], y, h;
    chdr_boxes(x, w, &y, &h);
    for (int i = 0; i < 3; i++) {
        if (focus && focused == i)
            ui_rect_gpu_draw(x[i], y, w[i], h, XMB_ACCENT, 255);
        else if (i == page)
            ui_rect_gpu_draw(x[i] + UIS_W(14), y + h - UIS_H(3), w[i] - UIS_W(28), UIS_H(3), XMB_ACCENT, 255);
    }
    ui_card_gpu_end();
}

void xmb_classic_header_text(int tab) {
    xmb_draw_topbar_user();
    const int page = classic_page(tab);
    const int lib_focus = xmb_classic_library_header_focus(tab);
    const bool focus = (tab == XMB_TAB_HOME && s_cfocus == -1) || lib_focus >= 0;
    const int focused = lib_focus >= 0 ? lib_focus : s_chdr;
    int x[3], w[3], y, h;
    chdr_boxes(x, w, &y, &h);
    const float px = UIS_TF(17.0f);
    for (int i = 0; i < 3; i++) {
        const bool on = focus && focused == i;
        const char *t = tr(k_hdr[i]);
        const int tw = ttf_text_width(t, px, true);
        drawTTF((u32)(x[i] + (w[i] - tw) / 2), (u32)(y + (h - (int)px) / 2 - UIS_H(2)), t, px,
                on ? XMB_KEY_LABEL_SEL : (i == page ? XMB_TEXT : XMB_TEXT_DIM), true);
    }
}

void xmb_classic_header_cpu(int tab) {
    xmb_cpu_draw_user();
    if (ui_card_gpu_ready()) return;
    const int page = classic_page(tab);
    const int lib_focus = xmb_classic_library_header_focus(tab);
    const bool focus = (tab == XMB_TAB_HOME && s_cfocus == -1) || lib_focus >= 0;
    const int focused = lib_focus >= 0 ? lib_focus : s_chdr;
    int x[3], w[3], y, h;
    chdr_boxes(x, w, &y, &h);
    for (int i = 0; i < 3; ++i) {
        if (focus && focused == i)
            drawRect((u32)x[i], (u32)y, (u32)w[i], (u32)h, XMB_ACCENT);
        else if (i == page)
            drawRect((u32)(x[i] + UIS_W(14)), (u32)(y + h - UIS_H(3)),
                     (u32)(w[i] - UIS_W(28)), (u32)UIS_H(3), XMB_ACCENT);
    }
}

void xmb_classic_gpu(void) {
    classic_tick();
    xmb_classic_header_gpu(XMB_TAB_HOME);
    if (!ui_card_gpu_ready()) return;

    ui_card_gpu_clip(C_TOP - UIS_H(8), 0);
    int fx = -1, fy = 0, fw = 0, fh = 0;            // the focused card, for the ring
    for (int i = 0; i < s_ncr; i++) {
        const CRow *r = &s_cr[i];
        const int y0 = crow_y(i);
        if (y0 > (int)display_height || y0 + crow_h(r) < C_TOP - UIS_H(8)) continue;
        int cw, ch;
        cshape(r->shape, &cw, &ch);
        const int cy = y0 + C_TITLE_H;
        const int step = cstep(r);
        const float off = s_ch[cslot(r->src)].value;
        const int col = *ccol(r->src);
        for (int c = 0; c < cshown(r); c++) {
            const int cx = C_X0 + c * step - (int)(off + 0.5f);
            if (cx > (int)display_width || cx + cw < 0) continue;
            const char *id = ""; ThumbImg img; int sw, sh;
            u32 toff = 0, pitch = 0;
            const bool have = ccard(r, c, &id, &img, &sw, &sh) &&
                              thumb_gpu_texture(id, sw, sh, &toff, &pitch, img);
            if (have) {
                const float fade = thumb_fade_in(id, sw, sh, img);
                if (fade < 1.0f) ui_rect_gpu_draw(cx, cy, cw, ch, XMB_THUMB_DIM, 255);
                ui_card_gpu_draw_ex(toff, (u32)sw, (u32)sh, pitch, cx, cy, cw, ch,
                                    0.0f, 1.0f, (u8)(fade * 255.0f + 0.5f));
            } else {
                ui_rect_gpu_draw(cx, cy, cw, ch, XMB_THUMB_DIM, 255);
                if (r->loading || (c < r->n && id[0] && !thumb_failed(id, img)))
                    depth_shimmer_gpu(cx, cy, cw, ch, 1.0f);
            }
            // Watched progress along the bottom (Continue Watching).
            if (c < r->n && r->src != CR_LIB && s_rows[r->src].items[c].progress_pct > 0) {
                const int sh2 = UIS_H(4) > 2 ? UIS_H(4) : 2;
                ui_rect_gpu_draw(cx, cy + ch - sh2, cw, sh2, XMB_TRACK, 230);
                ui_rect_gpu_draw(cx, cy + ch - sh2, cw * s_rows[r->src].items[c].progress_pct / 100,
                                 sh2, XMB_ACCENT_ALT, 255);
            }
            if (i == s_cfocus && c == col && c < r->n) { fx = cx; fy = cy; fw = cw; fh = ch; }
        }
    }
    if (fx >= 0) ui_card_gpu_selection(fx, fy, fw, fh);
    ui_card_gpu_clip(0, 0);
    ui_card_gpu_end();
}

void xmb_classic_cpu(void) {
    // Everything is on the GPU; with the GPU card path off, nothing is drawn
    // here, and the text alone still shows what each row holds.
}

static void clabel(const CRow *r, int c, char *a, size_t ac, char *b, size_t bc) {
    a[0] = b[0] = '\0';
    if (r->src == CR_LIB) { snprintf(a, ac, "%s", g_tabs[s_lib_tab[c]].label); return; }
    const XMBItem *it = &s_rows[r->src].items[c];
    snprintf(a, ac, "%s", q_title(r->src, c));
    const HomeExtra *x = &s_extra[r->src][c];
    if (strcmp(it->type, "Episode") == 0 && x->episode)
        snprintf(b, bc, "S%u:E%u - %s", (unsigned)x->season, (unsigned)x->episode, it->name);
    else if (strcmp(it->type, "MusicAlbum") == 0 && it->artist[0])
        snprintf(b, bc, "%s", it->artist);
    else
        snprintf(b, bc, "%s", it->year_str);
}

void xmb_classic_text(void) {
    xmb_classic_header_text(XMB_TAB_HOME);
    const int old_top = g_cpu_clip_top;
    g_cpu_clip_top = C_TOP - UIS_H(8);
    for (int i = 0; i < s_ncr; i++) {
        const CRow *r = &s_cr[i];
        const int y0 = crow_y(i);
        if (y0 > (int)display_height || y0 + crow_h(r) < C_TOP - UIS_H(8)) continue;
        drawTTF((u32)C_X0, (u32)y0, r->title, UIS_TF(19.0f),
                i == s_cfocus ? XMB_TEXT : XMB_TEXT_DIM, true);
        int cw, ch;
        cshape(r->shape, &cw, &ch);
        const int step = cstep(r);
        const float off = s_ch[cslot(r->src)].value;
        const int col = *ccol(r->src);
        for (int c = 0; c < r->n; c++) {
            const int cx = C_X0 + c * step - (int)(off + 0.5f);
            if (cx > (int)display_width || cx + cw < 0) continue;
            if (r->src == CR_LIB && (g_tabs[s_lib_tab[c]].kind == TABKIND_LOCAL || g_tabs[s_lib_tab[c]].kind == TABKIND_LIVETV))
                drawIcon((u32)(cx + (cw - UIS_W(40)) / 2), (u32)(y0 + C_TITLE_H + (ch - UIS_H(40)) / 2),
                         g_tabs[s_lib_tab[c]].kind == TABKIND_LIVETV ? ICON_ANTENNA : ICON_COLLECTIONS,
                         UIS_TF(40), XMB_TEXT_DIM);
            char a[160], b[160], fit[160];
            clabel(r, c, a, sizeof a, b, sizeof b);
            const bool on = i == s_cfocus && c == col;
            const int ly = y0 + C_TITLE_H + ch + UIS_H(8);
            depth_fit_text(fit, sizeof fit, a, UIS_TF(15.0f), UI_FACE_REGULAR, cw);
            drawTTF((u32)cx, (u32)ly, fit, UIS_TF(15.0f), on ? XMB_WHITE : XMB_TEXT, on);
            if (b[0]) {
                depth_fit_text(fit, sizeof fit, b, UIS_TF(13.0f), UI_FACE_REGULAR, cw);
                drawTTF((u32)cx, (u32)(ly + UIS_H(20)), fit, UIS_TF(13.0f), XMB_TEXT_FAINT);
            }
        }
    }
    g_cpu_clip_top = old_top;
}

void xmb_classic_hints(void) {
    if (s_cfocus == -1) {
        static const Hint h[] = {{'E', TRN("Nav")}, {'X', TRN("Select")}};
        draw_hints_bar(h, 2);
    } else if (s_cfocus < s_ncr && s_cr[s_cfocus].src == CR_LIB) {
        static const Hint h[] = {{'E', TRN("Nav")}, {'X', TRN("Open")}};
        draw_hints_bar(h, 2);
    } else {
        static const Hint h[] = {{'E', TRN("Nav")}, {'X', TRN("Open")}, {'T', TRN("Details")}};
        draw_hints_bar(h, 3);
    }
}

// --- input -------------------------------------------------------------------------------

// Leave Home for one of the usual screens, inside it.
static void classic_open_tab(int tab) {
    xmb_switch_tab(tab);
    spine_set_level(SPINE_L2);
    init_btns();
}

// L1 / R1 on any header page: the page to the left / right.  True when it
// took the press.
bool xmb_classic_switch_input(int tab) {
    if (!xmb_classic_chrome(tab)) return false;
    const bool l = BTN_PRESSED(l1), r = BTN_PRESSED(r1);
    if (!l && !r) return false;
    if ((xmb_classic_library(tab) || xmb_classic_media(tab) || xmb_classic_livetv(tab)) && l) {
        xmb_switch_tab(XMB_TAB_HOME);
        init_btns();
        return true;
    }
    const int page = classic_page(tab);
    const int to = page + (l ? -1 : 1);
    if (to < 0 || to > 2) { init_btns(); return true; }
    ui_sfx_play(SFX_CURSOR);
    if (to == 0) {
        xmb_switch_tab(XMB_TAB_HOME);
        init_btns();
    } else {
        classic_open_tab(to == 1 ? XMB_TAB_SEARCH : XMB_TAB_SETTINGS);
    }
    return true;
}

bool xmb_classic_input(void) {
    home_init_once();
    crows_build();
    if (BTN_REPEAT(up)) {
        if (s_cfocus > 0) s_cfocus--;
        else if (s_cfocus == 0) { s_cfocus = -1; s_chdr = 1; }
        ui_sfx_play(SFX_CURSOR);
        return false;
    }
    if (BTN_REPEAT(down)) {
        if (s_cfocus < s_ncr - 1) { s_cfocus++; ui_sfx_play(SFX_CURSOR); }
        return false;
    }
    if (s_cfocus == -1) {
        if (BTN_REPEAT(left) && s_chdr > 0)  { s_chdr--; ui_sfx_play(SFX_CURSOR); }
        if (BTN_REPEAT(right) && s_chdr < 2) { s_chdr++; ui_sfx_play(SFX_CURSOR); }
        if (BTN_PRESSED(cross)) {
            ui_sfx_play(SFX_DECIDE);
            if (s_chdr == 1)      classic_open_tab(XMB_TAB_SEARCH);
            else if (s_chdr == 2) classic_open_tab(XMB_TAB_SETTINGS);
            else if (s_ncr > 0)   s_cfocus = 0;           // Home: down into it
        }
        return false;
    }
    if (s_cfocus >= s_ncr) return false;
    const CRow *r = &s_cr[s_cfocus];
    int *col = ccol(r->src);
    if (BTN_REPEAT(left) && *col > 0)          { (*col)--; ui_sfx_play(SFX_CURSOR); }
    if (BTN_REPEAT(right) && *col < r->n - 1)  { (*col)++; ui_sfx_play(SFX_CURSOR); }
    if (r->n <= 0) return false;
    if (BTN_PRESSED(cross)) {
        ui_sfx_play(SFX_DECIDE);
        if (r->src == CR_LIB) { classic_open_tab(s_lib_tab[*col]); return false; }
        s_focus_row = r->src;                       // Home's own open: details, series, album
        xmb_home_open_focused();
        return false;
    }
    if (BTN_PRESSED(triangle) && r->src != CR_LIB) {
        s_focus_row = r->src;
        xmb_home_peek_focused();
        return false;
    }
    return false;
}

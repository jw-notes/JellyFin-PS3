// Settings > System > About: the version, who made the app, and the licences
// of everything in it.  A screen of its own, scrolled with Up / Down (L1 / R1
// a page at a time); O goes back.  The same list as CREDITS.md.

#include <stdio.h>
#include <string.h>

#include <sysutil/sysutil.h>

#include "ui.h"
#include "ui_visuals.h"
#include "ui_wave.h"
#include "rsxutil.h"
#include "update_check.h"

namespace {

enum Kind { K_HEAD, K_NAME, K_TEXT, K_GAP };
struct Line { Kind kind; const char *a; const char *b; };   // b: right column (licence)

// Proper names are not translated; headings and sentences are.
const Line k_lines[] = {
    { K_HEAD, TRN("Made by"), NULL },
    { K_NAME, "jw-notes", "JellyFin [PS]" },
    { K_NAME, "vortigauntlet", "JellyFin-PS3-LosslessAudio" },
    { K_NAME, "MontyMcK", "JellyFin-PS3, the original" },
    { K_TEXT, TRN("With contributions from Ethan Hammer, Lucian, orbisai0security and others."), NULL },
    { K_GAP, NULL, NULL },
    { K_HEAD, TRN("Thanks to"), NULL },
    { K_NAME, "Movian", "the media player this follows" },
    { K_NAME, "Jellyfin", "jellyfin.org" },
    { K_NAME, "ps3dev, PSL1GHT", "the SDK" },
    { K_GAP, NULL, NULL },
    { K_HEAD, TRN("Open-source software"), NULL },
    { K_NAME, "FFmpeg (TrueHD, DTS-HD)", "LGPL 2.1+" },
    { K_NAME, "liba52 (Dolby Digital)", "GPL 2+" },
    { K_NAME, "libdca (DTS)", "GPL 2+" },
    { K_NAME, "minimp3", "CC0" },
    { K_NAME, "stb_image, stb_truetype", "Public domain / MIT" },
    { K_NAME, "mohasi NTFS / exFAT drivers", "Apache 2.0" },
    { K_NAME, "PSL1GHT", "MIT" },
    { K_NAME, "libFLAC", "BSD" },
    { K_NAME, "FAAD2", "GPL 2+" },
    { K_GAP, NULL, NULL },
    { K_HEAD, TRN("Fonts and art"), NULL },
    { K_NAME, "Archivo, Noto Sans, Noto Sans JP, Michroma", "SIL OFL 1.1" },
    { K_NAME, "Open Sans, Roboto Condensed, Material Icons", "Apache 2.0" },
    { K_NAME, "Satoshi", "ITF Free Font License" },
    { K_NAME, "Tabler Icons", "MIT" },
    { K_NAME, "PlayStation button sprites (Kenney)", "CC0" },
    { K_NAME, "Jellyfin logo", "CC BY-SA 4.0" },
    { K_TEXT, TRN("Rodin is the PS3's own system font, read from the console."), NULL },
    { K_GAP, NULL, NULL },
    { K_HEAD, TRN("License"), NULL },
    { K_TEXT, TRN("JellyFin [PS] is free software under the GNU GPL, version 3 or later."), NULL },
    { K_TEXT, TRN("It comes with no warranty. Each component keeps its own licence."), NULL },
    { K_TEXT, TRN("Source code and the full licence texts:"), NULL },
    { K_TEXT, "github.com/jw-notes/JellyFin-PS3", NULL },
};
const int N_LINES = (int)(sizeof k_lines / sizeof k_lines[0]);

int line_h(Kind k) {
    switch (k) {
    case K_HEAD: return UIS_H(40);
    case K_GAP:  return UIS_H(14);
    default:     return UIS_H(26);
    }
}

void draw_about(int top) {
    waitflip();
    clearScreen(XMB_BG);
    wave_draw();
    rsxSync();

    const int W = (int)display_width;
    const int col_w = UIS_W(860);
    const int x0 = (W - col_w) / 2;
    const int y_title = XMB_OY + UIS_H(46);
    const int list_top = XMB_OY + UIS_H(150), list_bot = (int)display_height - XMB_OY - UIS_H(70);

    int y = list_top - top;
    for (int i = 0; i < N_LINES; i++) {
        const Line &l = k_lines[i];
        const int h = line_h(l.kind);
        if (y + h > list_top && y < list_bot) {
            switch (l.kind) {
            case K_HEAD:
                drawTTF((u32)x0, (u32)(y + UIS_H(12)), tr(l.a), UIS_TF(17.0f), XMB_ACCENT_ALT, true);
                break;
            case K_NAME: {
                drawTTF((u32)x0, (u32)y, l.a, UIS_TF(16.0f), XMB_TEXT);
                if (l.b) {
                    const float px = UIS_TF(15.0f);
                    const int bw = ttf_text_width(l.b, px);
                    drawTTF((u32)(x0 + col_w - bw), (u32)y, l.b, px, XMB_TEXT_DIM);
                }
                break;
            }
            case K_TEXT:
                drawTTF((u32)x0, (u32)y, tr(l.a), UIS_TF(15.0f), XMB_TEXT_DIM);
                break;
            default: break;
            }
        }
        y += h;
    }
    // The list scrolls under the title and the hints: mask it at both ends.
    drawRect(0, 0, (u32)W, (u32)(list_top - UIS_H(15)), XMB_BG);
    drawRect((u32)x0, (u32)(list_top - UIS_H(16)), (u32)col_w, 1, XMB_HAIRLINE);
    {
        const char *t = "JellyFin [PS]";
        const float px = UIS_TF(32.0f);
        drawTTF((u32)((W - ttf_text_width(t, px, true)) / 2), (u32)y_title, t, px, XMB_TEXT, true);
        char v[64];
        snprintf(v, sizeof v, TR("Version %s"), APP_VERSION);
        const float vp = UIS_TF(15.0f);
        drawTTF((u32)((W - ttf_text_width(v, vp)) / 2), (u32)(y_title + UIS_H(46)), v, vp, XMB_ACCENT);
    }
    drawRect(0, (u32)list_bot, (u32)W, (u32)((int)display_height - list_bot), XMB_BG);

    static const Hint hints[] = {{'D', TRN("Scroll")}, {'C', TRN("Back")}};
    draw_hints_bar(hints, 2);
    flip();
}

} // namespace

void xmb_show_about(void) {
    int total = 0;
    for (int i = 0; i < N_LINES; i++) total += line_h(k_lines[i].kind);
    const int view = (int)display_height - 2 * XMB_OY - UIS_H(220);
    const int max_top = total > view ? total - view : 0;
    const int step = UIS_H(26), page = view - UIS_H(52);
    int top = 0;

    init_btns();
    while (running) {
        sysUtilCheckCallback();
        poll_buttons();
        if (BTN_PRESSED(circle)) break;
        if (BTN_REPEAT(down)) top += step;
        if (BTN_REPEAT(up))   top -= step;
        if (BTN_PRESSED(r1))  top += page;
        if (BTN_PRESSED(l1))  top -= page;
        if (top > max_top) top = max_top;
        if (top < 0) top = 0;
        draw_about(top);
    }
    init_btns();
}

// Settings > System > About: the version, then three sections side by side --
// Updates (what changed lately), Credits and Licences.  L1 / R1 (or Left /
// Right) change section, as Classic's header tabs do; Up / Down scroll it; O
// goes back.  The
// credits and licences are the same list as CREDITS.md.

#include <stdio.h>
#include <string.h>

#include <sysutil/sysutil.h>

#include "ui.h"
#include "ui_visuals.h"
#include "ui_wave.h"
#include "rsxutil.h"
#include "update_check.h"

namespace {

enum Kind { K_HEAD, K_VER, K_ITEM, K_NAME, K_TEXT, K_GAP };
struct Line { Kind kind; const char *a; const char *b; };   // b: right column (licence)

// Updates: the last releases, newest first, from CHANGELOG.md in short.
// Add the new release at the top with each one.
const Line k_updates[] = {
    { K_VER,  "3.4.3", NULL },
    { K_ITEM, TRN("Each PS3 user keeps their own sign-in, as PS3 save data."), NULL },
    { K_ITEM, TRN("Share Sign-in lends your account to PS3 users with none of their own."), NULL },
    { K_ITEM, TRN("Your Jellyfin profile picture shows at the top right."), NULL },
    { K_ITEM, TRN("The PS button no longer freezes the console on the sign-in screen."), NULL },
    { K_ITEM, TRN("Backing out of Quick Connect returns to the sign-in choice."), NULL },
    { K_VER,  "3.4.2", NULL },
    { K_ITEM, TRN("Fixed a FLAC decoding bug with damaged files."), NULL },
    { K_VER,  "3.4.1", NULL },
    { K_ITEM, TRN("Cast members without a photo get a silhouette."), NULL },
    { K_ITEM, TRN("The XMB cast scrolls, and XMB Live TV is redesigned."), NULL },
    { K_VER,  "3.4.0", NULL },
    { K_ITEM, TRN("The app is renamed JellyFin [PS]."), NULL },
    { K_VER,  "3.3.9", NULL },
    { K_ITEM, TRN("Films keep their network connection over Wi-Fi."), NULL },
    { K_ITEM, TRN("Classic details page redesigned, with a scrolling cast."), NULL },
    { K_VER,  "3.3.8", NULL },
    { K_ITEM, TRN("Smoother Live TV on Wi-Fi, and reconnects after a dropped channel."), NULL },
    { K_VER,  "3.3.7", NULL },
    { K_ITEM, TRN("Classic is the default layout."), NULL },
    { K_VER,  "3.3.6", NULL },
    { K_ITEM, TRN("Live TV gets genre tabs, a channel grid and remote-style controls."), NULL },
    { K_GAP,  NULL, NULL },
    { K_TEXT, TRN("Every release in full:"), NULL },
    { K_TEXT, "github.com/jw-notes/JellyFin-PS3/releases", NULL },
};

// Proper names are not translated; headings and sentences are.
const Line k_credits[] = {
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
};

const Line k_licences[] = {
    { K_HEAD, TRN("License"), NULL },
    { K_TEXT, TRN("JellyFin [PS] is free software under the GNU GPL, version 3 or later."), NULL },
    { K_TEXT, TRN("It comes with no warranty. Each component keeps its own licence."), NULL },
    { K_TEXT, TRN("Source code and the full licence texts:"), NULL },
    { K_TEXT, "github.com/jw-notes/JellyFin-PS3", NULL },
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
};

struct Section { const char *label; const Line *lines; int n; };
#define SECTION(label, arr) { label, arr, (int)(sizeof arr / sizeof arr[0]) }
const Section k_sections[] = {
    SECTION(TRN("Updates"),  k_updates),
    SECTION(TRN("Credits"),  k_credits),
    SECTION(TRN("Licences"), k_licences),
};
const int N_SECTIONS = (int)(sizeof k_sections / sizeof k_sections[0]);

int line_h(Kind k) {
    switch (k) {
    case K_HEAD: return UIS_H(40);
    case K_VER:  return UIS_H(34);
    case K_GAP:  return UIS_H(14);
    default:     return UIS_H(26);
    }
}

int section_h(const Section &s) {
    int total = 0;
    for (int i = 0; i < s.n; i++) total += line_h(s.lines[i].kind);
    return total;
}

void draw_about(int sec, int top) {
    waitflip();
    clearScreen(XMB_BG);
    wave_draw();
    rsxSync();

    const int W = (int)display_width;
    const int col_w = UIS_W(860);
    const int x0 = (W - col_w) / 2;
    const int y_title = XMB_OY + UIS_H(40);
    const int y_tabs  = XMB_OY + UIS_H(132);
    const int list_top = XMB_OY + UIS_H(190), list_bot = (int)display_height - XMB_OY - UIS_H(70);

    const Section &s = k_sections[sec];
    int y = list_top - top;
    for (int i = 0; i < s.n; i++) {
        const Line &l = s.lines[i];
        const int h = line_h(l.kind);
        if (y + h > list_top && y < list_bot) {
            switch (l.kind) {
            case K_HEAD:
                drawTTF((u32)x0, (u32)(y + UIS_H(12)), tr(l.a), UIS_TF(17.0f), XMB_ACCENT_ALT, true);
                break;
            case K_VER:
                drawTTF((u32)x0, (u32)(y + UIS_H(8)), l.a, UIS_TF(17.0f), XMB_ACCENT_ALT, true);
                break;
            case K_ITEM:
                drawTTF((u32)(x0 + UIS_W(6)), (u32)y, "\xC2\xB7", UIS_TF(15.0f), XMB_ACCENT);
                drawTTF((u32)(x0 + UIS_W(22)), (u32)y, tr(l.a), UIS_TF(15.0f), XMB_TEXT_DIM);
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
    // The list scrolls under the title, the tabs and the hints: mask both ends.
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
    {
        // The sections as tabs, centred: the current one bold and underlined.
        const float px = UIS_TF(17.0f);
        const int gap = UIS_W(44);
        int w[8], total = 0;
        for (int i = 0; i < N_SECTIONS; i++) {
            w[i] = ttf_text_width(tr(k_sections[i].label), px, i == sec);
            total += w[i] + (i ? gap : 0);
        }
        int x = (W - total) / 2;
        for (int i = 0; i < N_SECTIONS; i++) {
            const bool on = i == sec;
            drawTTF((u32)x, (u32)y_tabs, tr(k_sections[i].label), px, on ? XMB_WHITE : XMB_TEXT_FAINT, on);
            if (on) drawRect((u32)x, (u32)(y_tabs + UIS_H(26)), (u32)w[i], UIS_H(3), XMB_ACCENT);
            x += w[i] + gap;
        }
    }
    drawRect(0, (u32)list_bot, (u32)W, (u32)((int)display_height - list_bot), XMB_BG);

    static const Hint hints[] = {{'l', ""}, {'r', TRN("Section")}, {'D', TRN("Scroll")}, {'C', TRN("Back")}};
    draw_hints_bar(hints, 4);
    flip();
}

} // namespace

void xmb_show_about(void) {
    const int view = (int)display_height - 2 * XMB_OY - UIS_H(260);
    const int step = UIS_H(26);
    int sec = 0, top = 0;

    init_btns();
    while (running) {
        sysUtilCheckCallback();
        poll_buttons();
        if (BTN_PRESSED(circle)) break;
        const bool prev = BTN_PRESSED(l1) || BTN_REPEAT(left);
        const bool next = BTN_PRESSED(r1) || BTN_REPEAT(right);
        if (prev && sec > 0)              { sec--; top = 0; }
        if (next && sec < N_SECTIONS - 1) { sec++; top = 0; }
        if (BTN_REPEAT(down)) top += step;
        if (BTN_REPEAT(up))   top -= step;
        const int total = section_h(k_sections[sec]);
        const int max_top = total > view ? total - view : 0;
        if (top > max_top) top = max_top;
        if (top < 0) top = 0;
        draw_about(sec, top);
    }
    init_btns();
}

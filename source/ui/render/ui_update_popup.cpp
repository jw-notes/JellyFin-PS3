// Update-available popup -- modal panel shown over the whole XMB screen once
// the background GitHub check (net/update_check.cpp) reports a newer release,
// and again from Settings > Software Update.  The frame is drawn normally
// first; this dims it with the GPU quad and lays the panel on top.
//
// It offers the update and, if the user takes it, downloads and installs it
// (net/update_install.cpp):
//   offer       X Update now  /  O Not now
//   downloading progress bar  /  O Cancel
//   ready       X Install now (the app closes)  /  O Later
//   failed      the reason    /  X Try again  /  O Close
// Without webMAN the ready step says where the package is instead.

#include <stdio.h>
#include <string.h>

#include "ui_visuals.h"
#include "ui_wave.h"
#include "update_check.h"
#include "update_install.h"

static bool s_dismissed = false;
static bool s_have_ver  = false;
static bool s_manual    = false;     // webMAN was not there: show the Package Manager steps
static char s_version[64];

bool xmb_update_popup_active(void) {
    if (s_dismissed) return false;
    if (!s_have_ver) {
        if (!update_check_result(s_version, sizeof(s_version))) return false;
        s_have_ver = true;
    }
    return true;
}

void xmb_update_popup_reopen(void) {
    s_dismissed = false;
    s_have_ver  = false;
    s_manual    = false;
}

static void close_popup(void) {
    s_dismissed = true;
    init_btns();   // don't leak the press into the screen underneath
}

void xmb_update_popup_input(void) {
    update_dl_poll();
    const int st = update_dl_state();
    if (s_manual) {
        // The package is in /dev_hdd0/packages; closing lets them install it.
        if (BTN_PRESSED(cross)) { running = 0; init_btns(); }
        else if (BTN_PRESSED(circle)) close_popup();
        return;
    }
    switch (st) {
    case UDL_RUNNING:
        if (BTN_PRESSED(circle)) { update_dl_cancel(); init_btns(); }
        break;
    case UDL_READY:
        if (BTN_PRESSED(cross)) {
            // webMAN installs it once the app has closed; without webMAN the
            // steps are shown instead.
            if (update_install()) running = 0;
            else                  s_manual = true;
            init_btns();
        } else if (BTN_PRESSED(circle)) {
            close_popup();
        }
        break;
    case UDL_FAILED:
        if (BTN_PRESSED(cross))       { update_dl_start(); init_btns(); }
        else if (BTN_PRESSED(circle)) close_popup();
        break;
    default:   // offer
        if (BTN_PRESSED(cross))       { update_dl_start(); init_btns(); }
        else if (BTN_PRESSED(circle)) close_popup();
        break;
    }
}

// 1px hairline outline around a rect (same as the settings card / confirm).
static void hairline_frame(int x, int y, int w, int h) {
    drawRect((u32)x, (u32)y, (u32)w, 1, XMB_HAIRLINE);
    drawRect((u32)x, (u32)(y + h - 1), (u32)w, 1, XMB_HAIRLINE);
    drawRect((u32)x, (u32)y, 1, (u32)h, XMB_HAIRLINE);
    drawRect((u32)(x + w - 1), (u32)y, 1, (u32)h, XMB_HAIRLINE);
}

static void centred(int mx, int mw, int y, const char *s, float px, u32 col, bool bold = false) {
    const int w = ttf_text_width(s, px, bold);
    drawTTF((u32)(mx + (mw - w) / 2), (u32)y, s, px, col, bold);
}

void xmb_update_popup_draw(void) {
    // GPU dim over the fully drawn frame; returns fenced, so the CPU panel
    // writes below land on top of it.
    wave_dim_screen(110);
    update_dl_poll();

    const int mw = UIS_W(600), mh = UIS_H(250);
    int mx = ((int)display_width  - mw) / 2;
    int my = ((int)display_height - mh) / 2 - UIS_H(20);
    drawRect((u32)mx, (u32)my, (u32)mw, (u32)mh, XMB_PANEL);
    hairline_frame(mx, my, mw, mh);

    const char *v = s_version;
    if (*v == 'v' || *v == 'V') v++;
    char line[200];
    const int st = update_dl_state();

    if (s_manual) {
        centred(mx, mw, my + UIS_H(28), TR("Install from Package Manager"), UIS_TF(21), XMB_TEXT, true);
        centred(mx, mw, my + UIS_H(72), TR("The new version is saved in /dev_hdd0/packages."),
                UIS_TF(14), XMB_TEXT_DIM);
        centred(mx, mw, my + UIS_H(98), TR("Close the app, then go to Game > Package Manager >"),
                UIS_TF(14), XMB_TEXT_DIM);
        centred(mx, mw, my + UIS_H(120), TR("Install Package Files > Standard."), UIS_TF(14), XMB_TEXT_DIM);
        static const Hint h[] = {{'X', TRN("Close the app")}, {'C', TRN("Later")}};
        draw_hints_bar(h, 2);
        return;
    }

    switch (st) {
    case UDL_RUNNING: {
        snprintf(line, sizeof line, TR("Downloading version %s"), v);
        centred(mx, mw, my + UIS_H(28), line, UIS_TF(21), XMB_TEXT, true);
        u32 got = 0, total = 0;
        update_dl_progress(&got, &total);
        const int bw = mw - UIS_W(80), bh = UIS_H(10);
        const int bx = mx + UIS_W(40), by = my + UIS_H(110);
        drawRect((u32)bx, (u32)by, (u32)bw, (u32)bh, XMB_TRACK);
        if (total > 0) {
            const u32 fill = (u32)((u64)bw * (got > total ? total : got) / total);
            if (fill) drawRect((u32)bx, (u32)by, fill, (u32)bh, XMB_ACCENT);
            snprintf(line, sizeof line, TR("%.1f MB of %.1f MB"),
                     (double)got / 1048576.0, (double)total / 1048576.0);
        } else {
            snprintf(line, sizeof line, TR("%.1f MB"), (double)got / 1048576.0);
        }
        centred(mx, mw, by + UIS_H(26), line, UIS_TF(14), XMB_TEXT_DIM);
        static const Hint h[] = {{'C', TRN("Cancel")}};
        draw_hints_bar(h, 1);
        break;
    }
    case UDL_READY:
        centred(mx, mw, my + UIS_H(28), TR("Ready to install"), UIS_TF(21), XMB_TEXT, true);
        snprintf(line, sizeof line, TR("Version %s has downloaded and checked out."), v);
        centred(mx, mw, my + UIS_H(72), line, UIS_TF(14), XMB_TEXT_DIM);
        centred(mx, mw, my + UIS_H(98), TR("The app closes and the new version installs."),
                UIS_TF(14), XMB_TEXT_DIM);
        centred(mx, mw, my + UIS_H(120), TR("Your sign-in and settings are kept."), UIS_TF(14), XMB_TEXT_FAINT);
        {
            static const Hint h[] = {{'X', TRN("Install now")}, {'C', TRN("Later")}};
            draw_hints_bar(h, 2);
        }
        break;
    case UDL_FAILED:
        centred(mx, mw, my + UIS_H(28), TR("The update did not download"), UIS_TF(21), XMB_TEXT, true);
        centred(mx, mw, my + UIS_H(76), tr(update_dl_error()), UIS_TF(14), XMB_TEXT_DIM);
        {
            // The code behind it, for a bug report; the sentence above is the explanation.
            bool http = false;
            const u32 code = update_dl_error_code(&http);
            if (code) {
                if (http) snprintf(line, sizeof line, "HTTP %u", (unsigned)code);
                else      snprintf(line, sizeof line, TR("Error code 0x%08x"), (unsigned)code);
                centred(mx, mw, my + UIS_H(102), line, UIS_TF(12), XMB_TEXT_FAINT);
            }
        }
        {
            static const Hint h[] = {{'X', TRN("Try again")}, {'C', TRN("Close")}};
            draw_hints_bar(h, 2);
        }
        break;
    default:
        centred(mx, mw, my + UIS_H(28), TR("New version available"), UIS_TF(21), XMB_TEXT, true);
        snprintf(line, sizeof line, TR("Version %s"), v);
        centred(mx, mw, my + UIS_H(66), line, UIS_TF(15), XMB_ACCENT);
        centred(mx, mw, my + UIS_H(100), TR("Download and install it now? It takes about a minute."),
                UIS_TF(14), XMB_TEXT_DIM);
        centred(mx, mw, my + UIS_H(124), TR("Your sign-in and settings are kept."), UIS_TF(14), XMB_TEXT_FAINT);
        {
            static const Hint h[] = {{'X', TRN("Update now")}, {'C', TRN("Not now")}};
            draw_hints_bar(h, 2);
        }
        break;
    }
}

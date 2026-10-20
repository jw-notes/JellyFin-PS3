// Authentication, config save/load.

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <io/pad.h>
#include <sysutil/sysutil.h>

#include "jellyfin_api.h"
#include "auth_data.h"
#include "ui.h"
#include "ui_visuals.h"
#include "timing.h"
#include "jf_paths.h"

volatile bool g_auth_expired = false;

// Per-install DeviceId, generated once and persisted.  Jellyfin keys device
// sessions on this string: with the old fixed "ps3" id, a login from ANY
// machine (an RPCS3 test run, a second console) took over the slot and
// revoked this install's token — which then failed as a silent empty
// library.  Survives logout on purpose so one install keeps one identity.
static char s_device_id[24] = "";

const char *jf_device_id(void) {
    if (s_device_id[0]) return s_device_id;
    FILE *f = fopen(jf_data_path("jellyfin_device_id.txt"), "r");
    if (f) {
        if (fscanf(f, "%23s", s_device_id) != 1) s_device_id[0] = '\0';
        fclose(f);
    }
    if (!s_device_id[0]) {
        u32 a = (u32)time(NULL);
        u32 b = (u32)timing_get_us();
        snprintf(s_device_id, sizeof(s_device_id), "ps3-%08x%08x",
                 a ^ 0x9e3779b9u, b);
        f = fopen(jf_data_path("jellyfin_device_id.txt"), "w");
        if (f) { fprintf(f, "%s\n", s_device_id); fclose(f); }
    }
    return s_device_id;
}

void save_config(void) {
    FILE *f = fopen(jf_data_path("jellyfin_config.txt"), "w");
    if (f) {
        fprintf(f, "%s\n%s\n%s\n%s\n", g_server, g_username, g_token, g_userid);
        fclose(f);
    }
}

int load_config(void) {
    FILE *f = fopen(jf_data_path("jellyfin_config.txt"), "r");
    if (!f) return 0;
    JFSavedSession session;
    bool valid = jf_read_session(f, &session);
    fclose(f);
    if (!valid) {
        g_token[0] = g_userid[0] = '\0';
        return 0;
    }
    strcpy(g_server, session.server);
    strcpy(g_username, session.username);
    strcpy(g_token, session.token);
    strcpy(g_userid, session.userid);
    return 1;
}

void jellyfin_logout(void) {
    // Best-effort: tell the server to invalidate this access token.  Ignore the
    // result — we clear local state regardless so the user is always logged out.
    //
    // Skipped when the token is ALREADY known dead (g_auth_expired): the server
    // has revoked it, so the call can only fail, and logging out is exactly the
    // moment the user is least able to wait for a network round trip that
    // cannot succeed.
    if (g_server[0] && g_token[0] && !g_auth_expired) {
        char url[512];
        snprintf(url, sizeof(url), "%s/Sessions/Logout", g_server);
        http_request(1, url, "{}", g_token, responseBuffer, RESPONSE_SIZE);
    }

    // Clear in-memory credentials.  Keep g_server so the user only re-enters
    // their username and password, not the server URL.
    g_token[0]    = '\0';
    g_userid[0]   = '\0';
    g_username[0] = '\0';

    // Remove the saved config so a restart does not auto-login.
    remove(jf_data_path("jellyfin_config.txt"));

    // The dead-token flag belongs to the session that just ended; leaving it
    // set would send the NEXT login straight back to the login screen.
    g_auth_expired = false;
}

// The saved session is dead server-side (a 401 came back on a request that
// carried our token — http.cpp sets g_auth_expired).  Jellyfin revokes a
// device's token whenever the same DeviceId authenticates again, so this
// happens for ordinary reasons: signing in from another client that reuses
// the id, a server restore, an admin revoking sessions.
//
// Until now nothing consumed that flag, and the symptom was silent and
// baffling: every API call 401s, so the library comes back EMPTY while the
// same account on a PC shows everything, and no amount of navigating helps
// because the app never re-authenticates.  Treat it as what it is — a logged
// out session — and send the user back to the login screen.
void jellyfin_session_expired(void) {
    g_auth_expired = false;
    g_token[0]  = '\0';
    g_userid[0] = '\0';
    remove(jf_data_path("jellyfin_config.txt"));

    drawHeader();
    drawTTF(40, 96,  TR("Session expired"), 20, XMB_TEXT);
    drawTTF(40, 128,
            TR("The server rejected this device's saved login, so the library "
               "could not be read."), 14, XMB_TEXT_DIM);
    drawTTF(40, 152,
            TR("This usually means another client signed in and took over the "
               "session."), 14, XMB_TEXT_DIM);
    drawTTF(40, 188, TR("X: sign in again"), 16, XMB_ACCENT);
    flip();

    init_btns();
    while (running) {
        sysUtilCheckCallback();
        poll_buttons();
        if (BTN_PRESSED(cross) || BTN_PRESSED(circle)) return;
    }
}

static void trim(char *s) {
    int len = strlen(s);
    while (len > 0 && (s[len-1]==' '||s[len-1]=='\n'||s[len-1]=='\r'||s[len-1]=='\t'))
        s[--len] = '\0';
    char *p = s;
    while (*p==' '||*p=='\n'||*p=='\r'||*p=='\t') p++;
    if (p != s) memmove(s, p, strlen(p)+1);
}

// --- the sign-in screens ---------------------------------------------------
//
// Centred and scaled with the screen (authored at 1280x720, like the XMB), in
// the style of a TV app's sign-in: a heading, a line or two of instructions,
// and the code large in the middle.

// One line of text centred on the screen at y (720p units).
static void ctext(int y720, const char *s, float px720, u32 col, bool bold = false) {
    const float px = UIS_TF(px720);
    const int w = ttf_text_width(s, px, bold);
    drawTTF((u32)(((int)display_width - w) / 2), (u32)(XMB_OY + UIS_H(y720)), s, px, col, bold);
}

// The Quick Connect code, one character to a box, with an accent underline.
static void draw_code_boxes(const char *code, int y720) {
    int n = (int)strlen(code);
    if (n > 10) n = 10;
    const int bw = UIS_W(66), bh = UIS_H(86), gap = UIS_W(12);
    const int total = n * bw + (n - 1) * gap;
    int x = ((int)display_width - total) / 2;
    const int y = XMB_OY + UIS_H(y720);
    const float px = UIS_TF(54.0f);
    for (int i = 0; i < n; i++, x += bw + gap) {
        drawRect((u32)x, (u32)y, (u32)bw, (u32)bh, XMB_PANEL);
        drawRect((u32)x, (u32)(y + bh - UIS_H(4)), (u32)bw, (u32)UIS_H(4), XMB_ACCENT);
        const char ch[2] = { code[i], '\0' };
        const int cw = ttf_text_width(ch, px, true);
        drawTTF((u32)(x + (bw - cw) / 2), (u32)(y + (bh - (int)px) / 2 - UIS_H(4)), ch, px,
                XMB_TEXT, true);
    }
}

// A choice card: a button glyph and a title, with one line under it.
static void draw_choice(int x, int y, int w, int h, char glyph, const char *title,
                        const char *line) {
    drawRect((u32)x, (u32)y, (u32)w, (u32)h, XMB_PANEL);
    drawRect((u32)x, (u32)y, (u32)w, 1, XMB_HAIRLINE);
    drawRect((u32)x, (u32)(y + h - 1), (u32)w, 1, XMB_HAIRLINE);
    const int gh = UIS_H(30);
    const int gx = x + UIS_W(24);
    draw_ps_button_vcentered((u32)gx, y + UIS_H(40), glyph, gh, 255);
    const int tx = gx + ps_btn_width(glyph, gh) + UIS_W(14);
    drawTTF((u32)tx, (u32)(y + UIS_H(26)), title, UIS_TF(21.0f), XMB_TEXT, true);
    drawTTF((u32)(x + UIS_W(24)), (u32)(y + UIS_H(72)), line, UIS_TF(14.0f), XMB_TEXT_DIM);
}

// Quick Connect: the console shows a six-digit code, the user approves it from
// a signed-in Jellyfin app (Settings > Quick Connect), and the server hands
// back a token — no password typed with a gamepad.  Returns 1 signed in,
// 0 cancelled, -1 when Quick Connect is off, expired or refused
// (the caller falls back to the password prompt).
static int quick_connect_login(void) {
    char url[512], body[160], secret[96] = "", code[16] = "";

    snprintf(url, sizeof url, "%s/QuickConnect/Enabled", g_server);
    int st = http_probe_get(url, NULL, responseBuffer, RESPONSE_SIZE);
    if (st != 200 || !strstr(responseBuffer, "true")) return -1;

    snprintf(url, sizeof url, "%s/QuickConnect/Initiate", g_server);
    st = http_request(HTTP_POST, url, "{}", NULL, responseBuffer, RESPONSE_SIZE);
    if (st != 200) return -1;
    json_get_string(responseBuffer, "Secret", secret, sizeof secret);
    json_get_string(responseBuffer, "Code",   code,   sizeof code);
    if (!secret[0] || !code[0]) return -1;

    u64 last_poll = 0;
    bool approved = false;
    init_btns();
    while (running && !approved) {
        sysUtilCheckCallback();
        poll_buttons();
        if (BTN_PRESSED(circle)) return 0;

        waitflip();
        drawHeader();
        ctext(150, TR("Sign in with Quick Connect"), 34.0f, XMB_TEXT, true);
        ctext(206, TR("On your phone, tablet or computer, open Jellyfin and sign in."),
              17.0f, XMB_TEXT_DIM);
        ctext(234, TR("Go to Settings > Quick Connect and enter this code:"),
              17.0f, XMB_TEXT_DIM);
        draw_code_boxes(code, 284);
        {
            // On a computer the web client is the server's own address.
            char web[300];
            snprintf(web, sizeof web, TR("On a computer, open %s in a web browser."), g_server);
            ctext(408, web, 15.0f, XMB_TEXT_FAINT);
        }
        {
            static const char *dots[4] = { "", ".", "..", "..." };
            char wait[64];
            snprintf(wait, sizeof wait, "%s%s", TR("Waiting for approval"),
                     dots[(timing_get_us() / 450000ull) % 4]);
            const float px = UIS_TF(16.0f);
            // Centred on the whole phrase with all three dots, so it holds still.
            char full[64];
            snprintf(full, sizeof full, "%s...", TR("Waiting for approval"));
            const int w = ttf_text_width(full, px);
            drawTTF((u32)(((int)display_width - w) / 2), (u32)(XMB_OY + UIS_H(460)), wait, px,
                    XMB_ACCENT_ALT);
        }
        static const Hint h[] = {{'C', TRN("Cancel")}};
        draw_hints_bar(h, 1);
        flip();

        u64 now = timing_get_us();
        if (now - last_poll < 3000000ull) continue;
        last_poll = now;
        snprintf(url, sizeof url, "%s/QuickConnect/Connect?secret=%s", g_server, secret);
        st = http_probe_get(url, NULL, responseBuffer, RESPONSE_SIZE);
        if (st == 404 || st == 401) return -1;    // code expired or revoked: use the password prompt
        if (st == 200 && strstr(responseBuffer, "\"Authenticated\":true")) approved = true;
    }
    if (!approved) return 0;

    snprintf(url,  sizeof url,  "%s/Users/AuthenticateWithQuickConnect", g_server);
    snprintf(body, sizeof body, "{\"Secret\":\"%s\"}", secret);
    st = http_request(HTTP_POST, url, body, NULL, responseBuffer, RESPONSE_SIZE);
    if (st == 200) {
        json_get_string(responseBuffer, "AccessToken", g_token,  sizeof(g_token));
        json_get_string(responseBuffer, "Id",          g_userid, sizeof(g_userid));
        json_get_string(responseBuffer, "Name",        g_username, sizeof(g_username));
        if (g_token[0]) { save_config(); return 1; }
    }
    return -1;   // rejected: the password prompt is the way in
}

int do_login(void) {
    // Offer Quick Connect first; Square keeps the classic username/password.
    // A server with Quick Connect off skips the choice entirely.
    {
        char url[512];
        snprintf(url, sizeof url, "%s/QuickConnect/Enabled", g_server);
        int st = http_probe_get(url, NULL, responseBuffer, RESPONSE_SIZE);
        if (st == 200 && strstr(responseBuffer, "true")) {
            waitflip();
            drawHeader();
            ctext(170, TR("Sign in to Jellyfin"), 34.0f, XMB_TEXT, true);
            ctext(226, TR("Approve this PS3 from your phone, tablet or computer, or type your login."),
                  17.0f, XMB_TEXT_DIM);
            {
                const int cw = UIS_W(380), ch = UIS_H(108), gap = UIS_W(28);
                const int x0 = ((int)display_width - (2 * cw + gap)) / 2;
                const int y0 = XMB_OY + UIS_H(290);
                draw_choice(x0, y0, cw, ch, 'X', TR("Quick Connect"),
                            TR("Enter a code on another device"));
                draw_choice(x0 + cw + gap, y0, cw, ch, 'S', TR("Username & password"),
                            TR("Type them on the keyboard"));
            }
            static const Hint h[] = {{'X', TRN("Quick Connect")}, {'S', TRN("Username & password")}};
            draw_hints_bar(h, 2);
            flip();
            init_btns();
            int pick = 0;
            while (running && !pick) {
                sysUtilCheckCallback();
                poll_buttons();
                if (BTN_PRESSED(cross))  pick = 1;
                if (BTN_PRESSED(square)) pick = 2;
                if (BTN_PRESSED(circle)) return 0;
            }
            if (pick == 1) {
                int r = quick_connect_login();
                if (r == 1) return 1;
                if (r == 0) return 0;
                // -1: fell over mid-way; use the password prompt below.
            }
        }
    }
    char password[64] = "";
    if (get_input(g_username, sizeof(g_username), TR("Username"), false) != 1) return 0;
    if (get_input(password,   sizeof(password),   TR("Password"), true)  != 1) return 0;
    trim(g_username);

    drawHeader();
    drawTTF(40, 96, TR("Signing in..."), 18, XMB_TEXT);
    {
        char line[320];
        snprintf(line, sizeof(line), "%s  \xC2\xB7  %s", g_username, g_server);
        drawTTF(40, 128, line, 14, XMB_TEXT_DIM);
    }
    flip();

    char url[512], body[6 * (sizeof(g_username) + sizeof(password)) + 32];
    snprintf(url,  sizeof(url),  "%s/Users/AuthenticateByName", g_server);
    if (!jf_build_login_body(g_username, password, body, sizeof(body))) return 0;

    {
        FILE *dbg = fopen("/dev_hdd0/tmp/jf_debug.txt", "w");
        if (dbg) {
            fprintf(dbg, "URL: %s\nBody(%d): <credentials redacted>\n",
                    url, (int)strlen(body));
            fclose(dbg);
        }
    }

    int status = http_request(1, url, body, NULL, responseBuffer, RESPONSE_SIZE);

    if (status == 200) {
        json_get_string(responseBuffer, "AccessToken", g_token,  sizeof(g_token));
        json_get_string(responseBuffer, "Id",          g_userid, sizeof(g_userid));
        if (g_token[0]) { save_config(); return 1; }
    }

    {
        FILE *dbg = fopen("/dev_hdd0/tmp/jf_debug.txt", "a");
        if (dbg) {
            fprintf(dbg, "Status: %d\nResponse(%d): %s\n",
                    status, (int)strlen(responseBuffer), responseBuffer);
            fclose(dbg);
        }
    }

    drawHeader();
    drawTTF(40, 96, TR("Couldn't sign in"), 22, XMB_TEXT, true);
    {
        const char *why;
        if      (status == 401) why = TR("Wrong username or password.");
        else if (status == 404) why = TR("Wrong server URL or path.");
        else if (status == 400) why = TR("Bad request - check your credentials.");
        else if (status ==  -1) why = TR("Could not reach the server.");
        else                    why = TR("The server returned an unexpected error.");
        char line[96];
        snprintf(line, sizeof(line), "%s  (status %d)", why, status);
        drawTTF(40, 136, line, 16, XMB_TEXT_DIM);
    }
    if (responseBuffer[0]) {
        char snippet[64]; snprintf(snippet, sizeof(snippet), "%.60s", responseBuffer);
        drawTTF(40, 164, snippet, 13, XMB_TEXT_FAINT);
    }
    {
        static const Hint h[] = {{'X',TRN("Try again")}};
        draw_hints_bar(h, 1);
    }
    flip();

    init_btns();
    while (running) {
        sysUtilCheckCallback();
        poll_buttons();
        if (BTN_PRESSED(cross)) return 0;
    }
    return 0;
}

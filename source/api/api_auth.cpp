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
#include "plog.h"
#include "login_store.h"
#include "user_prefs.h"

volatile bool g_auth_expired = false;

// --- where the sign-in is kept -----------------------------------------------
//
// Each PS3 user keeps their own sign-in as PS3 save data (login_store.h): the
// system files it under whoever is signed in to the console.
//
// Settings > System > Share Sign-in lends a sign-in to the PS3 users who have
// none of their own: it is copied to jellyfin_shared_login.txt, and a PS3 user
// whose own save is signed out borrows it.  A user signed in to their own
// account never sees it.  A borrower's Log Out only stops borrowing (the
// owner's session stays alive on the server); the owner's Log Out, or turning
// the setting off, ends the share.
//
// If the system will not read or write save data, everyone shares the one
// sign-in in jellyfin_config.txt, as every version before this one did.
//
// Every sign-in carries the DeviceId its token was issued under.  Jellyfin
// keys device sessions on that id, and signing in again on an id revokes the
// token issued on it before, so an id belongs to one account: a borrower uses
// the owner's id with the owner's token, and its own id for its own sign-in.

#define CONFIG_FILE "jellyfin_config.txt"
#define DEVICE_FILE "jellyfin_device_id.txt"
#define SERVER_FILE "jellyfin_server.txt"
#define SHARED_FILE "jellyfin_shared_login.txt"

static bool s_store_ok = true;     // false once the system refused save data
static bool s_loaded   = false;
static bool s_borrowed = false;    // using another PS3 user's shared sign-in
static char s_device_id[24]  = ""; // the id requests carry
static char s_own_device[24] = ""; // this PS3 user's own, while borrowing
static char s_share_token[256] = "";   // the shared sign-in's token ("" none)

struct Login { char server[256], user[64], token[256], uid[64], dev[24]; };

static void new_device_id(char *out, size_t cap) {
    const u32 a = (u32)time(NULL), b = (u32)timing_get_us();
    snprintf(out, cap, "ps3-%08x%08x", a ^ 0x9e3779b9u, b);
}

// One line of text into out; text moves past it.
static void take_line(const char **text, char *out, size_t cap) {
    const char *p = *text;
    const size_t n = strcspn(p, "\r\n");
    snprintf(out, cap, "%.*s", (int)n, p);
    p += n;
    if (*p == '\r') p++;
    if (*p == '\n') p++;
    *text = p;
}

static void parse_login(const char *text, Login *l) {
    take_line(&text, l->server, sizeof l->server);
    take_line(&text, l->user,   sizeof l->user);
    take_line(&text, l->token,  sizeof l->token);
    take_line(&text, l->uid,    sizeof l->uid);
    take_line(&text, l->dev,    sizeof l->dev);
}

static void login_text(char *out, size_t cap, bool signed_in, const char *dev) {
    snprintf(out, cap, "%s\n%s\n%s\n%s\n%s\n", g_server,
             signed_in ? g_username : "", signed_in ? g_token : "",
             signed_in ? g_userid : "", dev);
}

static void use_login(const Login *l) {
    if (l->server[0]) snprintf(g_server, sizeof g_server, "%s", l->server);
    snprintf(g_username, sizeof g_username, "%s", l->user);
    snprintf(g_token,    sizeof g_token,    "%s", l->token);
    snprintf(g_userid,   sizeof g_userid,   "%s", l->uid);
    if (!g_token[0] || !g_userid[0]) g_token[0] = g_userid[0] = '\0';
}

// --- the files ---------------------------------------------------------------

static void file_read_device_id(void) {
    FILE *f = fopen(jf_data_path(DEVICE_FILE), "r");
    if (!f) return;
    if (fscanf(f, "%23s", s_device_id) != 1) s_device_id[0] = '\0';
    fclose(f);
}

// The sign-in every version before saves used, into the globals.
static bool file_load(void) {
    FILE *f = fopen(jf_data_path(CONFIG_FILE), "r");
    if (!f) return false;
    JFSavedSession session;
    const bool valid = jf_read_session(f, &session);
    fclose(f);
    if (!valid) return false;
    strcpy(g_server, session.server);
    strcpy(g_username, session.username);
    strcpy(g_token, session.token);
    strcpy(g_userid, session.userid);
    return true;
}

static void file_save(void) {
    FILE *f = fopen(jf_data_path(CONFIG_FILE), "w");
    if (f) {
        fprintf(f, "%s\n%s\n%s\n%s\n", g_server, g_username, g_token, g_userid);
        fclose(f);
    }
}

static bool shared_read(Login *l) {
    char text[1024] = "";
    FILE *f = fopen(jf_data_path(SHARED_FILE), "r");
    if (!f) return false;
    const size_t n = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    text[n] = '\0';
    memset(l, 0, sizeof *l);
    parse_login(text, l);
    return l->server[0] && l->token[0] && l->uid[0] && l->dev[0];
}

static void shared_end(void) {
    remove(jf_data_path(SHARED_FILE));
    s_share_token[0] = '\0';
}

// --- the save ------------------------------------------------------------------

static bool store_write(bool signed_in, const char *dev) {
    char text[1024], sub[128], detail[512];
    login_text(text, sizeof text, signed_in, dev);
    // A borrower's own save holds no sign-in, but it is using one: say whose.
    if (signed_in && g_username[0])       snprintf(sub, sizeof sub, TR("Signed in as %s"), g_username);
    else if (s_borrowed && g_username[0]) snprintf(sub, sizeof sub, TR("Signed in as %s (shared)"), g_username);
    else                                  snprintf(sub, sizeof sub, "%s", TR("Not signed in"));
    snprintf(detail, sizeof detail, "%s\n%s\n\n%s", TR("Your Jellyfin sign-in for this PS3 user."),
             g_server, TR("Developed by jw-notes, based on JellyFin-PS3 by vortigauntlet."));
    static char prefs[16 * 1024];
    const size_t prefs_len = user_prefs_bundle(prefs, sizeof prefs);
    if (login_store_save(text, prefs_len ? prefs : NULL, prefs_len, sub, detail)) return true;
    s_store_ok = false;              // from here on, the shared files
    return false;
}

// This PS3 user's save into the globals.  The first time a user has none, the
// sign-in from before saves were used becomes theirs, with its device id:
// whoever opens this version first keeps the old sign-in, and the others sign
// in once (server filled in) or borrow a shared one.
static void store_load(void) {
    char text[1024];
    bool found = false;
    if (!login_store_load(text, sizeof text, &found)) {
        s_store_ok = false;
        return;
    }
    if (!found) {
        file_read_device_id();
        const bool had = file_load();
        if (!had) {
            g_token[0] = g_userid[0] = '\0';
            s_device_id[0] = '\0';
        }
        if (!s_device_id[0]) new_device_id(s_device_id, sizeof s_device_id);
        // The old copy goes only once the save holds it.
        if (store_write(had, s_device_id) && had) {
            remove(jf_data_path(CONFIG_FILE));
            plog("login save: took over the old sign-in");
        }
        return;
    }
    Login l;
    memset(&l, 0, sizeof l);
    parse_login(text, &l);
    use_login(&l);
    snprintf(s_device_id, sizeof s_device_id, "%s", l.dev);
    if (!s_device_id[0]) {
        new_device_id(s_device_id, sizeof s_device_id);
        store_write(g_token[0] != 0, s_device_id);
    }
}

// --- the app's side --------------------------------------------------------------

static void load_once(void) {
    if (s_loaded) return;
    s_loaded = true;
    store_load();
    if (!s_store_ok) {
        // No save data: the one sign-in for everyone.
        s_device_id[0] = '\0';
        file_read_device_id();
        if (!file_load()) g_token[0] = g_userid[0] = '\0';
        if (!s_device_id[0]) {
            new_device_id(s_device_id, sizeof s_device_id);
            FILE *f = fopen(jf_data_path(DEVICE_FILE), "w");
            if (f) { fprintf(f, "%s\n", s_device_id); fclose(f); }
        }
    } else {
        Login sh;
        if (shared_read(&sh)) {
            snprintf(s_share_token, sizeof s_share_token, "%s", sh.token);
            if (!g_token[0]) {
                // Signed out here: borrow the shared sign-in.
                use_login(&sh);
                snprintf(s_own_device, sizeof s_own_device, "%s", s_device_id);
                snprintf(s_device_id, sizeof s_device_id, "%s", sh.dev);
                s_borrowed = g_token[0] != 0;
                if (!s_borrowed) snprintf(s_device_id, sizeof s_device_id, "%s", s_own_device);
            }
        }
    }
    {
        char line[96];
        snprintf(line, sizeof line, "login: %s%s%s", s_store_ok ? "save data" : "shared files",
                 s_borrowed ? ", borrowing the shared sign-in" : "",
                 s_share_token[0] && !s_borrowed && !strcmp(s_share_token, g_token) ? ", sharing it" : "");
        plog(line);
    }
    // Signed out: start from the server the other PS3 users use.
    if (!g_server[0]) {
        FILE *f = fopen(jf_data_path(SERVER_FILE), "r");
        if (f) {
            char s[256] = "";
            if (fgets(s, sizeof s, f)) {
                s[strcspn(s, "\r\n")] = '\0';
                snprintf(g_server, sizeof g_server, "%s", s);
            }
            fclose(f);
        }
    }
}

// Read by every request's header, on any thread.  The sign-in is read once,
// on the main thread, by load_config() at start-up; the only requests before
// that are the update check's to GitHub, which the id means nothing to.
const char *jf_device_id(void) {
    return s_device_id[0] ? s_device_id : "ps3";
}

bool jf_login_borrowed(void) { return s_borrowed; }

// The save written again as it stands: the sign-in (a borrower's own, signed
// out) and the PS3 user's settings with it.  user_prefs_save() calls this.
void jf_login_resave(void) {
    if (!s_loaded || !s_store_ok) return;
    if (s_borrowed) store_write(false, s_own_device);
    else            store_write(g_token[0] != 0, s_device_id);
}

bool jf_login_shared(void) {
    return s_store_ok && !s_borrowed && g_token[0] && s_share_token[0] &&
           strcmp(s_share_token, g_token) == 0;
}

// One account is shared on a console at a time: while another PS3 user's is,
// this one cannot be (its borrowers would be signed in to two accounts on
// one device id).
bool jf_login_share_taken(void) {
    return s_store_ok && !s_borrowed && s_share_token[0] && !jf_login_shared();
}

// Ends the account another PS3 user shared.  Anyone signed in to their own
// account can: if the PS3 user who shared it is deleted (and their save with
// it), nobody else could ever turn it off.
void jf_login_end_other_share(void) {
    if (!jf_login_share_taken()) return;
    plog("login: ended another account's share");
    shared_end();
}

void jf_login_set_shared(bool on) {
    load_once();
    if (!s_store_ok || s_borrowed || !g_token[0] || on == jf_login_shared()) return;
    if (on && jf_login_share_taken()) return;
    plog(on ? "login: sharing turned on" : "login: sharing turned off");
    if (!on) { shared_end(); return; }
    char text[1024];
    login_text(text, sizeof text, true, s_device_id);
    FILE *f = fopen(jf_data_path(SHARED_FILE), "w");
    if (!f) return;
    fputs(text, f);
    fclose(f);
    snprintf(s_share_token, sizeof s_share_token, "%s", g_token);
}

void save_config(void) {
    load_once();
    if (!s_store_ok || !store_write(true, s_device_id)) file_save();
    FILE *f = fopen(jf_data_path(SERVER_FILE), "w");
    if (f) { fprintf(f, "%s\n", g_server); fclose(f); }
}

// Before the session's token is cleared (Log Out, or the server refused it):
// a borrower goes back to its own id, and a shared session that is over ends
// the share.  `dead` is a token the server no longer accepts.
static void leaving_session(bool dead) {
    if (s_borrowed) {
        if (dead) shared_end();
        s_borrowed = false;
        snprintf(s_device_id, sizeof s_device_id, "%s", s_own_device);
        return;
    }
    if (jf_login_shared()) shared_end();
}

// Signed out: the server and the device id stay, the token goes.
static void remove_config(void) {
    load_once();
    if (!s_store_ok || !store_write(false, s_device_id)) remove(jf_data_path(CONFIG_FILE));
}

int load_config(void) {
    load_once();
    return g_token[0] ? 1 : 0;
}

void jellyfin_logout(void) {
    // Best-effort: tell the server to invalidate this access token.  Ignore the
    // result — we clear local state regardless so the user is always logged out.
    //
    // Skipped when the token is ALREADY known dead (g_auth_expired): the server
    // has revoked it, so the call can only fail, and logging out is exactly the
    // moment the user is least able to wait for a network round trip that
    // cannot succeed.
    // A borrowed sign-in is the owner's: leave its session alone.
    if (!s_borrowed && g_server[0] && g_token[0] && !g_auth_expired) {
        char url[512];
        snprintf(url, sizeof(url), "%s/Sessions/Logout", g_server);
        http_request(1, url, "{}", g_token, responseBuffer, RESPONSE_SIZE);
    }

    leaving_session(g_auth_expired);

    // Clear in-memory credentials.  Keep g_server so the user only re-enters
    // their username and password, not the server URL.
    g_token[0]    = '\0';
    g_userid[0]   = '\0';
    g_username[0] = '\0';

    // Remove the saved config so a restart does not auto-login.
    remove_config();

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
    leaving_session(true);
    g_auth_expired = false;
    g_token[0]  = '\0';
    g_userid[0] = '\0';
    remove_config();

    // Drawn every frame while it waits, as every screen must be: the PS
    // button's menu draws over the app's frames, and a screen that stops
    // flipping froze the whole console when it was opened (2026-10-22).
    init_btns();
    while (running) {
        sysUtilCheckCallback();
        poll_buttons();
        if (BTN_PRESSED(cross) || BTN_PRESSED(circle)) return;
        waitflip();
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

// The choice between Quick Connect (X) and the keyboard (Square): 1 or 2, or
// 0 for O (back to the server address).
static int login_choice(void) {
    init_btns();
    // Drawn every frame while it waits (see jellyfin_session_expired).
    while (running) {
        sysUtilCheckCallback();
        poll_buttons();
        if (BTN_PRESSED(cross))  return 1;
        if (BTN_PRESSED(square)) return 2;
        if (BTN_PRESSED(circle)) return 0;
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
    }
    return 0;
}

static int password_login(void);

// 1 signed in, 0 the server refused or could not be reached (said on screen),
// -1 the user backed out to the server address.  Backing out of Quick Connect
// or the keyboard comes back to the choice, not out of sign-in.
int do_login(void) {
    char url[512];
    snprintf(url, sizeof url, "%s/QuickConnect/Enabled", g_server);
    const int st = http_probe_get(url, NULL, responseBuffer, RESPONSE_SIZE);
    const bool qc = st == 200 && strstr(responseBuffer, "true");
    while (running) {
        int pick = 2;
        if (qc) {
            pick = login_choice();
            if (pick == 0) return -1;
        }
        if (pick == 1) {
            const int r = quick_connect_login();
            if (r == 1) return 1;
            if (r == 0) continue;            // cancelled: the choice again
            // -1: Quick Connect fell over mid-way; the keyboard instead.
        }
        const int r = password_login();
        if (r != -1) return r;
        if (!qc) return -1;                  // no choice to go back to
    }
    return -1;
}

// Username and password on the keyboard: 1 signed in, 0 refused (said on
// screen), -1 backed out of the keyboard.
static int password_login(void) {
    char password[64] = "";
    if (get_input(g_username, sizeof(g_username), TR("Username"), false) != 1) return -1;
    if (get_input(password,   sizeof(password),   TR("Password"), true)  != 1) return -1;
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

    const char *why;
    if      (status == 401) why = TR("Wrong username or password.");
    else if (status == 404) why = TR("Wrong server URL or path.");
    else if (status == 400) why = TR("Bad request - check your credentials.");
    else if (status ==  -1) why = TR("Could not reach the server.");
    else                    why = TR("The server returned an unexpected error.");
    char line[96], snippet[64];
    snprintf(line, sizeof(line), "%s  (status %d)", why, status);
    snprintf(snippet, sizeof(snippet), "%.60s", responseBuffer);

    // Drawn every frame while it waits (see jellyfin_session_expired).
    init_btns();
    while (running) {
        sysUtilCheckCallback();
        poll_buttons();
        if (BTN_PRESSED(cross)) return 0;
        waitflip();
        drawHeader();
        drawTTF(40, 96, TR("Couldn't sign in"), 22, XMB_TEXT, true);
        drawTTF(40, 136, line, 16, XMB_TEXT_DIM);
        if (snippet[0]) drawTTF(40, 164, snippet, 13, XMB_TEXT_FAINT);
        static const Hint h[] = {{'X',TRN("Try again")}};
        draw_hints_bar(h, 1);
        flip();
    }
    return 0;
}

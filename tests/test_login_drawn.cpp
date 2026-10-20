// Drive the drawn login keyboard (ui_osk_login.cpp) the way a pad would:
// read the key labels and the highlighted key from what it draws, steer with
// the D-pad, press Cross.  Covers the server-URL case that hung on hardware
// ("192.168.1"), passwords, cancel, the firmware fallback switch, length
// limits, and a long random-input run for memory errors under ASan.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include "ui_visuals.h"
#include "timing.h"

int get_input(char *, int, const char *, bool, bool);

Buttons btn_cur, btn_prev;
u32 running = 1;
u32 display_width = 1280, display_height = 720;

struct Pos { int x, y; };
static std::map<std::string, Pos> s_keys;     // label -> position this frame
static std::string s_sel;                     // highlighted label
static Pos s_sel_pos;
static int s_frames, s_system_calls, s_flip_balance;
static std::string s_log;

// Scripted input: one button per frame, released the next frame.
static Buttons s_next;
static bool (*s_driver)(void);

void init_btns(void) { memset(&btn_cur, 0, sizeof btn_cur); memset(&btn_prev, 0, sizeof btn_prev); }
void poll_buttons(void) {
    btn_prev = btn_cur;
    if (btn_prev.up || btn_prev.down || btn_prev.left || btn_prev.right ||
        btn_prev.cross || btn_prev.circle || btn_prev.square || btn_prev.start) {
        memset(&btn_cur, 0, sizeof btn_cur);          // release between presses
        return;
    }
    memset(&s_next, 0, sizeof s_next);
    if (s_driver && !s_driver()) running = 0;          // driver gave up: end the loop
    btn_cur = s_next;
}
void sysUtilCheckCallback(void) {}
void waitflip(void) { ++s_flip_balance; s_keys.clear(); s_sel.clear(); }
void flip(void) { --s_flip_balance; ++s_frames; }
void clearScreen(u32) {}
void rsxSync(void) {}
void wave_draw(void) {}
void xmb_draw_divider(void) {}
void drawRect(u32, u32, u32, u32, u32) {}
int  ttf_text_width(const char *s, float px, bool) { return (int)(strlen(s) * px / 2); }
void draw_hints_bar(const Hint *, int) {}
void drawTTF(u32 x, u32 y, const char *s, float, u32 c, bool) {
    if (y < XMB_CONTENT_Y + 60) return;               // prompt, brand, typed text
    s_keys[s] = Pos{(int)x, (int)y};
    if (c == XMB_KEY_LABEL_SEL) { s_sel = s; s_sel_pos = Pos{(int)x, (int)y}; }
}
void drawIcon(u32 x, u32 y, int, float, u32 c) {
    s_keys["<BS>"] = Pos{(int)x, (int)y};
    if (c == XMB_KEY_LABEL_SEL) { s_sel = "<BS>"; s_sel_pos = Pos{(int)x, (int)y}; }
}
u64 timing_get_us(void) { return (u64)s_frames * 16667; }
const char *jf_data_path(const char *name) { return name; }
void crash_log(const char *m) { s_log += m; s_log += '\n'; }
int get_input_system(char *out, int, const char *, bool, bool) {
    ++s_system_calls; strcpy(out, "sys"); return 1;
}

// Steer to a label and press it.  Returns true on the frame it is pressed.
static bool press_label(const std::string &want) {
    auto it = s_keys.find(want);
    if (it == s_keys.end()) return false;
    const Pos t = it->second;
    // Same row within a few px: the backspace icon sits lower than text labels.
    if (abs(s_sel_pos.y - t.y) > OSK_KEY_H / 2) {
        (t.y < s_sel_pos.y ? s_next.up : s_next.down) = true;
        return false;
    }
    if (s_sel != want) { (t.x < s_sel_pos.x ? s_next.left : s_next.right) = true; return false; }
    s_next.cross = true;
    return true;
}

static std::string s_text;      // what the driver still has to type
static size_t s_at;
static int s_budget;
static bool s_finish_with_start;

static bool type_driver(void) {
    if (--s_budget < 0) return false;
    if (s_keys.empty()) return true;                     // first frame not drawn yet
    if (s_at == s_text.size()) {
        if (s_finish_with_start) s_next.start = true;
        else press_label("Enter");
        return true;
    }
    const char ch = s_text[s_at];
    std::string c(1, ch);
    if (c == " ") c = "Space";
    const bool letter = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
    if (s_keys.count(c)) {
        if (press_label(c)) ++s_at;
    } else if (letter && s_keys.count(std::string(1, (char)(ch ^ 0x20)))) {
        press_label("Caps");
    } else if (s_keys.count("#+=")) {
        press_label("#+=");
    } else {
        press_label("ABC");
    }
    return true;
}

static int type(const char *text, char *out, int max_len, bool password, bool keep, bool start = false) {
    s_text = text; s_at = 0; s_budget = 20000; s_finish_with_start = start;
    s_driver = type_driver; running = 1; s_frames = 0; s_flip_balance = 0; s_log.clear();
    init_btns();
    const int r = get_input(out, max_len, "prompt", password, keep);
    assert(s_flip_balance == 0);
    return r;
}

static int s_cancel_after;
static bool cancel_driver(void) {
    if (s_keys.empty()) return true;
    if (--s_cancel_after > 0) { press_label("1"); return true; }
    s_next.circle = true;
    return true;
}

static bool random_driver(void) {
    if (--s_budget < 0) return false;
    switch (rand() % 8) {
    case 0: s_next.up = true; break;   case 1: s_next.down = true; break;
    case 2: s_next.left = true; break; case 3: s_next.right = true; break;
    case 4: case 5: s_next.cross = true; break;
    case 6: s_next.square = true; break;
    default: break;
    }
    return true;
}

int main() {
    char out[256];

    // The reported hang: the address prefix, then the whole URL.
    strcpy(out, "http://");
    assert(type("192.168.1", out, sizeof out, false, true) == 1);
    assert(strcmp(out, "http://192.168.1") == 0);
    assert(s_log.find("osk: open") != std::string::npos);
    assert(s_log.find("osk: done after") != std::string::npos);
    assert(s_log.find("192") == std::string::npos);      // never logs the text

    strcpy(out, "http://");
    assert(type("192.168.1.141:8097", out, sizeof out, false, true) == 1);
    assert(strcmp(out, "http://192.168.1.141:8097") == 0);

    out[0] = 'x';
    assert(type("https://Media.Example-Host.net/jf_1?a=b&c=~", out, sizeof out, false, false) == 1);
    assert(strcmp(out, "https://Media.Example-Host.net/jf_1?a=b&c=~") == 0);

    // Password with a space and symbols, confirmed with Start; nothing logged.
    char pw[64];
    assert(type("Pa ss!@#\"'\\", pw, sizeof pw, true, false, true) == 1);
    assert(strcmp(pw, "Pa ss!@#\"'\\") == 0);
    assert(s_log.find("Pa") == std::string::npos);

    // Too long for the buffer: stops at max_len - 1, still terminated.
    char small[8];
    assert(type("1234567890", small, sizeof small, false, false) == 1);
    assert(strcmp(small, "1234567") == 0);

    // keep_text with an unterminated buffer is cut to fit, not overrun.
    char raw[6]; memset(raw, 'z', sizeof raw);
    assert(type("", raw, sizeof raw, false, true, true) == 1);
    assert(strcmp(raw, "zzzzz") == 0);

    // Circle cancels.
    s_cancel_after = 3; s_driver = cancel_driver; running = 1; init_btns(); s_log.clear();
    assert(get_input(out, sizeof out, "p", false, false) == -1);
    assert(s_log.find("osk: cancel after") != std::string::npos);

    // Application exit while the keyboard is up.
    s_budget = 5; s_driver = random_driver; running = 1; init_btns();
    assert(get_input(out, sizeof out, "p", false, false) == -1);

    // Firmware keyboard fallback via jellyfin_sysosk.txt.
    { FILE *f = fopen("jellyfin_sysosk.txt", "w"); fputs("1\n", f); fclose(f); }
    assert(get_input(out, sizeof out, "p", false, false) == 1 && strcmp(out, "sys") == 0);
    assert(s_system_calls == 1);
    { FILE *f = fopen("jellyfin_sysosk.txt", "w"); fputs("0\n", f); fclose(f); }
    assert(type("a", out, sizeof out, false, false) == 1 && strcmp(out, "a") == 0);
    assert(s_system_calls == 1);
    remove("jellyfin_sysosk.txt");

    // Random pad input: no out-of-bounds access, the text always fits.
    srand(1234);
    for (int round = 0; round < 200; round++) {
        char buf[16];
        s_budget = 2000; s_driver = random_driver; running = 1; init_btns();
        get_input(buf, sizeof buf, "p", round & 1, false);
        assert(strlen(buf) < sizeof buf);
    }

    puts("login keyboard (drawn): URL/IP entry, symbols, caps, passwords, limits, cancel, exit, fallback and random input passed");
    return 0;
}

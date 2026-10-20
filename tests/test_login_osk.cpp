// Exercise the actual firmware-dialog adapter with simulated sysutil events.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sysutil/sysutil.h>
#include <sysutil/osk.h>
#include "input_utf.h"

int get_input_system(char *, int, const char *, bool, bool);
u32 running = 1;
static input_cb callback;
static int phase, scenario, unregisters, aborts, frames, keyboard_limit;
static u64 clock_us;
static bool password_panel;
static std::string entered, initial, logs;

void waitflip() {}
void clearScreen(u32) {}
void rsxSync() {}
void flip() { ++frames; }
void init_btns() {}
u64 timing_get_us() { clock_us += 100000; return clock_us; }
void crash_log(const char *s) { logs += s; logs += '\n'; }
int sysUtilRegisterCallback(int slot, input_cb cb, void *) {
    assert(slot == 2);
    if (scenario == 4) return -1;
    callback = cb; return 0;
}
int sysUtilUnregisterCallback(int slot) { assert(slot == 2); ++unregisters; return 0; }
int oskAddSupportLanguage(u32) { return 0; }
int oskSetKeyLayoutOption(u32) { return 0; }
int oskLoadAsync(sys_mem_container_t, oskParam *p, oskInputFieldInfo *info) {
    keyboard_limit = info->maxLength;
    password_panel = p->firstViewPanel == OSK_PANEL_TYPE_PASSWORD;
    char text[256]; assert(input_from_utf16(info->startText, 256, text, sizeof text));
    initial = text;
    return scenario == 3 ? -1 : 0;
}
int oskUnloadAsync(oskCallbackReturnParam *out) {
    assert(out->len == 255);
    out->res = scenario == 1 ? OSK_CANCELED : scenario == 2 ? OSK_NO_TEXT : OSK_OK;
    assert(input_to_utf16(entered.c_str(), out->str, 256));
    phase = 2;
    return 0;
}
int oskAbort() { ++aborts; phase = 2; return 0; }
int sysUtilCheckCallback() {
    assert(frames < 1000);
    if (scenario == 5 && phase == 0) { running = 0; return 0; }
    if (phase == 0) ++phase; // keyboard is still being typed into
    else if (phase == 1) callback(SYSUTIL_OSK_DONE, 0, NULL);
    else if (phase == 2) { phase = 3; callback(SYSUTIL_OSK_UNLOADED, 0, NULL); }
    return 0;
}
static void reset(int which, const char *text) {
    scenario = which; phase = unregisters = aborts = frames = 0;
    running = 1; entered = text; logs.clear();
}
int main() {
    char out[256] = "http://";
    reset(0, "192.168.1");
    assert(get_input_system(out, sizeof out, "Server URL", false, true) == 1);
    assert(initial == "http://" && strcmp(out, "192.168.1") == 0);
    assert(frames >= 2 && unregisters == 1 && keyboard_limit == 255);
    reset(0, "http://192.168.1.20:8096");
    assert(get_input_system(out, sizeof out, "Server URL", false, true) == 1);
    assert(strcmp(out, entered.c_str()) == 0);
    reset(1, "cancelled edits");
    assert(get_input_system(out, sizeof out, "Server URL", false, true) == -1);
    assert(strcmp(out, "http://192.168.1.20:8096") == 0);
    char secret[64] = "";
    reset(0, " secret\\\" ");
    assert(get_input_system(secret, sizeof secret, "Password", true, false) == 1);
    assert(password_panel && strcmp(secret, entered.c_str()) == 0);
    assert(logs.find(entered) == std::string::npos);
    reset(2, "");
    assert(get_input_system(secret, sizeof secret, "Password", true, false) == 1 && !secret[0]);
    for (int failure = 3; failure <= 5; ++failure) {
        reset(failure, "");
        assert(get_input_system(out, sizeof out, "Server URL", false, true) == -1);
        if (failure != 4) assert(unregisters == 1);
        if (failure == 5) assert(aborts == 1);
    }
    reset(0, "too long");
    char short_out[4] = "old";
    assert(get_input_system(short_out, sizeof short_out, "Test", false, true) == -1);
    assert(strcmp(short_out, "old") == 0);
    uint16_t wide[32]; char roundtrip[64];
    const char *unicode = "Ren\xc3\xa9 \xf0\x9f\x98\x80";
    assert(input_to_utf16(unicode, wide, 32));
    assert(input_from_utf16(wide, 32, roundtrip, sizeof roundtrip));
    assert(strcmp(unicode, roundtrip) == 0);
    assert(!input_to_utf16("\xe0\x80\x80", wide, 32));
    assert(!input_to_utf16("\xf0", wide, 32));
    uint16_t invalid[] = {0xd800, 0};
    assert(!input_from_utf16(invalid, 2, roundtrip, sizeof roundtrip));
    puts("login OSK: typing, confirmation, cancel, failures, exit, privacy and UTF bounds passed");
}

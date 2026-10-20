// Firmware keyboard for server and login entry: the fallback behind the drawn
// one (ui_osk_login.cpp), chosen with "1" in jellyfin_sysosk.txt.
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/memory.h>
#include <sysutil/osk.h>
#include <sysutil/sysutil.h>
#include "ui.h"
#include "ui_visuals.h"
#include "timing.h"
#include "input_utf.h"

extern void crash_log(const char *msg);

namespace {
const int INPUT_SLOT = 2;  // slot 0: main; slot 1: library search
const int INPUT_CHARS = 255;
static u16 s_message[128], s_initial[INPUT_CHARS + 1], s_output[INPUT_CHARS + 1];
static char s_text[INPUT_CHARS + 1];
static bool s_active = false, s_closing = false;
static int s_result = -1, s_capacity = 0;
static u64 s_close_started = 0;

static void input_frame() {
    waitflip();
    clearScreen(XMB_BG);
    rsxSync();
    flip(); // sysutil needs continuing flips to display the firmware overlay
}

static void input_callback(u64 status, u64 param, void *data) {
    (void)param; (void)data;
    if (status == SYSUTIL_OSK_DONE && !s_closing) {
        oskCallbackReturnParam ret;
        memset(&ret, 0, sizeof ret);
        memset(s_output, 0, sizeof s_output);
        ret.res = OSK_OK;
        ret.len = INPUT_CHARS;
        ret.str = s_output;
        s_closing = true;
        s_close_started = timing_get_us();
        const s32 rc = oskUnloadAsync(&ret);
        if (rc != 0) {
            crash_log("input: keyboard unload failed");
            oskAbort();
        } else if (ret.res == OSK_OK || ret.res == OSK_NO_TEXT) {
            if (ret.res == OSK_NO_TEXT) s_output[0] = 0;
            if (input_from_utf16(s_output, INPUT_CHARS + 1, s_text, s_capacity))
                s_result = 1;
            else crash_log("input: result invalid or exceeds byte capacity");
        }
        // Deliberately do not log text: this dialog also collects passwords.
        crash_log(s_result == 1 ? "input: confirmed" : "input: cancelled or failed");
    } else if (status == SYSUTIL_OSK_UNLOADED) {
        s_active = false;
        memset(s_output, 0, sizeof s_output);
        memset(s_initial, 0, sizeof s_initial);
        crash_log("input: keyboard unloaded");
    }
}
}

int get_input_system(char *out, int max_len, const char *prompt, bool is_password, bool keep_text) {
    if (!out || !prompt || max_len < 1 || max_len > INPUT_CHARS + 1 || s_active)
        return -1;
    if (!keep_text) out[0] = 0;
    else out[max_len - 1] = 0;
    if (!input_to_utf16(prompt, s_message, sizeof s_message / sizeof s_message[0]) ||
        !input_to_utf16(out, s_initial, INPUT_CHARS + 1)) return -1;
    s_result = -1;
    s_capacity = max_len;
    s_closing = false;
    s_text[0] = 0;

    oskParam p;
    memset(&p, 0, sizeof p);
    p.allowedPanels = is_password ? OSK_PANEL_TYPE_PASSWORD :
        OSK_PANEL_TYPE_ALPHABET | OSK_PANEL_TYPE_NUMERAL | OSK_PANEL_TYPE_ENGLISH;
    p.firstViewPanel = is_password ? OSK_PANEL_TYPE_PASSWORD : OSK_PANEL_TYPE_ALPHABET;
    p.prohibitFlags = OSK_PROHIBIT_RETURN;
    oskInputFieldInfo info;
    memset(&info, 0, sizeof info);
    info.message = s_message;
    info.startText = s_initial;
    info.maxLength = max_len - 1;

    if (sysUtilRegisterCallback(INPUT_SLOT, input_callback, NULL) != 0) {
        crash_log("input: callback registration failed");
        return -1;
    }
    oskAddSupportLanguage(OSK_PANEL_TYPE_ENGLISH);
    oskSetKeyLayoutOption(OSK_10KEY_PANEL | OSK_FULLKEY_PANEL);
    input_frame();
    const s32 rc = oskLoadAsync((sys_mem_container_t)0xffffffffu, &p, &info);
    if (rc != 0) {
        char b[80];
        snprintf(b, sizeof b, "input: keyboard load failed 0x%08x", (unsigned)rc);
        crash_log(b);
        sysUtilUnregisterCallback(INPUT_SLOT);
        return -1;
    }
    s_active = true;
    crash_log(is_password ? "input: system password keyboard open" : "input: system keyboard open");
    while (s_active) {
        sysUtilCheckCallback();
        if (!running && !s_closing) {
            s_result = -1;
            s_closing = true;
            s_close_started = timing_get_us();
            oskAbort();
        }
        if (s_closing && timing_get_us() - s_close_started > 5000000ull) {
            crash_log("input: keyboard unload timed out");
            s_result = -1;
            oskAbort();
            break; // static buffers remain alive if firmware unload is late
        }
        if (s_active) input_frame();
    }
    if (!s_active) sysUtilUnregisterCallback(INPUT_SLOT);
    if (!running) s_result = -1;
    if (s_result == 1) memcpy(out, s_text, strlen(s_text) + 1);
    memset(s_text, 0, sizeof s_text);
    if (!s_active) {
        memset(s_output, 0, sizeof s_output);
        memset(s_initial, 0, sizeof s_initial);
    }
    init_btns();  // don't carry the dialog's confirm button into the next screen
    return s_result;
}

// Skip Intros / Skip Credits store -- see autoskip.h.

#include "autoskip.h"
#include "i18n.h"
#include "jf_paths.h"     // jf_data_path()
#include <stdio.h>

#define INTRO_FILE   "jellyfin_skip_intro.txt"
#define CREDITS_FILE "jellyfin_skip_credits.txt"
// The single Auto Skip toggle these replaced: on meant intros went by themselves.
#define LEGACY_FILE  "jellyfin_autoskip.txt"

static SkipMode s_intro   = SKIP_BUTTON;
static SkipMode s_credits = SKIP_BUTTON;

static bool read_int(const char *name, int *v) {
    FILE *f = fopen(jf_data_path(name), "r");
    if (!f) return false;
    const bool ok = fscanf(f, "%d", v) == 1;
    fclose(f);
    return ok;
}

static void save(const char *name, SkipMode m) {
    FILE *f = fopen(jf_data_path(name), "w");
    if (!f) return;
    fprintf(f, "%d\n", (int)m);
    fclose(f);
}

static SkipMode stepped(SkipMode m, int dir) {
    return (SkipMode)(((int)m + (dir < 0 ? SKIP_MODES - 1 : 1)) % SKIP_MODES);
}

void autoskip_load(void) {
    int v = 0;
    if (read_int(INTRO_FILE, &v)) {
        if (v >= 0 && v < SKIP_MODES) s_intro = (SkipMode)v;
    } else if (read_int(LEGACY_FILE, &v) && v != 0) {
        s_intro = SKIP_AUTO;
    }
    if (read_int(CREDITS_FILE, &v) && v >= 0 && v < SKIP_MODES) s_credits = (SkipMode)v;
}

SkipMode skip_intro_mode(void)   { return s_intro; }
SkipMode skip_credits_mode(void) { return s_credits; }

void skip_intro_step(int dir)   { s_intro   = stepped(s_intro, dir);   save(INTRO_FILE, s_intro); }
void skip_credits_step(int dir) { s_credits = stepped(s_credits, dir); save(CREDITS_FILE, s_credits); }

const char *skip_mode_label(SkipMode m) {
    return m == SKIP_OFF ? TRN("Off") : m == SKIP_AUTO ? TRN("Auto") : TRN("Button");
}

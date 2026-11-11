// Player Stats Overlay toggle store — see statsovl.h.

#include "statsovl.h"
#include "jf_paths.h"     // jf_data_path()
#include <stdio.h>

#define STATSOVL_FILE "jellyfin_stats.txt"

static bool s_enabled = false;
static int  s_override = -1;      // this playback: -1 = as the setting says, else shown (1) or hidden (0)

bool statsovl_enabled(void) { return s_enabled; }

void statsovl_set_enabled(bool on) {
    s_enabled = on;
    s_override = -1;
    statsovl_save();
}

bool statsovl_shown(void) { return s_override >= 0 ? s_override != 0 : s_enabled; }

void statsovl_toggle_shown(void) { s_override = statsovl_shown() ? 0 : 1; }

void statsovl_session_end(void) { s_override = -1; }

// Missing file => disabled (the overlay is a diagnostic, not a default).
void statsovl_load(void) {
    FILE *f = fopen(jf_data_path(STATSOVL_FILE), "r");
    if (!f) return;
    int v = 0;
    if (fscanf(f, "%d", &v) == 1) s_enabled = (v != 0);
    fclose(f);
}

void statsovl_save(void) {
    FILE *f = fopen(jf_data_path(STATSOVL_FILE), "w");
    if (!f) return;
    fprintf(f, "%d\n", s_enabled ? 1 : 0);
    fclose(f);
}

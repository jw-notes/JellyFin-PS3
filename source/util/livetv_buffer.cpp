#include "livetv_buffer.h"
#include "../player/core/live_buffer.h"
#include "i18n.h"
#include "jf_paths.h"
#include <stdio.h>

static int s_step = -1;

int livetv_buffer_step_get(void) {
    if (s_step < 0) {
        s_step = LIVE_BUFFER_DEFAULT_STEP;
        FILE *f = fopen(jf_data_path("jellyfin_livetv_buffer.txt"), "r");
        if (f) {
            int value = -1;
            if (fscanf(f, "%d", &value) == 1 && value >= 0 && value < LIVE_BUFFER_STEPS) s_step = value;
            fclose(f);
        }
    }
    return s_step;
}

unsigned livetv_buffer_seconds(void) { return live_buffer_seconds(livetv_buffer_step_get()); }

const char *livetv_buffer_label(void) {
    static char b[24];
    snprintf(b, sizeof b, TR("%d sec"), (int)livetv_buffer_seconds());
    return b;
}

void livetv_buffer_step(int direction) {
    s_step = (livetv_buffer_step_get() + (direction < 0 ? LIVE_BUFFER_STEPS - 1 : 1)) % LIVE_BUFFER_STEPS;
    FILE *f = fopen(jf_data_path("jellyfin_livetv_buffer.txt"), "w");
    if (f) { fprintf(f, "%d\n", s_step); fclose(f); }
}

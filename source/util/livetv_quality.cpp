#include "livetv_quality.h"
#include "jf_paths.h"
#include <stdio.h>

static int s_quality = -1;
static const vquality_t k_quality[] = { VQ_480P, VQ_720P, VQ_1080P };
static const unsigned k_bps[] = { 1500000u, 4000000u, 10000000u };
static const char *const k_label[] = { "480p / 1.5 Mbps", "720p / 4 Mbps", "1080p / 10 Mbps" };

static int selected(void) {
    if (s_quality < 0) {
        s_quality = 1; // modest default for variable wireless throughput
        FILE *f = fopen(jf_data_path("jellyfin_livetv_quality.txt"), "r");
        if (f) {
            int value = -1;
            if (fscanf(f, "%d", &value) == 1 && value >= 0 && value < 3) s_quality = value;
            fclose(f);
        }
    }
    return s_quality;
}

vquality_t livetv_quality_get(void) { return k_quality[selected()]; }
unsigned livetv_quality_bps(void) { return k_bps[selected()]; }
const char *livetv_quality_label(void) { return k_label[selected()]; }
void livetv_quality_step(int direction) {
    s_quality = (selected() + (direction < 0 ? 2 : 1)) % 3;
    FILE *f = fopen(jf_data_path("jellyfin_livetv_quality.txt"), "w");
    if (f) { fprintf(f, "%d\n", s_quality); fclose(f); }
}

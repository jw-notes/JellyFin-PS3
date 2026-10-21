#include "livetv_quality.h"
#include "jf_paths.h"
#include <stdio.h>

// Saved as the index here; Auto came last so older saved choices keep their
// meaning.  The rows step Auto, 480p, 720p, 1080p.
static int s_quality = -1;
static const vquality_t k_quality[] = { VQ_480P, VQ_720P, VQ_1080P, VQ_AUTO };
static const unsigned k_bps[] = { 1500000u, 4000000u, 10000000u, 8000000u };
static const char *const k_label[] = { "480p / 1.5 Mbps", "720p / 4 Mbps", "1080p / 10 Mbps", "Auto" };
static const int k_order[] = { 3, 0, 1, 2 };
#define N_QUALITY 4

static int selected(void) {
    if (s_quality < 0) {
        s_quality = 3; // Auto: channels pass through when the console can play them
        FILE *f = fopen(jf_data_path("jellyfin_livetv_quality.txt"), "r");
        if (f) {
            int value = -1;
            if (fscanf(f, "%d", &value) == 1 && value >= 0 && value < N_QUALITY) s_quality = value;
            fclose(f);
        }
    }
    return s_quality;
}

vquality_t livetv_quality_get(void) { return k_quality[selected()]; }
unsigned livetv_quality_bps(void) { return k_bps[selected()]; }
const char *livetv_quality_label(void) { return k_label[selected()]; }
void livetv_quality_step(int direction) {
    int pos = 0;
    while (k_order[pos] != selected()) pos++;
    s_quality = k_order[(pos + (direction < 0 ? N_QUALITY - 1 : 1)) % N_QUALITY];
    FILE *f = fopen(jf_data_path("jellyfin_livetv_quality.txt"), "w");
    if (f) { fprintf(f, "%d\n", s_quality); fclose(f); }
}

// Pin the two contracts the player relies on: the saved live profile's rate
// matches its decoder dimensions, and changing it never changes movie quality.
#include <assert.h>
#include <stdio.h>
#include "livetv_quality.h"

const char *jf_data_path(const char *) { return "/dev/null"; }

int main() {
    vquality_set(VQ_1080P_25);
    assert(livetv_quality_get() == VQ_720P);
    const unsigned rates[] = { 4000000u, 10000000u, 1500000u };
    const unsigned widths[] = { 1280, 1920, 854 };
    const unsigned heights[] = { 720, 1080, 480 };
    for (int i = 0; i < 3; ++i) {
        u32 w, h; unsigned rate;
        vquality_params(livetv_quality_get(), false, 1920, 1080,
                        &w, &h, NULL, NULL, &rate);
        assert(rate == rates[i] && rate == livetv_quality_bps());
        assert(w == widths[i] && h == heights[i]);
        assert(vquality_get() == VQ_1080P_25);
        livetv_quality_step(1);
    }
    assert(livetv_quality_get() == VQ_720P);
    livetv_quality_step(-1);
    assert(livetv_quality_get() == VQ_480P);
    assert(vquality_get() == VQ_1080P_25);
    puts("Live TV quality: default, profile/rate alignment and movie isolation passed");
}

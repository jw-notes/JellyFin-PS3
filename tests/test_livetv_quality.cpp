// Pin the two contracts the player relies on: the saved live profile's rate
// matches its decoder dimensions, and changing it never changes movie quality.
// Auto is the default and comes first in the rows; fixed steps follow it.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "livetv_quality.h"

const char *jf_data_path(const char *) { return "/dev/null"; }

int main() {
    vquality_set(VQ_1080P_25);
    assert(livetv_quality_get() == VQ_AUTO);
    assert(livetv_quality_bps() == 4000000u);
    assert(strcmp(livetv_quality_label(), "Auto") == 0);
    // Auto takes the 720p frame, or 1080p with the 1080p toggle on.
    u32 w, h; unsigned rate;
    vquality_params(livetv_quality_get(), false, 1920, 1080, &w, &h, NULL, NULL, &rate);
    assert(w == 1280 && h == 720);
    vquality_params(livetv_quality_get(), true, 1920, 1080, &w, &h, NULL, NULL, &rate);
    assert(w == 1920 && h == 1080);

    livetv_quality_step(1);
    const unsigned rates[] = { 1500000u, 4000000u, 10000000u };
    const unsigned widths[] = { 854, 1280, 1920 };
    const unsigned heights[] = { 480, 720, 1080 };
    for (int i = 0; i < 3; ++i) {
        vquality_params(livetv_quality_get(), false, 1920, 1080,
                        &w, &h, NULL, NULL, &rate);
        assert(rate == rates[i] && rate == livetv_quality_bps());
        assert(w == widths[i] && h == heights[i]);
        assert(vquality_get() == VQ_1080P_25);
        livetv_quality_step(1);
    }
    assert(livetv_quality_get() == VQ_AUTO);
    livetv_quality_step(-1);
    assert(livetv_quality_get() == VQ_1080P);
    livetv_quality_step(1);
    livetv_quality_step(1);
    assert(livetv_quality_get() == VQ_480P);
    assert(vquality_get() == VQ_1080P_25);
    puts("Live TV quality: Auto default, order, profile/rate alignment and movie isolation passed");
}

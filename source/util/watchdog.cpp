#include <stdio.h>
#include <unistd.h>
#include <sys/thread.h>
#include <rsx/rsx.h>

#include "watchdog.h"
#include "rsxutil.h"
#include "meminfo.h"
#include "timing.h"

extern void crash_log(const char *msg);

#define WD_GPU_LABEL 250
#define WD_HIST      256        // steps remembered, indexed by sequence & 255

volatile const char *g_wd_stage = "start";
volatile u32         g_wd_beat  = 0;

static char s_hist[WD_HIST][80];
static u32 s_seq = 0;
static bool s_started = false;

void wd_stage(const char *s) {
    g_wd_stage = s;
    if (!s_started) return;
    // Sequence 0 never goes out, so a label still reading 0 means "nothing yet".
    if (++s_seq == 0) s_seq = 1;
    snprintf(s_hist[s_seq & (WD_HIST - 1)], sizeof s_hist[0], "%s", s);
    rsxSetWriteBackendLabel(context, WD_GPU_LABEL, s_seq);
}

static void wd_thread(void *arg) {
    (void)arg;
    u32 seen = g_wd_beat, frames_at_hb = seen;
    u64 since = timing_get_us(), last_hb = since;
    bool reported = false;
    for (;;) {
        usleep(250000);
        const u64 now = timing_get_us();
        const u32 beat = g_wd_beat;
        char b[256];
        if (beat != seen) {
            if (reported) {
                snprintf(b, sizeof b, "WATCHDOG: main loop moving again after %u ms",
                         (unsigned)((now - since) / 1000));
                crash_log(b);
            }
            seen = beat; since = now; reported = false;
        } else if (!reported && beat != 0 && now - since >= 2000000ULL) {
            snprintf(b, sizeof b, "WATCHDOG: main loop stuck at '%s' for %u ms (frame %u)",
                     (const char *)g_wd_stage, (unsigned)((now - since) / 1000), (unsigned)beat);
            crash_log(b);
            const u32 gpu = *(vu32 *)gcmGetLabelAddress(WD_GPU_LABEL);
            const u32 cpu = s_seq;
            const char *done = gpu ? s_hist[gpu & (WD_HIST - 1)] : NULL;
            const char *next = (gpu && gpu != cpu) ? s_hist[(gpu + 1) & (WD_HIST - 1)] : NULL;
            snprintf(b, sizeof b, "WATCHDOG: GPU finished step %u '%s'; CPU queued up to %u; "
                     "GPU stuck in '%s' (flip status %u)",
                     (unsigned)gpu, done ? done : "(none)", (unsigned)cpu,
                     gpu == cpu ? "(nothing queued: waiting on the flip itself)"
                                : next ? next : "(unknown)",
                     (unsigned)gcmGetFlipStatus());
            crash_log(b);
            // The steps around it, oldest first: 4 the GPU finished, the next 3.
            for (u32 k = 0; k < 7; k++) {
                const u32 at = gpu - 3 + k;
                if (at == 0 || at > cpu) continue;
                snprintf(b, sizeof b, "WATCHDOG:   %s %u %s", at <= gpu ? "done" : "NOT ",
                         (unsigned)at, s_hist[at & (WD_HIST - 1)]);
                crash_log(b);
            }
            reported = true;
        }
        // One line every 10 s while the loop runs: frames drawn and the step.
        if (now - last_hb >= 10000000ULL && !reported) {
            snprintf(b, sizeof b, "hb: %u frames in 10 s, at '%s'",
                     (unsigned)(beat - frames_at_hb), (const char *)g_wd_stage);
            crash_log(b);
            frames_at_hb = beat; last_hb = now;
        }
    }
}

void wd_start(void) {
    if (s_started) return;
    *(vu32 *)gcmGetLabelAddress(WD_GPU_LABEL) = 0;
    static char name[] = "jf_watchdog";
    sys_ppu_thread_t tid;
    // Above the UI's priority, so a spinning main loop cannot starve it.
    if (sysThreadCreate(&tid, wd_thread, NULL, 900, 16 * 1024, 0, name) == 0)
        s_started = true;
    else
        crash_log("WATCHDOG: thread did not start");
}

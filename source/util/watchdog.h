#pragma once
#include <ppu-types.h>

// Main-loop watchdog.  The XMB loop names each step it enters (wd_stage) and
// beats once per frame (wd_beat); a thread notices when the beat stops for 2 s
// and writes the step it stopped in to crash_log.txt.  A hard hang otherwise
// leaves only "13.10 first frame done": the per-step breadcrumbs fire on the
// first frame alone, and player_log is written by a thread that can be
// starved.  "13.5a waitflip" as the stuck step means the RSX stopped flipping.
//
// Each step also queues an RSX backend label (WD_GPU_LABEL) carrying its
// sequence number, written by the GPU once everything queued before it has
// rendered.  On a GPU hang the watchdog reads it back and names the last step
// the GPU got through, so the draw that hung is the one after it.

extern volatile const char *g_wd_stage;
extern volatile u32         g_wd_beat;

void wd_stage(const char *s);
static inline void wd_beat(void) { g_wd_beat = g_wd_beat + 1; }

void wd_start(void);

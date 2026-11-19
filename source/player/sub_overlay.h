#pragma once
#include <ppu-types.h>
#include "subtitles_pgs.h"

// -------------------------------------------------------------------------
//  Subtitle overlay
// -------------------------------------------------------------------------
//  Subtitles, text and PGS alike, are composed by the CPU into a main-RAM
//  band the width of the screen, uploaded to an RSX texture, and drawn every
//  frame as one alpha-blended GPU quad queued after the video, the same way
//  as the HUD and the stats panel.
//
//  They must not be drawn by the CPU straight into the framebuffer: the video
//  quad is still being drawn there by the GPU, so whichever lands last wins,
//  and the words flicker or never show at all.
//
//  The band is composed only when what it shows changes (a new line, a new
//  PGS display set, another colour or typeface), so a steady subtitle costs
//  a few FIFO words a frame.
//
//  The buffers are allocated by sub_overlay_alloc() at the start of every
//  playback, before the decoder and the read-ahead ring (which takes whatever
//  main memory is left), and kept for the life of the app: they are sized by
//  the display, not the film, and freeing them would leave holes later
//  allocations may not fit back into.  An allocation that fails is tried
//  again at the next playback; meanwhile subtitles are not drawn.

// Reserve the buffers.  Call at the start of a playback; a no-op once done.
void sub_overlay_alloc(void);

// A text cue (lines separated by '\n'), bottom-centred in the title-safe
// area.  NULL or "" shows nothing.
void sub_overlay_text(const char *line);

// A PGS bitmap; `gen` changes whenever the bitmap's pixels do
// (subs_pgs_generation()).  NULL shows nothing.
void sub_overlay_pgs(const PgsBitmap *bmp, u32 gen);

// Nothing to show this frame.
void sub_overlay_none(void);

// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
#pragma once
#include <ppu-types.h>

// E-AC-3 (Dolby Digital Plus) decode path -- the vendored FFmpeg AC-3 / E-AC-3 decoder (eac3_api.h).
// Lives alongside the other decoders in adec.cpp, which selects it from the stream type (adec_set_codec)
// and owns the PES queue, the PCM ring and the clock.  This module splits the elementary stream into
// access units (an independent frame and the dependent frames that extend it, as 7.1 does), decodes
// them, and hands the channels to the output stage (adec_out.h), which maps them to the port and pushes
// them at 48 kHz.
//
// Files on a drive only: a server converts E-AC-3 to AC-3 for this player.  Dolby Digital passthrough
// cannot carry it (that needs a faster HDMI link than the PS3's), so it is always decoded here.

// Opens the decoder for a ring out_channels wide (6: FL FR FC LFE SL SR; 2: folded to FL FR).
// False when out of memory.
bool adec_eac3_open(int out_channels);
void adec_eac3_close(void);

// Seek / flush: the partial-frame carry, the decoder's overlap and the resampler's history go.
void adec_eac3_reset(void);

// Decodes one PES payload (PES header already stripped by adec.cpp).  Frames may straddle PES
// boundaries, so a partial tail is carried to the next call.  Runs on the adec thread only.
void adec_eac3_decode_payload(const u8 *es, int len);

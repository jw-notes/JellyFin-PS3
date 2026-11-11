// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
//
// libavutil's float transforms (tx_float.c, which #includes tx_template.c), vendored unmodified in
// mlp/ff/libavutil/ (see eac3/PROVENANCE.md).  Its own translation unit, as in FFmpeg.
#include "mlp/ff/libavutil/internal.h"   // NULL_IF_CONFIG_SMALL, as libavutil's own build has it
#include "mlp/ff/libavutil/intmath.h"    // ff_ctz: common.h only pulls it in inside FFmpeg's own build
#include "mlp/ff/libavutil/tx_float.c"

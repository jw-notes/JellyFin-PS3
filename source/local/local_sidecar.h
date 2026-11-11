// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
#pragma once
#include <stdbool.h>
#include <stdint.h>

// -------------------------------------------------------------------------
//  Subtitle files beside a video on a drive ("Film.srt", "Film.eng.srt")
// -------------------------------------------------------------------------
//  Pure C, host-tested (tests/test_local_sidecar.c).  Which files belong to
//  a video, what to call each in the subtitle menu, and their text as UTF-8,
//  whatever the file was saved as.
// -------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

#define SIDECAR_MAX       24            // listed per video
#define SIDECAR_FILE_MAX  (1024 * 1024) // a subtitle file read whole; a bigger one is refused

// Whether `file` is a SubRip file for the video `video` (both names, not paths): "<stem>.srt" or
// "<stem>.<tag>.srt", the case of either ignored, where <stem> is the video's name without its extension.
// *tag gets what is between ("" for none); `tag` may be NULL.
bool sidecar_match(const char *video, const char *file, char *tag, int tag_cap);

// What the subtitle menu calls one: the language the tag names, and what else it says
// ("eng" -> "English", "ch-traditional" -> "Chinese - traditional", "persian" -> "Persian",
// "fr-goofy" -> "French - goofy"), the tag itself when it names no language, "Subtitles" when empty.
void sidecar_label(const char *tag, char *out, int cap);

// The file's text as UTF-8, NUL-terminated: a UTF-8 byte-order mark dropped, UTF-16 with a byte-order mark
// converted, and text that is not UTF-8 read as Windows-1252 (the usual case for an old Western file).
// Returns the length written; the output is cut on a character boundary when it does not fit.
int  sidecar_to_utf8(const uint8_t *in, int len, char *out, int cap);

#ifdef __cplusplus
}
#endif

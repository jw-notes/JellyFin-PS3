// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "mkv_demux.h"

// -------------------------------------------------------------------------
//  MP4 / QuickTime (.mp4, .m4v, .mov) demuxer
// -------------------------------------------------------------------------
//  Pure C++ over a read-at callback (no PS3 headers), host-tested by
//  tests/test_mp4_demux.cpp.  The tracks it finds are described as Matroska
//  tracks (MkvTrack, in `view`), with the codec IDs and CodecPrivate Matroska
//  would give them: H.264 as V_MPEG4/ISO/AVC with its avcC, AAC as A_AAC with
//  its AudioSpecificConfig, MP3 as A_MPEG/L3, AC-3 as A_AC3, and so on.  So
//  everything that already knows what the player can do with a Matroska track
//  (local_probe, mkv_ts) needs nothing new, and mkv_ts writes the transport
//  stream from frames this reader hands it (mkv_ts_open_source).
//
//  Covered: the moov box wherever it is (the start or the end of the file),
//  tkhd, mdhd, hdlr, an edit list's initial delay and media-time shift, and the
//  sample tables (stsd's first entry, stts, ctts, stss, stsc, stsz/stz2,
//  stco/co64).  Not covered: fragmented MP4 (moof), which is refused; more than
//  one sample description per track (the first is used).
//
//  Memory is bounded whatever the film's length: the sample tables are not
//  loaded but read from the file through a small window per table, in the
//  order playback walks them.  A two-hour film's tables come to several MB,
//  more than the player has to spare.
// -------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

#define MP4_MAX_TRACKS  16

typedef struct Mp4File Mp4File;

// Opens the file and parses its moov.  NULL on failure, with the reason in `err` (may be NULL).  The
// returned file is freed by mp4_close.
Mp4File *mp4_open(MkvReadAt rd, void *ctx, uint64_t file_size, char *err, int err_cap);
void     mp4_close(Mp4File *f);

// The tracks as Matroska tracks (number = 1..n in the order the file lists them), the duration and the
// file size.  Valid until mp4_close.  Nothing else in it is used.
const MkvFile *mp4_view(const Mp4File *f);

// ---- reading frames ------------------------------------------------------------------------------
// One reader per file: it walks the chosen tracks together, the earliest decode time first, and reads
// each sample whole into `buf` (a sample bigger than `cap` is skipped and counted).
typedef struct Mp4Reader Mp4Reader;

// `tracks` lists the track numbers to read (0 entries are ignored); at most 4.
Mp4Reader *mp4_reader_open(Mp4File *f, const int *tracks, int n_tracks, uint8_t *buf, uint32_t cap);
void       mp4_reader_close(Mp4Reader *r);
// Positions every track for `target_ns`: the video track at the sync sample at or before it, the others
// at their sample at or before that sample's time.  `video_track` 0 = no video (the others go to the
// target itself).  True on success.
bool       mp4_reader_seek(Mp4Reader *r, int video_track, uint64_t target_ns);
// Next frame: 1 with *out filled (data valid until the next call), 0 at the end, -1 on a read error.
int        mp4_reader_next(Mp4Reader *r, MkvFrame *out);
// Samples skipped because they did not fit the buffer.
uint32_t   mp4_reader_oversize(const Mp4Reader *r);

#ifdef __cplusplus
}
#endif

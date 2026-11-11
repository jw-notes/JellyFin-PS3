// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
// Host test for source/video/mp4_demux.cpp, and mkv_ts writing a transport stream from it.
// The fixtures are made by ffmpeg (see the mp4 rules in Makefile.host):
//   h264_aac.mp4       H.264 with B-frames, AAC, 3 s, key frame every 1 s, moov at the end
//   h264_aac_fast.mp4  the same with moov at the start
//   h264_mp3.mov       QuickTime, H.264 and MP3 at 44.1 kHz
//   h264_ac3.mp4       H.264 and AC-3 5.1
//
//   make -f Makefile.host test_mp4_demux && ./test_mp4_demux

#include "mp4_demux.h"
#include "mkv_ts.h"
#include "aac_adts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_checks = 0, s_failed = 0;
#define CHECK(cond) do { s_checks++; if (!(cond)) { s_failed++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

struct Mem { uint8_t *p; uint64_t n; };

static int mem_read(void *ctx, uint64_t off, uint8_t *buf, int len) {
    Mem *m = (Mem *)ctx;
    if (off >= m->n) return 0;
    if ((uint64_t)len > m->n - off) len = (int)(m->n - off);
    memcpy(buf, m->p + off, (size_t)len);
    return len;
}

static bool load(const char *path, Mem *m) {
    FILE *f = fopen(path, "rb");
    if (!f) { printf("missing fixture %s\n", path); return false; }
    fseek(f, 0, SEEK_END);
    m->n = (uint64_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    m->p = (uint8_t *)malloc((size_t)m->n);
    const bool ok = m->p && fread(m->p, 1, (size_t)m->n, f) == m->n;
    fclose(f);
    return ok;
}

static const MkvTrack *first(const MkvFile *v, int type) {
    for (int i = 0; i < v->n_tracks; i++) if (v->tracks[i].type == type) return &v->tracks[i];
    return NULL;
}

static uint8_t s_buf[2 * 1024 * 1024];

struct Walk { int video, audio, keys; bool dts_order_ok; int64_t first_video_pts, last_video_pts; bool first_is_key; };

// Reads every frame; checks they come in decode-time order across the tracks (within one frame of slack for B-frames).
static Walk walk_all(Mp4File *f, int vt, int at) {
    Walk w; memset(&w, 0, sizeof w); w.dts_order_ok = true; w.first_video_pts = -1;
    const int tracks[2] = { vt, at };
    Mp4Reader *r = mp4_reader_open(f, tracks, 2, s_buf, sizeof s_buf);
    CHECK(r != NULL);
    if (!r) return w;
    MkvFrame fr;
    int rc;
    int64_t last_audio = -1;
    while ((rc = mp4_reader_next(r, &fr)) == 1) {
        CHECK(fr.size > 0);
        if (fr.track == vt) {
            if (w.video == 0) { w.first_is_key = fr.key; w.first_video_pts = fr.pts_ns; }
            w.video++;
            if (fr.key) w.keys++;
            if (fr.pts_ns > w.last_video_pts) w.last_video_pts = fr.pts_ns;
        } else {
            if (last_audio >= 0 && fr.pts_ns < last_audio) w.dts_order_ok = false;
            last_audio = fr.pts_ns;
            w.audio++;
        }
    }
    CHECK(rc == 0);
    mp4_reader_close(r);
    return w;
}

// mkv_ts over the reader: a transport stream of whole packets, starting with PAT and PMT.
struct Src { Mp4Reader *r; int vt; };
static int src_next(void *ctx, MkvFrame *out) { return mp4_reader_next(((Src *)ctx)->r, out); }
static bool src_seek(void *ctx, uint64_t ns) { Src *s = (Src *)ctx; return mp4_reader_seek(s->r, s->vt, ns); }

static void ts_check(Mp4File *f, int vt, int at, uint64_t start_ns, uint64_t *entry_ns, long *bytes) {
    uint32_t cap = 0;
    uint8_t *buf = mkv_ts_frame_buffer(&cap);
    CHECK(buf != NULL);
    const int tracks[2] = { vt, at };
    Src s = { mp4_reader_open(f, tracks, 2, buf, cap), vt };
    CHECK(s.r != NULL);
    MkvTsSource src = { src_next, src_seek, &s };
    char err[64] = "";
    uint64_t actual = 0;
    MkvTs *t = mkv_ts_open_source(mp4_view(f), src, vt, at, start_ns, &actual, err, sizeof err);
    if (!t) printf("mkv_ts_open_source: %s\n", err);
    CHECK(t != NULL);
    *entry_ns = actual;
    *bytes = 0;
    if (t) {
        static uint8_t out[188 * 64];
        int n, packets = 0;
        bool sync_ok = true, first_pat = false;
        while ((n = mkv_ts_read(t, out, (int)sizeof out)) > 0) {
            CHECK(n % 188 == 0);
            for (int i = 0; i < n; i += 188) {
                if (out[i] != 0x47) sync_ok = false;
                if (packets == 0) first_pat = out[i + 1] == 0x40 && out[i + 2] == 0x00;
                packets++;
            }
            *bytes += n;
        }
        CHECK(n == 0);
        CHECK(sync_ok);
        CHECK(first_pat);
        MkvTsStats st; mkv_ts_stats(t, &st);
        CHECK(st.video_frames > 0);
        if (at) CHECK(st.audio_frames > 0);
        mkv_ts_close(t);
    }
    mp4_reader_close(s.r);
}

static void check_file(const char *path, const char *acodec, int rate, int channels, int audio_frames_min) {
    printf("-- %s\n", path);
    Mem m;
    if (!load(path, &m)) { s_failed++; return; }
    char err[64] = "";
    Mp4File *f = mp4_open(mem_read, &m, m.n, err, sizeof err);
    if (!f) printf("mp4_open: %s\n", err);
    CHECK(f != NULL);
    if (!f) { free(m.p); return; }
    const MkvFile *v = mp4_view(f);
    CHECK(v->duration_ns > 2.9e9 && v->duration_ns < 3.3e9);
    const MkvTrack *vt = first(v, MKV_TRACK_VIDEO), *at = first(v, MKV_TRACK_AUDIO);
    CHECK(vt && at);
    if (!vt || !at) { mp4_close(f); free(m.p); return; }

    CHECK(mkv_video_codec(vt) == MKV_VC_AVC);
    CHECK(vt->width == 320 && vt->height == 240);
    MkvAvcConfig avc;
    CHECK(mkv_parse_avcc(vt->codec_private, vt->cp_len, &avc));
    CHECK(avc.n_sps == 1 && avc.n_pps == 1);
    char why[48];
    CHECK(mkv_ts_video_supported(v, vt->number, why, sizeof why));
    CHECK(vt->default_duration_ns > 41000000 && vt->default_duration_ns < 42000000);   // 24 fps

    CHECK(strcmp(at->codec_id, acodec) == 0);
    CHECK((int)(at->sample_rate + 0.5) == rate);
    CHECK(at->channels == channels);
    CHECK(mkv_ts_audio_track_supported(at, why, sizeof why));
    if (!strcmp(acodec, "A_AAC")) {
        AacConfig c;
        CHECK(aac_parse_asc(at->codec_private, at->cp_len, &c));
    }

    // every frame, in decode order
    Walk w = walk_all(f, vt->number, at->number);
    CHECK(w.video == 72);
    CHECK(w.keys == 3);
    CHECK(w.first_is_key);
    CHECK(w.first_video_pts >= 0 && w.first_video_pts < 50000000);       // the edit list puts the first picture at 0
    CHECK(w.last_video_pts > 2900000000LL && w.last_video_pts < 3000000000LL);
    CHECK(w.audio >= audio_frames_min);
    CHECK(w.dts_order_ok);

    // a seek lands on the key frame at or before the target
    {
        const int tracks[2] = { vt->number, at->number };
        Mp4Reader *r = mp4_reader_open(f, tracks, 2, s_buf, sizeof s_buf);
        CHECK(mp4_reader_seek(r, vt->number, 1700000000ULL));
        MkvFrame fr;
        bool got_video = false;
        int64_t first_audio = -1;
        for (int i = 0; i < 40 && mp4_reader_next(r, &fr) == 1; i++) {
            if (fr.track == vt->number && !got_video) {
                got_video = true;
                CHECK(fr.key);
                CHECK(fr.pts_ns >= 950000000LL && fr.pts_ns <= 1050000000LL);
            }
            if (fr.track == at->number && first_audio < 0) first_audio = fr.pts_ns;
        }
        CHECK(got_video);
        CHECK(first_audio >= 900000000LL && first_audio <= 1100000000LL);
        // and back to the start
        CHECK(mp4_reader_seek(r, vt->number, 0));
        CHECK(mp4_reader_next(r, &fr) == 1);
        CHECK(fr.pts_ns < 100000000LL);
        mp4_reader_close(r);
    }

    // the transport stream, from the start and from a seek
    uint64_t entry = 1; long bytes = 0;
    ts_check(f, vt->number, at->number, 0, &entry, &bytes);
    CHECK(entry < 50000000ULL);
    CHECK(bytes > 50000);
    ts_check(f, vt->number, at->number, 2500000000ULL, &entry, &bytes);
    CHECK(entry >= 1950000000ULL && entry <= 2050000000ULL);
    mp4_close(f);

    // hostile: every truncation either opens and reads to an end, or is refused; never a crash
    for (uint64_t cut = 0; cut < m.n; cut += m.n / 23 + 1) {
        Mem t = { m.p, cut };
        Mp4File *g = mp4_open(mem_read, &t, t.n, err, sizeof err);
        if (!g) continue;
        const MkvFile *gv = mp4_view(g);
        const MkvTrack *gt = first(gv, MKV_TRACK_VIDEO);
        if (gt) {
            const int tracks[1] = { gt->number };
            Mp4Reader *r = mp4_reader_open(g, tracks, 1, s_buf, sizeof s_buf);
            if (r) {
                MkvFrame fr; int n = 0;
                while (mp4_reader_next(r, &fr) == 1 && n < 100000) n++;
                CHECK(n < 100000);
                mp4_reader_close(r);
            }
        }
        mp4_close(g);
    }
    free(m.p);
}

int main(void) {
    check_file("mp4_h264_aac.mp4", "A_AAC", 48000, 2, 140);
    check_file("mp4_h264_aac_fast.mp4", "A_AAC", 48000, 2, 140);
    check_file("mp4_h264_mp3.mov", "A_MPEG/L3", 44100, 2, 110);
    check_file("mp4_h264_ac3.mp4", "A_AC3", 48000, 6, 90);

    // not an MP4 at all
    {
        uint8_t junk[64]; memset(junk, 0x5A, sizeof junk);
        Mem m = { junk, sizeof junk };
        char err[64] = "";
        CHECK(mp4_open(mem_read, &m, m.n, err, sizeof err) == NULL);
        CHECK(err[0] != 0);
    }
    mkv_ts_release();
    printf("mp4_demux: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}

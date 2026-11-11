// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
// MP4 / QuickTime demuxer: see mp4_demux.h.

#include "mp4_demux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOURCC(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))
#define TAB_BYTES     4096          // one sample table's read window
#define ENTRY_MAX     8192          // a sample description read whole (avcC, esds ... are a few hundred bytes)
#define READER_TRACKS 4

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint64_t be64(const uint8_t *p) { return ((uint64_t)be32(p) << 32) | be32(p + 4); }

// ---- a sample table, read through a window -------------------------------------------------------

struct Tab {
    uint64_t off, len;              // the table's entries, in the file
    uint8_t *buf;                   // TAB_BYTES, allocated on first use
    uint64_t win;                   // byte (from off) the window starts at
    uint32_t win_len;
};

struct Mp4Trk {
    int      number, type;          // as in the view
    uint32_t ts;                    // media timescale
    int64_t  shift;                 // edit list: media time that plays at the track's start
    int64_t  delay_ns;              // edit list: an empty edit before it
    uint32_t n_samples;
    Tab      stts, ctts, stss, stsc, stsz, stco;
    uint32_t stts_n, ctts_n, stss_n, stsc_n, stco_n;
    uint32_t stsz_fixed;            // every sample this size (0: per-sample sizes)
    int      stz2_bits;             // 0: stsz (32-bit sizes); 4, 8 or 16: stz2
    bool     co64, has_ctts, has_stss;
};

struct Mp4File {
    MkvReadAt rd;
    void     *ctx;
    uint64_t  size;
    bool      err;
    uint32_t  movie_ts;
    MkvFile   view;
    Mp4Trk    trk[MP4_MAX_TRACKS];
    int       n_trk;
};

static int rd_at(Mp4File *f, uint64_t off, uint8_t *buf, int len) {
    if (len <= 0) return 0;
    const int got = f->rd(f->ctx, off, buf, len);
    if (got < 0) f->err = true;
    return got;
}

static bool rd_exact(Mp4File *f, uint64_t off, uint8_t *buf, int len) {
    return rd_at(f, off, buf, len) == len;
}

// `need` bytes of the table from byte `at`, or NULL past its end or on a read error.
static const uint8_t *tab_at(Mp4File *f, Tab *t, uint64_t at, int need) {
    if (at + (uint64_t)need > t->len) return NULL;
    if (!t->buf) {
        t->buf = (uint8_t *)malloc(TAB_BYTES);
        if (!t->buf) { f->err = true; return NULL; }
        t->win_len = 0;
    }
    if (at < t->win || at + (uint64_t)need > t->win + t->win_len) {
        uint64_t left = t->len - at;
        const int want = left < TAB_BYTES ? (int)left : TAB_BYTES;
        if (!rd_exact(f, t->off + at, t->buf, want)) { t->win_len = 0; return NULL; }
        t->win = at; t->win_len = (uint32_t)want;
    }
    return t->buf + (at - t->win);
}

static void tab_free(Tab *t) { free(t->buf); t->buf = NULL; t->win_len = 0; }

// ---- boxes --------------------------------------------------------------------------------------------

struct Box { uint32_t type; uint64_t body, end; };

// The box at `off`, which must end by `end`.  False when there is none or it is malformed.
static bool box_at(Mp4File *f, uint64_t off, uint64_t end, Box *b) {
    uint8_t h[16];
    if (off + 8 > end || !rd_exact(f, off, h, 8)) return false;
    uint64_t size = be32(h);
    b->type = be32(h + 4);
    uint64_t hdr = 8;
    if (size == 1) {
        if (off + 16 > end || !rd_exact(f, off + 8, h + 8, 8)) return false;
        size = be64(h + 8); hdr = 16;
    } else if (size == 0) {
        size = end - off;
    }
    if (size < hdr || off + size > end) return false;
    b->body = off + hdr; b->end = off + size;
    return true;
}

// The first child of type `type` in [off, end).
static bool find_child(Mp4File *f, uint64_t off, uint64_t end, uint32_t type, Box *out) {
    Box b;
    while (box_at(f, off, end, &b)) {
        if (b.type == type) { *out = b; return true; }
        off = b.end;
    }
    return false;
}

// The same, in a buffer already read.  Returns the child's body and its length.
static const uint8_t *find_in(const uint8_t *p, int len, uint32_t type, int *body_len) {
    int at = 0;
    while (at + 8 <= len) {
        uint32_t size = be32(p + at);
        const uint32_t t = be32(p + at + 4);
        if (size == 0) size = (uint32_t)(len - at);
        if (size < 8 || at + (int)size > len) return NULL;
        if (t == type) { *body_len = (int)size - 8; return p + at + 8; }
        at += (int)size;
    }
    return NULL;
}

// ---- sample descriptions ------------------------------------------------------------------------------

static void set_codec(MkvTrack *t, const char *id) { snprintf(t->codec_id, sizeof t->codec_id, "%s", id); }

static void set_private(MkvTrack *t, const uint8_t *p, int len) {
    t->cp_total = len;
    t->cp_len = len < MKV_CP_MAX ? len : MKV_CP_MAX;
    memcpy(t->codec_private, p, (size_t)t->cp_len);
}

// An MPEG-4 descriptor's length (1 to 4 bytes, 7 bits each).  -1 when it runs out.
static int desc_len(const uint8_t *p, int len, int *at) {
    int v = 0;
    for (int i = 0; i < 4; i++) {
        if (*at >= len) return -1;
        const uint8_t b = p[(*at)++];
        v = (v << 7) | (b & 0x7F);
        if (!(b & 0x80)) return v;
    }
    return v;
}

// esds: the codec (objectTypeIndication) and, for AAC, the AudioSpecificConfig.
static void parse_esds(MkvTrack *t, const uint8_t *p, int len) {
    int at = 4;                                         // version and flags
    if (at >= len || p[at++] != 0x03) return;          // ES_Descriptor
    if (desc_len(p, len, &at) < 0 || at + 3 > len) return;
    at += 2;                                            // ES_ID
    const uint8_t fl = p[at++];
    if (fl & 0x80) at += 2;                             // dependsOn_ES_ID
    if (fl & 0x40) { if (at >= len) return; at += 1 + p[at]; }   // URL
    if (fl & 0x20) at += 2;                             // OCR_ES_Id
    if (at >= len || p[at++] != 0x04) return;          // DecoderConfigDescriptor
    const int dcl = desc_len(p, len, &at);
    if (dcl < 13 || at + 13 > len) return;
    const uint8_t oti = p[at];
    const int dc_end = at + dcl < len ? at + dcl : len;
    at += 13;
    switch (oti) {
    case 0x40: case 0x66: case 0x67: case 0x68: set_codec(t, "A_AAC"); break;
    case 0x69: case 0x6B: set_codec(t, "A_MPEG/L3"); break;     // layer II files are rare; minimp3 reads both
    case 0xA5: set_codec(t, "A_AC3"); break;
    case 0xA6: set_codec(t, "A_EAC3"); break;
    case 0xA9: set_codec(t, "A_DTS"); break;
    case 0xDD: set_codec(t, "A_VORBIS"); break;
    default:   set_codec(t, "A_MP4/OTHER"); return;
    }
    if (at < dc_end && p[at] == 0x05) {                 // DecoderSpecificInfo
        at++;
        const int dl = desc_len(p, dc_end, &at);
        if (dl > 0 && at + dl <= dc_end) set_private(t, p + at, dl);
    }
}

// A video sample entry: its size and, for H.264, the avcC.
static void parse_visual(MkvTrack *t, uint32_t fmt, const uint8_t *e, int len) {
    if (len < 78) return;
    t->width = be16(e + 24); t->height = be16(e + 26);
    switch (fmt) {
    case FOURCC('a','v','c','1'): case FOURCC('a','v','c','3'): {
        set_codec(t, "V_MPEG4/ISO/AVC");
        int cl = 0;
        const uint8_t *c = find_in(e + 78, len - 78, FOURCC('a','v','c','C'), &cl);
        if (c) set_private(t, c, cl);
        break;
    }
    case FOURCC('h','v','c','1'): case FOURCC('h','e','v','1'): set_codec(t, "V_MPEGH/ISO/HEVC"); break;
    case FOURCC('m','p','4','v'): set_codec(t, "V_MPEG4/ISO/ASP"); break;
    case FOURCC('a','v','0','1'): set_codec(t, "V_AV1"); break;
    case FOURCC('v','p','0','9'): set_codec(t, "V_VP9"); break;
    case FOURCC('m','p','2','v'): case FOURCC('m','2','v','1'): set_codec(t, "V_MPEG2"); break;
    default: set_codec(t, "V_QUICKTIME"); break;
    }
}

// An audio sample entry: channels, rate and the codec, from the QuickTime version 0, 1 or 2 layout.
static void parse_audio(MkvTrack *t, uint32_t fmt, const uint8_t *e, int len) {
    if (len < 28) return;
    const uint16_t ver = be16(e + 8);
    int kids = 28;
    t->channels = be16(e + 16);
    t->bit_depth = be16(e + 18);
    t->sample_rate = (double)be32(e + 24) / 65536.0;
    if (ver == 1) kids = 44;
    else if (ver == 2 && len >= 64) {
        kids = 64;
        uint64_t bits = be64(e + 32);
        double rate; memcpy(&rate, &bits, sizeof rate);
        t->sample_rate = rate;
        t->channels = (int)be32(e + 40);
        t->bit_depth = (int)be32(e + 48);
    }
    const uint8_t *k = kids < len ? e + kids : e + len;
    const int kl = kids < len ? len - kids : 0;
    int bl = 0;
    switch (fmt) {
    case FOURCC('m','p','4','a'): {
        const uint8_t *b = find_in(k, kl, FOURCC('e','s','d','s'), &bl);
        if (!b) {                                       // QuickTime keeps it inside a 'wave' box
            int wl = 0;
            const uint8_t *w = find_in(k, kl, FOURCC('w','a','v','e'), &wl);
            if (w) b = find_in(w, wl, FOURCC('e','s','d','s'), &bl);
        }
        if (b) parse_esds(t, b, bl); else set_codec(t, "A_MP4/OTHER");
        break;
    }
    case FOURCC('.','m','p','3'): case FOURCC('m','s',0,'U'): set_codec(t, "A_MPEG/L3"); break;
    case FOURCC('a','c','-','3'): set_codec(t, "A_AC3"); break;
    case FOURCC('e','c','-','3'): set_codec(t, "A_EAC3"); break;
    case FOURCC('d','t','s','c'): case FOURCC('d','t','s','h'): case FOURCC('d','t','s','l'): set_codec(t, "A_DTS"); break;
    case FOURCC('O','p','u','s'): set_codec(t, "A_OPUS"); break;
    case FOURCC('f','L','a','C'): {
        set_codec(t, "A_FLAC");
        // dfLa: version/flags, then the metadata blocks: Matroska's CodecPrivate is "fLaC" and the blocks
        const uint8_t *b = find_in(k, kl, FOURCC('d','f','L','a'), &bl);
        if (b && bl > 4 && bl - 4 + 4 <= MKV_CP_MAX) {
            uint8_t cp[MKV_CP_MAX];
            memcpy(cp, "fLaC", 4);
            memcpy(cp + 4, b + 4, (size_t)(bl - 4));
            set_private(t, cp, bl);
        }
        break;
    }
    default: set_codec(t, "A_QUICKTIME"); break;        // PCM, ALAC ...: no decoder here
    }
}

// The first sample description of the track.
static void parse_stsd(Mp4File *f, const Box &stsd, MkvTrack *t) {
    Box e;
    if (!box_at(f, stsd.body + 8, stsd.end, &e)) return;            // version/flags, entry count, the entry
    uint64_t elen = e.end - e.body;
    if (elen > ENTRY_MAX) elen = ENTRY_MAX;
    uint8_t *buf = (uint8_t *)malloc((size_t)elen + 1);
    if (!buf) return;
    if (rd_exact(f, e.body, buf, (int)elen)) {
        if (t->type == MKV_TRACK_VIDEO) parse_visual(t, e.type, buf, (int)elen);
        else if (t->type == MKV_TRACK_AUDIO) parse_audio(t, e.type, buf, (int)elen);
        else {
            char id[16];
            snprintf(id, sizeof id, "S_MP4/%c%c%c%c", (char)(e.type >> 24), (char)(e.type >> 16), (char)(e.type >> 8), (char)e.type);
            set_codec(t, id);
        }
    }
    free(buf);
}

// ---- tracks ---------------------------------------------------------------------------------------

static void set_tab(Tab *t, uint64_t off, uint64_t end) { t->off = off; t->len = end > off ? end - off : 0; }

// A FullBox's entry count and where the entries start.
static bool counted(Mp4File *f, const Box &b, uint32_t *n, uint64_t *entries, int skip) {
    uint8_t h[12];
    if (b.end - b.body < (uint64_t)(4 + skip + 4) || !rd_exact(f, b.body, h, 4 + skip + 4)) return false;
    *n = be32(h + 4 + skip);
    *entries = b.body + 4 + (uint64_t)skip + 4;
    return true;
}

static void parse_stbl(Mp4File *f, const Box &stbl, Mp4Trk *tk, MkvTrack *t) {
    Box b;
    uint64_t at = stbl.body;
    while (box_at(f, at, stbl.end, &b)) {
        uint32_t n = 0; uint64_t e = 0;
        switch (b.type) {
        case FOURCC('s','t','s','d'): parse_stsd(f, b, t); break;
        case FOURCC('s','t','t','s'): if (counted(f, b, &n, &e, 0)) { tk->stts_n = n; set_tab(&tk->stts, e, b.end); } break;
        case FOURCC('c','t','t','s'): if (counted(f, b, &n, &e, 0)) { tk->ctts_n = n; set_tab(&tk->ctts, e, b.end); tk->has_ctts = n > 0; } break;
        case FOURCC('s','t','s','s'): if (counted(f, b, &n, &e, 0)) { tk->stss_n = n; set_tab(&tk->stss, e, b.end); tk->has_stss = true; } break;
        case FOURCC('s','t','s','c'): if (counted(f, b, &n, &e, 0)) { tk->stsc_n = n; set_tab(&tk->stsc, e, b.end); } break;
        case FOURCC('s','t','c','o'): if (counted(f, b, &n, &e, 0)) { tk->stco_n = n; set_tab(&tk->stco, e, b.end); tk->co64 = false; } break;
        case FOURCC('c','o','6','4'): if (counted(f, b, &n, &e, 0)) { tk->stco_n = n; set_tab(&tk->stco, e, b.end); tk->co64 = true; } break;
        case FOURCC('s','t','s','z'): {
            uint8_t h[12];
            if (b.end - b.body >= 12 && rd_exact(f, b.body, h, 12)) {
                tk->stsz_fixed = be32(h + 4); tk->n_samples = be32(h + 8); tk->stz2_bits = 0;
                set_tab(&tk->stsz, b.body + 12, b.end);
            }
            break;
        }
        case FOURCC('s','t','z','2'): {
            uint8_t h[12];
            if (b.end - b.body >= 12 && rd_exact(f, b.body, h, 12)) {
                tk->stsz_fixed = 0; tk->stz2_bits = h[7]; tk->n_samples = be32(h + 8);
                set_tab(&tk->stsz, b.body + 12, b.end);
            }
            break;
        }
        default: break;
        }
        at = b.end;
    }
}

static void parse_elst(Mp4File *f, const Box &elst, Mp4Trk *tk) {
    uint8_t h[8];
    if (elst.end - elst.body < 8 || !rd_exact(f, elst.body, h, 8)) return;
    const int ver = h[0];
    const uint32_t n = be32(h + 4);
    const int esz = ver == 1 ? 20 : 12;
    uint64_t at = elst.body + 8;
    for (uint32_t i = 0; i < n && i < 4 && at + (uint64_t)esz <= elst.end; i++, at += (uint64_t)esz) {
        uint8_t e[20];
        if (!rd_exact(f, at, e, esz)) return;
        const uint64_t dur = ver == 1 ? be64(e) : be32(e);
        const int64_t mt = ver == 1 ? (int64_t)be64(e + 8) : (int64_t)(int32_t)be32(e + 4);
        if (mt == -1) {                                 // an empty edit: the track starts this much later
            if (f->movie_ts) tk->delay_ns += (int64_t)(dur * 1000000000ULL / f->movie_ts);
            continue;
        }
        tk->shift = mt;
        return;
    }
}

static void parse_trak(Mp4File *f, const Box &trak) {
    if (f->n_trk >= MP4_MAX_TRACKS || f->view.n_tracks >= MKV_MAX_TRACKS) return;
    Mp4Trk *tk = &f->trk[f->n_trk];
    MkvTrack *t = &f->view.tracks[f->view.n_tracks];
    memset(tk, 0, sizeof *tk);
    memset(t, 0, sizeof *t);
    t->number = f->n_trk + 1;
    t->enabled = true;
    snprintf(t->language, sizeof t->language, "und");

    Box b, mdia, minf, stbl;
    if (find_child(f, trak.body, trak.end, FOURCC('t','k','h','d'), &b)) {
        uint8_t h[4];
        if (rd_exact(f, b.body, h, 4)) t->enabled = (h[3] & 1) != 0;
    }
    if (find_child(f, trak.body, trak.end, FOURCC('e','d','t','s'), &b)) {
        Box el;
        if (find_child(f, b.body, b.end, FOURCC('e','l','s','t'), &el)) parse_elst(f, el, tk);
    }
    if (!find_child(f, trak.body, trak.end, FOURCC('m','d','i','a'), &mdia)) return;
    if (find_child(f, mdia.body, mdia.end, FOURCC('m','d','h','d'), &b)) {
        uint8_t h[36];
        const int n = (int)((b.end - b.body) < 36 ? (b.end - b.body) : 36);
        if (n >= 24 && rd_exact(f, b.body, h, n)) {
            const bool v1 = h[0] == 1;
            tk->ts = v1 ? be32(h + 20) : be32(h + 12);
            const int lo = v1 ? 32 : 20;
            if (lo + 2 <= n) {
                const uint16_t l = be16(h + lo);
                if (l >= 0x400 && l != 0x7FFF) {      // ISO 639-2/T, three 5-bit letters
                    t->language[0] = (char)(((l >> 10) & 31) + 0x60);
                    t->language[1] = (char)(((l >> 5) & 31) + 0x60);
                    t->language[2] = (char)((l & 31) + 0x60);
                    t->language[3] = '\0';
                } else if (l == 0) {
                    snprintf(t->language, sizeof t->language, "eng");      // QuickTime's language 0 is English
                }
            }
        }
    }
    if (find_child(f, mdia.body, mdia.end, FOURCC('h','d','l','r'), &b)) {
        uint8_t h[12];
        if (b.end - b.body >= 12 && rd_exact(f, b.body, h, 12)) {
            const uint32_t ht = be32(h + 8);
            t->type = ht == FOURCC('v','i','d','e') ? MKV_TRACK_VIDEO
                    : ht == FOURCC('s','o','u','n') ? MKV_TRACK_AUDIO
                    : (ht == FOURCC('s','b','t','l') || ht == FOURCC('t','e','x','t') || ht == FOURCC('s','u','b','t')) ? MKV_TRACK_SUBTITLE
                    : 0;
        }
    }
    if (!t->type || !tk->ts) return;                   // a timecode, chapter or hint track: not listed
    if (!find_child(f, mdia.body, mdia.end, FOURCC('m','i','n','f'), &minf)) return;
    if (!find_child(f, minf.body, minf.end, FOURCC('s','t','b','l'), &stbl)) return;
    parse_stbl(f, stbl, tk, t);
    if (!tk->n_samples || !tk->stts_n || !tk->stsc_n || !tk->stco_n) return;   // nothing to play (or fragmented)

    tk->number = t->number;
    tk->type = t->type;
    // stts gives the frame duration of a constant-rate track: its first entry
    if (t->type == MKV_TRACK_VIDEO) {
        const uint8_t *e = tab_at(f, &tk->stts, 0, 8);
        if (e && be32(e + 4)) t->default_duration_ns = (uint64_t)be32(e + 4) * 1000000000ULL / tk->ts;
        tab_free(&tk->stts);
    }
    f->n_trk++;
    f->view.n_tracks++;
}

Mp4File *mp4_open(MkvReadAt rd, void *ctx, uint64_t file_size, char *err, int err_cap) {
    Mp4File *f = (Mp4File *)calloc(1, sizeof *f);
    if (!f) { if (err && err_cap > 0) snprintf(err, (size_t)err_cap, "out of memory"); return NULL; }
    f->rd = rd; f->ctx = ctx; f->size = file_size;
    f->view.rd = rd; f->view.ctx = ctx; f->view.file_size = file_size; f->view.timescale_ns = 1;
    const char *why = NULL;
    Box b, moov;
    bool have_moov = false, fragmented = false;
    uint64_t at = 0;
    while (box_at(f, at, file_size, &b)) {
        if (b.type == FOURCC('m','o','o','v')) { moov = b; have_moov = true; }
        if (b.type == FOURCC('m','o','o','f')) fragmented = true;
        at = b.end;
    }
    if (!have_moov) why = f->err ? "cannot read the file" : "no movie index (moov)";
    else {
        if (find_child(f, moov.body, moov.end, FOURCC('m','v','h','d'), &b)) {
            uint8_t h[32];
            const int n = (int)((b.end - b.body) < 32 ? (b.end - b.body) : 32);
            if (n >= 20 && rd_exact(f, b.body, h, n)) {
                const bool v1 = h[0] == 1;
                f->movie_ts = v1 ? be32(h + 20) : be32(h + 12);
                const uint64_t dur = v1 ? (n >= 32 ? be64(h + 24) : 0) : be32(h + 16);
                if (f->movie_ts && dur != 0xFFFFFFFFu) f->view.duration_ns = (double)dur * 1e9 / (double)f->movie_ts;
            }
        }
        at = moov.body;
        while (box_at(f, at, moov.end, &b)) {
            if (b.type == FOURCC('t','r','a','k')) parse_trak(f, b);
            at = b.end;
        }
        if (f->n_trk == 0) why = fragmented ? "fragmented MP4 is not supported" : "no tracks to play";
    }
    if (why) {
        if (err && err_cap > 0) snprintf(err, (size_t)err_cap, "%s", why);
        mp4_close(f);
        return NULL;
    }
    return f;
}

void mp4_close(Mp4File *f) {
    if (!f) return;
    for (int i = 0; i < f->n_trk; i++) {
        Mp4Trk *t = &f->trk[i];
        tab_free(&t->stts); tab_free(&t->ctts); tab_free(&t->stss);
        tab_free(&t->stsc); tab_free(&t->stsz); tab_free(&t->stco);
    }
    free(f);
}

const MkvFile *mp4_view(const Mp4File *f) { return &f->view; }

// ---- reading --------------------------------------------------------------------------------------

static Mp4Trk *trk_of(Mp4File *f, int number) {
    for (int i = 0; i < f->n_trk; i++) if (f->trk[i].number == number) return &f->trk[i];
    return NULL;
}

static int64_t ticks_ns(int64_t v, uint32_t ts) {
    const int64_t s = v / (int64_t)ts, r = v % (int64_t)ts;
    return s * 1000000000LL + r * 1000000000LL / (int64_t)ts;
}

static uint32_t sample_size(Mp4File *f, Mp4Trk *t, uint32_t i) {
    if (t->stsz_fixed) return t->stsz_fixed;
    const uint8_t *p;
    switch (t->stz2_bits) {
    case 0:  p = tab_at(f, &t->stsz, (uint64_t)i * 4, 4); return p ? be32(p) : 0;
    case 16: p = tab_at(f, &t->stsz, (uint64_t)i * 2, 2); return p ? be16(p) : 0;
    case 8:  p = tab_at(f, &t->stsz, i, 1);                 return p ? p[0] : 0;
    case 4:  p = tab_at(f, &t->stsz, i / 2, 1);             return p ? ((i & 1) ? (p[0] & 15) : (p[0] >> 4)) : 0;
    default: return 0;
    }
}

static uint64_t chunk_offset(Mp4File *f, Mp4Trk *t, uint32_t c) {
    const uint8_t *p = t->co64 ? tab_at(f, &t->stco, (uint64_t)c * 8, 8) : tab_at(f, &t->stco, (uint64_t)c * 4, 4);
    return p ? (t->co64 ? be64(p) : be32(p)) : 0;
}

struct Cur {
    Mp4Trk  *t;
    uint32_t i;                                          // the next sample, 0-based
    uint32_t stts_i, stts_left, stts_delta;
    int64_t  dts;                                        // media ticks
    uint32_t ctts_i, ctts_left;
    int32_t  ctts_v;
    uint32_t stsc_i, chunk, spc, chunk_left, next_first;  // next_first: the next stsc entry's first chunk (0-based)
    uint64_t off;
    uint32_t stss_i, next_sync;                          // 1-based sample number of the next sync sample
    bool     done;
};

struct Mp4Reader {
    Mp4File  *f;
    Cur       cur[READER_TRACKS];
    int       n;
    uint8_t  *buf;
    uint32_t  cap;
    uint32_t  oversize;
};

static bool stts_entry(Mp4File *f, Mp4Trk *t, uint32_t j, uint32_t *count, uint32_t *delta) {
    const uint8_t *p = tab_at(f, &t->stts, (uint64_t)j * 8, 8);
    if (!p) return false;
    *count = be32(p); *delta = be32(p + 4);
    return true;
}

static bool stsc_entry(Mp4File *f, Mp4Trk *t, uint32_t j, uint32_t *first, uint32_t *spc) {
    const uint8_t *p = tab_at(f, &t->stsc, (uint64_t)j * 12, 12);
    if (!p) return false;
    *first = be32(p) ? be32(p) - 1 : 0; *spc = be32(p + 4);
    return true;
}

static uint32_t sync_at(Mp4File *f, Mp4Trk *t, uint32_t j) {
    const uint8_t *p = tab_at(f, &t->stss, (uint64_t)j * 4, 4);
    return p ? be32(p) : 0xFFFFFFFFu;
}

// The first stss index whose sample number is >= s (1-based); stss_n when none.
static uint32_t stss_lower(Mp4File *f, Mp4Trk *t, uint32_t s) {
    uint32_t lo = 0, hi = t->stss_n;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        if (sync_at(f, t, mid) < s) lo = mid + 1; else hi = mid;
    }
    return lo;
}

// Puts the cursor on sample s.  False on a read error or a table that does not add up.
static bool cur_set(Mp4File *f, Cur *c, Mp4Trk *t, uint32_t s) {
    memset(c, 0, sizeof *c);
    c->t = t;
    c->i = s;
    if (s >= t->n_samples) { c->done = true; return true; }
    // decode time
    uint32_t base = 0; int64_t dts = 0;
    for (uint32_t j = 0; j < t->stts_n; j++) {
        uint32_t cnt, d;
        if (!stts_entry(f, t, j, &cnt, &d)) return false;
        if (s < base + cnt) {
            c->stts_i = j; c->stts_left = base + cnt - s; c->stts_delta = d;
            c->dts = dts + (int64_t)(s - base) * d;
            break;
        }
        base += cnt; dts += (int64_t)cnt * d;
        if (j + 1 == t->stts_n) { c->stts_i = j; c->stts_left = 0xFFFFFFFFu; c->stts_delta = d; c->dts = dts + (int64_t)(s - base) * d; }
    }
    // composition offset
    if (t->has_ctts) {
        base = 0;
        for (uint32_t j = 0; j < t->ctts_n; j++) {
            const uint8_t *p = tab_at(f, &t->ctts, (uint64_t)j * 8, 8);
            if (!p) return false;
            const uint32_t cnt = be32(p);
            if (s < base + cnt) { c->ctts_i = j; c->ctts_left = base + cnt - s; c->ctts_v = (int32_t)be32(p + 4); break; }
            base += cnt;
        }
    }
    // the chunk it is in
    base = 0;
    for (uint32_t j = 0; j < t->stsc_n; j++) {
        uint32_t first, spc, nfirst = t->stco_n, nspc;
        if (!stsc_entry(f, t, j, &first, &spc)) return false;
        if (j + 1 < t->stsc_n && !stsc_entry(f, t, j + 1, &nfirst, &nspc)) return false;
        if (nfirst > t->stco_n) nfirst = t->stco_n;
        const uint64_t chunks = nfirst > first ? nfirst - first : 0;
        const uint64_t covers = chunks * spc;
        if (spc && (uint64_t)s < base + covers) {
            const uint32_t k = (s - base) / spc, in = (s - base) % spc;
            c->stsc_i = j; c->spc = spc; c->next_first = nfirst;
            c->chunk = first + k; c->chunk_left = spc - in;
            c->off = chunk_offset(f, t, c->chunk);
            for (uint32_t q = s - in; q < s; q++) c->off += sample_size(f, t, q);
            break;
        }
        base += (uint32_t)covers;
        if (j + 1 == t->stsc_n) { c->done = true; return true; }      // the table ends before the sample
    }
    // the next sync sample
    if (t->has_stss) {
        c->stss_i = stss_lower(f, t, s + 1);
        c->next_sync = c->stss_i < t->stss_n ? sync_at(f, t, c->stss_i) : 0xFFFFFFFFu;
    }
    return !f->err;
}

static int64_t cur_dts_ns(const Cur *c) { return ticks_ns(c->dts - c->t->shift, c->t->ts) + c->t->delay_ns; }
static int64_t cur_pts_ns(const Cur *c) { return ticks_ns(c->dts + c->ctts_v - c->t->shift, c->t->ts) + c->t->delay_ns; }

// After sample c->i has been read: on to the next.
static void cur_advance(Mp4File *f, Cur *c) {
    Mp4Trk *t = c->t;
    const uint32_t size = sample_size(f, t, c->i);
    c->i++;
    if (c->i >= t->n_samples) { c->done = true; return; }
    c->dts += c->stts_delta;
    if (c->stts_left != 0xFFFFFFFFu && --c->stts_left == 0) {
        uint32_t cnt, d;
        if (c->stts_i + 1 < t->stts_n && stts_entry(f, t, c->stts_i + 1, &cnt, &d)) {
            c->stts_i++; c->stts_left = cnt ? cnt : 1; c->stts_delta = d;
        } else {
            c->stts_left = 0xFFFFFFFFu;                  // past the table: the last delta goes on
        }
    }
    if (t->has_ctts && c->ctts_left && --c->ctts_left == 0 && c->ctts_i + 1 < t->ctts_n) {
        const uint8_t *p = tab_at(f, &t->ctts, (uint64_t)(c->ctts_i + 1) * 8, 8);
        if (p) { c->ctts_i++; c->ctts_left = be32(p); c->ctts_v = (int32_t)be32(p + 4); }
    }
    if (--c->chunk_left == 0) {
        c->chunk++;
        if (c->chunk >= t->stco_n) { c->done = true; return; }
        if (c->chunk >= c->next_first && c->stsc_i + 1 < t->stsc_n) {
            uint32_t first, spc;
            c->stsc_i++;
            if (stsc_entry(f, t, c->stsc_i, &first, &spc)) c->spc = spc;
            uint32_t nfirst = t->stco_n, nspc;
            if (c->stsc_i + 1 < t->stsc_n && stsc_entry(f, t, c->stsc_i + 1, &nfirst, &nspc)) c->next_first = nfirst;
            else c->next_first = t->stco_n;
        }
        // a chunk said to hold no samples is skipped
        while (c->spc == 0 && c->chunk + 1 < t->stco_n) c->chunk++;
        c->chunk_left = c->spc ? c->spc : 1;
        c->off = chunk_offset(f, t, c->chunk);
    } else {
        c->off += size;
    }
    if (t->has_stss && c->i + 1 > c->next_sync) {
        c->stss_i++;
        c->next_sync = c->stss_i < t->stss_n ? sync_at(f, t, c->stss_i) : 0xFFFFFFFFu;
    }
}

Mp4Reader *mp4_reader_open(Mp4File *f, const int *tracks, int n_tracks, uint8_t *buf, uint32_t cap) {
    Mp4Reader *r = (Mp4Reader *)calloc(1, sizeof *r);
    if (!r) return NULL;
    r->f = f; r->buf = buf; r->cap = cap;
    for (int k = 0; k < n_tracks && r->n < READER_TRACKS; k++) {
        Mp4Trk *t = tracks[k] ? trk_of(f, tracks[k]) : NULL;
        if (!t) continue;
        bool dup = false;
        for (int q = 0; q < r->n; q++) dup |= r->cur[q].t == t;
        if (dup) continue;
        if (!cur_set(f, &r->cur[r->n], t, 0)) { free(r); return NULL; }
        r->n++;
    }
    if (r->n == 0) { free(r); return NULL; }
    return r;
}

void mp4_reader_close(Mp4Reader *r) { free(r); }

uint32_t mp4_reader_oversize(const Mp4Reader *r) { return r->oversize; }

// The last sample of the track whose decode time is at or before `ns` (0 when none is).
static uint32_t sample_at_time(Mp4File *f, Mp4Trk *t, int64_t ns) {
    // to the track's own ticks
    int64_t rel = ns - t->delay_ns;
    if (rel < 0) return 0;
    const int64_t want = (rel / 1000000000LL) * (int64_t)t->ts + (rel % 1000000000LL) * (int64_t)t->ts / 1000000000LL + t->shift;
    uint32_t base = 0; int64_t dts = 0;
    for (uint32_t j = 0; j < t->stts_n; j++) {
        uint32_t cnt, d;
        if (!stts_entry(f, t, j, &cnt, &d)) break;
        const int64_t end = dts + (int64_t)cnt * d;
        if (want < end || j + 1 == t->stts_n) {
            uint64_t k = d ? (uint64_t)((want > dts ? want - dts : 0) / d) : 0;
            if (k >= cnt && j + 1 < t->stts_n) k = cnt - 1;
            uint64_t s = (uint64_t)base + k;
            if (s >= t->n_samples) s = t->n_samples ? t->n_samples - 1 : 0;
            return (uint32_t)s;
        }
        base += cnt; dts = end;
    }
    return 0;
}

bool mp4_reader_seek(Mp4Reader *r, int video_track, uint64_t target_ns) {
    Mp4File *f = r->f;
    int64_t key_ns = (int64_t)target_ns;
    Cur *vc = NULL;
    for (int k = 0; k < r->n; k++) if (r->cur[k].t->number == video_track) vc = &r->cur[k];
    if (vc) {
        Mp4Trk *t = vc->t;
        uint32_t s = sample_at_time(f, t, (int64_t)target_ns);
        if (t->has_stss && t->stss_n) {
            // the last sync sample at or before s (1-based numbers in stss)
            const uint32_t j = stss_lower(f, t, s + 2);   // first sync > s+1
            const uint32_t sync = j > 0 ? sync_at(f, t, j - 1) : 1;
            s = sync ? sync - 1 : 0;
        }
        if (!cur_set(f, vc, t, s)) return false;
        key_ns = cur_pts_ns(vc);
        if (key_ns < 0) key_ns = 0;
    }
    for (int k = 0; k < r->n; k++) {
        Cur *c = &r->cur[k];
        if (c == vc) continue;
        if (!cur_set(f, c, c->t, sample_at_time(f, c->t, key_ns))) return false;
    }
    return !f->err;
}

int mp4_reader_next(Mp4Reader *r, MkvFrame *out) {
    Mp4File *f = r->f;
    for (;;) {
        Cur *best = NULL;
        int64_t best_t = 0;
        for (int k = 0; k < r->n; k++) {
            Cur *c = &r->cur[k];
            if (c->done) continue;
            const int64_t d = cur_dts_ns(c);
            if (!best || d < best_t) { best = c; best_t = d; }
        }
        if (!best) return 0;
        Mp4Trk *t = best->t;
        const uint32_t size = sample_size(f, t, best->i);
        if (f->err) return -1;
        const bool key = !t->has_stss || best->i + 1 == best->next_sync;
        if (size > r->cap) {
            r->oversize++;
            cur_advance(f, best);
            if (f->err) return -1;
            continue;
        }
        const int got = rd_at(f, best->off, r->buf, (int)size);
        if (got < 0) return -1;
        if (got < (int)size) { best->done = true; continue; }          // the file ends early: that track is over
        memset(out, 0, sizeof *out);
        out->track = t->number;
        out->pts_ns = cur_pts_ns(best);
        out->duration_ns = (uint64_t)ticks_ns(best->stts_delta, t->ts);
        out->key = key;
        out->data = r->buf;
        out->size = size;
        cur_advance(f, best);
        if (f->err) return -1;
        return 1;
    }
}

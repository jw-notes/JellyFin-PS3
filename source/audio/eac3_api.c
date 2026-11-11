// JellyFin [PS] for PS3 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
//
// Single-TU wrapper over the vendored FFmpeg AC-3 / E-AC-3 decoder (source/audio/eac3/, see
// PROVENANCE.md there): see eac3_api.h.
#include <stdlib.h>
#include <string.h>
#include "mlp/ff/libavutil/internal.h"
#include "mlp/ff/libavcodec/avcodec.h"

// The decoder (ac3dec_float.c #includes eac3dec.c and ac3dec.c) and the tables, parser and DSP it uses.
// vlc.c, crc.c and the channel-layout helpers are mlp_api.c's and mlp_compat.c's: one copy, linked.
#include "eac3/ac3dec_float.c"
#include "eac3/ac3dec_data.c"
#include "eac3/ac3.c"
#include "eac3/ac3_parser.c"
#include "eac3/ac3tab.c"
#include "eac3/eac3_data.c"
#include "eac3/kbdwin.c"
#include "eac3/ac3dsp.c"
#include "eac3/bswapdsp.c"
// The table is defined in this header, which FFmpeg includes from a .c of its own.
#include "eac3/ac3_channel_layout_tab.h"

#include "eac3_api.h"

// --- shims (see mlp/ff/libavcodec/parser.h) ------------------------------------------------------
// libavcodec's AC-3 parser object is compiled with the header parser but never registered or run.
int  ff_aac_ac3_parse(AVCodecParserContext *s1, AVCodecContext *avctx, const uint8_t **poutbuf, int *poutbuf_size,
                      const uint8_t *buf, int buf_size)
{ (void)s1; (void)avctx; (void)buf; *poutbuf = NULL; *poutbuf_size = 0; return buf_size; }
void ff_parse_close(AVCodecParserContext *s) { (void)s; }
// The decoder publishes the stream's downmix levels as frame side data; nothing here reads them, so
// one scratch record takes them.
AVDownmixInfo *av_downmix_info_update_side_data(struct AVFrame *frame)
{
    static AVDownmixInfo s_info;
    (void)frame;
    memset(&s_info, 0, sizeof s_info);
    return &s_info;
}

// --- the instance ---------------------------------------------------------------------------------

struct eac3_dec {
    AVCodecContext   avctx;
    AC3DecodeContext ctx;
    AVFrame          frame;
    float            pcm[EAC3_API_MAX_CHANNELS * EAC3_API_MAX_SAMPLES];
    int              open;
};

// What av_opt_set_defaults() would set from ac3dec_float.c's option table, which this app does not run.
static void set_defaults(AC3DecodeContext *s)
{
    s->class = (AVClass *)&ac3_eac3_decoder_class;
    s->consistent_noise_generation = 0;
    s->drc_scale = 1.0f;
    s->heavy_compression = 0;
    s->target_level = 0;
    s->preferred_stereo_downmix = -1;
    s->ltrt_center_mix_level = -1.0f;
    s->ltrt_surround_mix_level = -1.0f;
    s->loro_center_mix_level = -1.0f;
    s->loro_surround_mix_level = -1.0f;
    memset(&s->downmix_layout, 0, sizeof s->downmix_layout);      // no downmix: every channel the stream has
}

eac3_dec *eac3_api_open(void)
{
    eac3_dec *d = (eac3_dec *)calloc(1, sizeof *d);
    if (!d) return NULL;
    set_defaults(&d->ctx);
    d->avctx.priv_data = &d->ctx;
    d->avctx.codec_id = AV_CODEC_ID_EAC3;
    d->avctx.opaque = d->pcm;
    d->avctx.opaque_size = (int)sizeof d->pcm;
    if (ac3_decode_init(&d->avctx) < 0) { ac3_decode_end(&d->avctx); free(d); return NULL; }
    d->open = 1;
    return d;
}

void eac3_api_close(eac3_dec *d)
{
    if (!d) return;
    if (d->open) ac3_decode_end(&d->avctx);
    free(d);
}

void eac3_api_flush(eac3_dec *d)
{
    if (d && d->open) ac3_decode_flush(&d->avctx);
}

int eac3_api_frame_size(const uint8_t *buf, int len, int *dependent)
{
    if (dependent) *dependent = 0;
    if (!buf || len < AC3_HEADER_SIZE + 8) return 0;
    if (buf[0] != 0x0B || buf[1] != 0x77) return 0;
    // ff_ac3_parse_header reads a little past the header: hand it a padded copy
    uint8_t tmp[AC3_HEADER_SIZE + 16 + EAC3_API_PADDING];
    memset(tmp, 0, sizeof tmp);
    memcpy(tmp, buf, (size_t)(len < (int)(AC3_HEADER_SIZE + 16) ? len : (int)(AC3_HEADER_SIZE + 16)));
    GetBitContext gb;
    AC3HeaderInfo hdr;
    memset(&hdr, 0, sizeof hdr);
    init_get_bits(&gb, tmp, (AC3_HEADER_SIZE + 16) * 8);
    if (ff_ac3_parse_header(&gb, &hdr) < 0 || hdr.frame_size <= 0) return 0;
    if (dependent) *dependent = hdr.frame_type == EAC3_FRAME_TYPE_DEPENDENT;
    return hdr.frame_size;
}

int eac3_api_decode(eac3_dec *d, const uint8_t *data, int size, int *samples)
{
    if (samples) *samples = 0;
    if (!d || !d->open || !data || size <= 0) return -1;
    AVPacket pkt;
    pkt.data = data;
    pkt.size = size;
    memset(&d->frame, 0, sizeof d->frame);
    int got = 0;
    const int r = ac3_decode_frame(&d->avctx, &d->frame, &got, &pkt);
    if (r < 0) return r;
    if (got && samples) *samples = d->frame.nb_samples;
    return r > 0 ? r : size;
}

int eac3_api_channels(const eac3_dec *d) { return d ? d->avctx.ch_layout.nb_channels : 0; }
int eac3_api_sample_rate(const eac3_dec *d) { return d ? d->avctx.sample_rate : 0; }

uint64_t eac3_api_channel_mask(const eac3_dec *d)
{
    if (!d || d->avctx.ch_layout.order != AV_CHANNEL_ORDER_NATIVE) return 0;
    return d->avctx.ch_layout.u.mask;
}

const float *eac3_api_plane(const eac3_dec *d, int ch)
{
    if (!d || ch < 0 || ch >= d->avctx.ch_layout.nb_channels || ch >= EAC3_API_MAX_CHANNELS) return NULL;
    return (const float *)d->frame.extended_data[ch];
}

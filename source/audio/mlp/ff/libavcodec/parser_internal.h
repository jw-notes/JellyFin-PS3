/*
 * Minimal stand-in for libavcodec/parser_internal.h, for the vendored AC-3 /
 * E-AC-3 decoder: the FFCodecParser descriptor ac3_parser.c fills in.  Never
 * registered or used (see parser.h).
 */
#ifndef AVCODEC_PARSER_INTERNAL_H
#define AVCODEC_PARSER_INTERNAL_H

#include "parser.h"

typedef struct FFCodecParser {
    int  codec_ids[7];
    int  priv_data_size;
    int  (*init)(AVCodecParserContext *s);
    int  (*parse)(AVCodecParserContext *s, AVCodecContext *avctx,
                  const uint8_t **poutbuf, int *poutbuf_size,
                  const uint8_t *buf, int buf_size);
    void (*close)(AVCodecParserContext *s);
} FFCodecParser;

#define PARSER_CODEC_LIST(...) .codec_ids = { __VA_ARGS__ }

#endif /* AVCODEC_PARSER_INTERNAL_H */

/*
 * Minimal stand-in for libavcodec/parser.h, for the vendored AC-3 / E-AC-3
 * decoder (source/audio/eac3/, see PROVENANCE.md there).  ac3_parser.c keeps
 * the header parser the decoder needs (ff_ac3_parse_header) behind the same
 * #if as libavcodec's AC-3 parser object; that object only has to COMPILE
 * here: nothing registers or calls it.
 */
#ifndef AVCODEC_PARSER_H
#define AVCODEC_PARSER_H

#include "avcodec.h"

typedef struct AVCodecParserContext {
    void *priv_data;
} AVCodecParserContext;

int  ff_aac_ac3_parse(AVCodecParserContext *s1, AVCodecContext *avctx,
                      const uint8_t **poutbuf, int *poutbuf_size,
                      const uint8_t *buf, int buf_size);
void ff_parse_close(AVCodecParserContext *s);

#endif /* AVCODEC_PARSER_H */

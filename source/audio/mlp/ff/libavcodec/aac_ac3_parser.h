/*
 * Minimal stand-in for libavcodec/aac_ac3_parser.h, for the vendored AC-3 /
 * E-AC-3 decoder: the parser context ac3_parser.c's (unused) parser object
 * initialises.  See parser.h.
 */
#ifndef AVCODEC_AAC_AC3_PARSER_H
#define AVCODEC_AAC_AC3_PARSER_H

#include <stdint.h>
#include "libavutil/crc.h"
#include "parser.h"

typedef struct AACAC3ParseContext {
    int header_size;
    int (*sync)(uint64_t state, int *need_next_header, int *new_frame_start);
    const AVCRC *crc_ctx;
} AACAC3ParseContext;

#endif /* AVCODEC_AAC_AC3_PARSER_H */

#pragma once
/* Stub: the real header includes frame.h, whose full AVFrame collides with
   the minimal one this shim defines.  The DCA decoder only uses these types
   to EXPORT downmix metadata to a caller; it never affects decoded audio,
   and this player does its own PS3 channel mapping regardless. */
#include <stddef.h>
#include <stdint.h>
enum AVDownmixType { AV_DOWNMIX_TYPE_UNKNOWN = 0, AV_DOWNMIX_TYPE_LORO, AV_DOWNMIX_TYPE_LTRT, AV_DOWNMIX_TYPE_DPLII };
typedef double AVDownmixCoeff;
typedef struct AVDownmixMatrix { enum AVDownmixType preferred_downmix_type; int nb_channels; } AVDownmixMatrix;
AVDownmixMatrix *av_downmix_matrix_alloc(enum AVDownmixType t, int nch, size_t *size);
AVDownmixCoeff  *av_downmix_matrix_coeff(AVDownmixMatrix *dm, int out, int in);

/* The AC-3 decoder's downmix metadata (upstream's AVDownmixInfo).  It fills one in and publishes it as
   frame side data; eac3_api.c's av_downmix_info_update_side_data() gives it somewhere to go and
   nothing reads it: the player maps channels itself. */
typedef struct AVDownmixInfo {
    enum AVDownmixType preferred_downmix_type;
    double center_mix_level;
    double center_mix_level_ltrt;
    double surround_mix_level;
    double surround_mix_level_ltrt;
    double lfe_mix_level;
} AVDownmixInfo;
struct AVFrame;
AVDownmixInfo *av_downmix_info_update_side_data(struct AVFrame *frame);

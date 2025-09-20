#ifndef POLLUX_INTERNAL_CODEC_CODEC_H
#define POLLUX_INTERNAL_CODEC_CODEC_H

#include <libavformat/avformat.h>
#include <libavutil/opt.h>

#include "pollux/codec/av1.h"
#include "pollux/codec/codec.h"
#include "pollux/codec/hevc.h"
#include "pollux/internal/util.h"
#include "pollux/pollux_codec_id.h"

#if !defined(codec_hevc_libx265) && !defined(codec_hevc_hevc_nvenc) && \
  !defined(codec_hevc_hevc_amf) && !defined(codec_hevc_hevc_qsv)
#  define codec_hevc_libx265
#endif

#if !defined(codec_av1_libsvtav1) && !defined(codec_av1_libaom_av1) && \
  !defined(codec_av1_av1_nvenc) && !defined(codec_av1_av1_amf) && \
  !defined(codec_av1_av1_qsv) && !defined(codec_av1_librav1e)
#  define codec_av1_libsvtav1
#endif

static inline void codec_range_check_or_set(int *range) {
  range_set(codec_range_min, codec_range_max, range);
}

void hevc_priv_set(void *cc_priv, const hevc_encode_args_t *args);
void av1_priv_set(void *cc_priv, const av1_encode_args_t *args);

void codec_priv_set(void *cc_priv, pollux_codec_id_t codec_id,
                    const void *args);

#endif // POLLUX_INTERNAL_CODEC_CODEC_H

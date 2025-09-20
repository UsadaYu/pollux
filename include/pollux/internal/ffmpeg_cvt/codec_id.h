#ifndef POLLUX_INTERNAL_FFMPEG_CVT_CODEC_ID_H
#define POLLUX_INTERNAL_FFMPEG_CVT_CODEC_ID_H

#include <libavcodec/codec_id.h>
#include <sirius/sirius_log.h>

#include "pollux/internal/decls.h"
#include "pollux/pollux_codec_id.h"

#define CVT(c) \
  switch (c) { \
    C(pollux_codec_id_mjpeg, AV_CODEC_ID_MJPEG) \
    C(pollux_codec_id_h264, AV_CODEC_ID_H264) \
    C(pollux_codec_id_png, AV_CODEC_ID_PNG) \
    C(pollux_codec_id_gif, AV_CODEC_ID_GIF) \
    C(pollux_codec_id_hevc, AV_CODEC_ID_HEVC) \
    C(pollux_codec_id_av1, AV_CODEC_ID_AV1) \
  default: \
    D(c) \
    if (unlikely((enum AVCodecID)c <= AV_CODEC_ID_NONE)) { \
      sirius_error("Invalid codec id\n"); \
      return false; \
    } \
  } \
  return true;

#ifdef __cplusplus
extern "C" {
#endif

static inline bool cvt_codec_id_plx_to_ff(pollux_codec_id_t src,
                                          enum AVCodecID *dst) {
#define C(s, d) \
case s: \
  *dst = d; \
  break;
#define D(c) \
  sirius_warn("Codec id `%lld` may not be supported\n", (int64_t)c); \
  *dst = (enum AVCodecID)c;
  CVT(src)
#undef D
#undef C
}

static inline bool cvt_codec_id_ff_to_plx(enum AVCodecID src,
                                          pollux_codec_id_t *dst) {
#define C(d, s) \
case s: \
  *dst = d; \
  break;
#define D(c) \
  sirius_error("Codec id `%lld` is not supported\n", (int64_t)c); \
  return false;
  CVT(src)
#undef D
#undef C
}

#ifdef __cplusplus
}
#endif

#undef CVT

#endif // POLLUX_INTERNAL_FFMPEG_CVT_CODEC_ID_H

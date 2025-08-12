#ifndef CVT_CODEC_ID_H
#define CVT_CODEC_ID_H

#include <libavcodec/codec_id.h>
#include <sirius/sirius_log.h>

#include "pollux_codec_id.h"

#define CVT(c)                                      \
  switch (c) {                                      \
    C(pollux_codec_id_mjpeg, AV_CODEC_ID_MJPEG)     \
    C(pollux_codec_id_h264, AV_CODEC_ID_H264)       \
    C(pollux_codec_id_png, AV_CODEC_ID_PNG)         \
    C(pollux_codec_id_gif, AV_CODEC_ID_GIF)         \
    C(pollux_codec_id_hevc, AV_CODEC_ID_HEVC)       \
    C(pollux_codec_id_av1, AV_CODEC_ID_AV1)         \
    default:                                        \
      sirius_warn("Unsupported codec id: %d\n", c); \
      return false;                                 \
  }                                                 \
  return true;

#ifdef __cplusplus
extern "C" {
#endif

static inline bool cvt_codec_id_plx_to_ff(
    pollux_codec_id_t src, enum AVCodecID *dst) {
#define C(s, d) \
  case s:       \
    *dst = d;   \
    break;
  CVT(src)
#undef C
}

static inline bool cvt_codec_id_ff_to_plx(
    enum AVCodecID src, pollux_codec_id_t *dst) {
#define C(d, s) \
  case s:       \
    *dst = d;   \
    break;
  CVT(src)
#undef C
}

#ifdef __cplusplus
}
#endif

#undef CVT

#endif  // CVT_CODEC_ID_H

#ifndef CVT_PIXEL_H
#define CVT_PIXEL_H

#include <libavutil/pixfmt.h>
#include <sirius/sirius_log.h>

#include "pollux_pixel_fmt.h"

#define CVT(c)                                          \
  switch (c) {                                          \
    C(pollux_pix_fmt_yuv420p, AV_PIX_FMT_YUV420P)       \
    C(pollux_pix_fmt_yuyv422, AV_PIX_FMT_YUYV422)       \
    C(pollux_pix_fmt_rgb24, AV_PIX_FMT_RGB24)           \
    C(pollux_pix_fmt_bgr24, AV_PIX_FMT_BGR24)           \
    C(pollux_pix_fmt_yuv444p, AV_PIX_FMT_YUV444P)       \
    C(pollux_pix_fmt_pal8, AV_PIX_FMT_PAL8)             \
    C(pollux_pix_fmt_yuvj420p, AV_PIX_FMT_YUVJ420P)     \
    C(pollux_pix_fmt_bgr8, AV_PIX_FMT_BGR8)             \
    C(pollux_pix_fmt_bgr4, AV_PIX_FMT_BGR4)             \
    C(pollux_pix_fmt_bgr4_byte, AV_PIX_FMT_BGR4_BYTE)   \
    C(pollux_pix_fmt_rgb8, AV_PIX_FMT_RGB8)             \
    C(pollux_pix_fmt_rgb4, AV_PIX_FMT_RGB4)             \
    C(pollux_pix_fmt_rgb4_byte, AV_PIX_FMT_RGB4_BYTE)   \
    C(pollux_pix_fmt_nv21, AV_PIX_FMT_NV21)             \
    C(pollux_pix_fmt_nv12, AV_PIX_FMT_NV12)             \
    default:                                            \
      sirius_warn("Unsupported pixel format: %d\n", c); \
      return false;                                     \
  }                                                     \
  return true;

#ifdef __cplusplus
extern "C" {
#endif

static inline bool cvt_pix_plx_to_ff(
    pollux_pix_fmt_t src, enum AVPixelFormat *dst) {
#define C(p, f) \
  case p:       \
    *dst = f;   \
    break;
  CVT(src);
#undef C
}

static inline bool cvt_pix_ff_to_plx(
    enum AVPixelFormat src, pollux_pix_fmt_t *dst) {
#define C(f, p) \
  case p:       \
    *dst = f;   \
    break;
  CVT(src);
#undef C
}

#ifdef __cplusplus
}
#endif

#undef CVT

#endif  // CVT_PIXEL_H

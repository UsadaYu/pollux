#ifndef POLLUX_INTERNAL_FFMPEG_CVT_PIXEL_H
#define POLLUX_INTERNAL_FFMPEG_CVT_PIXEL_H

#include <libavutil/pixfmt.h>
#include <sirius/sirius_log.h>

#include "pollux/internal/decls.h"
#include "pollux/pollux_pixel_fmt.h"

#define CVT(c) \
  switch (c) { \
    C(pollux_pix_fmt_yuv420p, AV_PIX_FMT_YUV420P) \
    C(pollux_pix_fmt_yuyv422, AV_PIX_FMT_YUYV422) \
    C(pollux_pix_fmt_rgb24, AV_PIX_FMT_RGB24) \
    C(pollux_pix_fmt_bgr24, AV_PIX_FMT_BGR24) \
    C(pollux_pix_fmt_yuv444p, AV_PIX_FMT_YUV444P) \
    C(pollux_pix_fmt_pal8, AV_PIX_FMT_PAL8) \
    C(pollux_pix_fmt_yuvj420p, AV_PIX_FMT_YUVJ420P) \
    C(pollux_pix_fmt_yuvj422p, AV_PIX_FMT_YUVJ422P) \
    C(pollux_pix_fmt_yuvj444p, AV_PIX_FMT_YUVJ444P) \
    C(pollux_pix_fmt_bgr8, AV_PIX_FMT_BGR8) \
    C(pollux_pix_fmt_bgr4, AV_PIX_FMT_BGR4) \
    C(pollux_pix_fmt_bgr4_byte, AV_PIX_FMT_BGR4_BYTE) \
    C(pollux_pix_fmt_rgb8, AV_PIX_FMT_RGB8) \
    C(pollux_pix_fmt_rgb4, AV_PIX_FMT_RGB4) \
    C(pollux_pix_fmt_rgb4_byte, AV_PIX_FMT_RGB4_BYTE) \
    C(pollux_pix_fmt_nv21, AV_PIX_FMT_NV21) \
    C(pollux_pix_fmt_nv12, AV_PIX_FMT_NV12) \
  default: \
    D(c) \
    if (unlikely((enum AVPixelFormat)c <= AV_PIX_FMT_NONE || \
                 (enum AVPixelFormat)c >= AV_PIX_FMT_NB)) { \
      sirius_error("Invalid pixel format\n"); \
      return false; \
    } \
  } \
  return true;

#ifdef __cplusplus
extern "C" {
#endif

static inline bool cvt_pix_plx_to_ff(pollux_pix_fmt_t src,
                                     enum AVPixelFormat *dst) {
#define C(p, f) \
case p: \
  *dst = f; \
  break;
#define D(c) \
  sirius_warn("Pixel format `%lld` may not be supported\n", (int64_t)c); \
  *dst = (enum AVPixelFormat)c;
  CVT(src);
#undef D
#undef C
}

static inline bool cvt_pix_ff_to_plx(enum AVPixelFormat src,
                                     pollux_pix_fmt_t *dst) {
#define C(f, p) \
case p: \
  *dst = f; \
  break;
#define D(c) \
  sirius_error("Pixel format `%lld` is not supported\n", (int64_t)c); \
  return false;
  CVT(src);
#undef D
#undef C
}

#ifdef __cplusplus
}
#endif

#undef CVT

#endif // POLLUX_INTERNAL_FFMPEG_CVT_PIXEL_H

#ifndef POLLUX_INTERNAL_FFMPEG_CVT_FRAME_H
#define POLLUX_INTERNAL_FFMPEG_CVT_FRAME_H

#include <libavutil/frame.h>
#include <sirius/sirius_attributes.h>

#include "pollux/internal/decls.h"
#include "pollux/internal/ffmpeg_cvt/pixel.h"
#include "pollux/pollux_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline bool cvt_frame_plx_to_ff(const pollux_frame_t *src,
                                       AVFrame *dst) {
  if (unlikely(!src || !dst))
    return false;

  dst->width = src->width;
  dst->height = src->height;
  if (unlikely(!cvt_pix_plx_to_ff(src->fmt, &dst->format))) {
    return false;
  }
  dst->pts = src->pts;
  dst->pkt_dts = src->pkt_dts;
  dst->time_base.den = src->time_base.den;
  dst->time_base.num = src->time_base.num;
  memcpy(dst->linesize, src->linesize, sizeof(src->linesize));
  memcpy(dst->data, src->data, sizeof(src->data));

  return true;
}

static inline bool cvt_frame_ff_to_plx(const AVFrame *src,
                                       pollux_frame_t *dst) {
  if (unlikely(!src || !dst))
    return false;

  dst->width = src->width;
  dst->height = src->height;
  if (unlikely(!cvt_pix_ff_to_plx(src->format, &dst->fmt))) {
    return false;
  }
  dst->pts = src->pts;
  dst->pkt_dts = src->pkt_dts;
  dst->time_base.den = src->time_base.den;
  dst->time_base.num = src->time_base.num;
  memcpy(dst->linesize, src->linesize, sizeof(src->linesize));
  memcpy(dst->data, src->data, sizeof(src->data));

  return true;
}

#ifdef __cplusplus
}
#endif

#endif // POLLUX_INTERNAL_FFMPEG_CVT_FRAME_H

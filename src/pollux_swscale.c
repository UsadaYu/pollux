#include "pollux_swscale.h"

#include <libswscale/swscale.h>

#include "cvt/cvt_pixel.h"
#include "internal/internal_frame.h"
#include "pollux_erron.h"

int pollux_sws_scale(pollux_sws_handle handle,
                     pollux_frame_t *src,
                     pollux_frame_t *dst) {
  if (!handle || !src || !dst) return pollux_err_entry;

  if (!src->data[0]) {
    sirius_error("The source address is null\n");
    return pollux_err_null_pointer;
  }

  auto rf = (internal_frame_t *)(dst->priv_data);
  if (!rf) goto label_error;

  AVFrame *f = rf->av_frame;
  if (!f || !(f->data[0])) goto label_error;

  auto sc = (struct SwsContext *)handle;
  dst->height = sws_scale(
      sc, (const uint8_t *const *)(src->data),
      src->linesize, 0, src->height, f->data, f->linesize);
  if (dst->height < 0) {
    sirius_error("Ffmpeg-sws_scale\n");
    return -1;
  }
  memcpy(dst->linesize, f->linesize, sizeof(f->linesize));
  memcpy(dst->data, f->data, sizeof(f->data));

  return 0;

label_error:
  sirius_error("The destination address is null\n");
  return pollux_err_null_pointer;
}

pollux_sws_handle pollux_sws_context_alloc(
    int src_width, int src_height,
    pollux_pix_fmt_t src_fmt, int dst_width,
    int dst_height, pollux_pix_fmt_t dst_fmt) {
  enum AVPixelFormat src_av_fmt, dst_av_fmt;
  if (!cvt_pix_plx_to_ff(src_fmt, &src_av_fmt) ||
      !cvt_pix_plx_to_ff(dst_fmt, &dst_av_fmt))
    return nullptr;

  int flags = SWS_BILINEAR;
  SwsFilter *src_filter = nullptr;
  SwsFilter *dst_filter = nullptr;
  double *param = nullptr;
  struct SwsContext *sc =
      sws_getContext(src_width, src_height, src_av_fmt,
                     dst_width, dst_height, dst_av_fmt,
                     flags, src_filter, dst_filter, param);
  if (!sc) {
    sirius_error("Ffmpeg-sws_getContext\n");
    return nullptr;
  }

  return (pollux_sws_handle)sc;
}

void pollux_sws_context_free(pollux_sws_handle *handle) {
  if (!handle) return;

  auto sc = (struct SwsContext *)(*handle);
  if (sc) {
    sws_freeContext(sc);
    sc = nullptr;
  }
}

#include "pollux_frame.h"

#include <libavutil/imgutils.h>

#include "cvt/cvt_pixel.h"
#include "internal/internal_ff_erron.h"
#include "internal/internal_frame.h"
#include "pollux_erron.h"

void pollux_frame_free(pollux_frame_t **res) {
  pollux_frame_t **r = res;

  if (r && (*r)) {
    auto rf = (internal_frame_t **)(&((*r)->priv_data));

    if (rf && (*rf)) {
      AVFrame **f = &((*rf)->av_frame);

      if (f && (*f)) {
        if ((*rf)->has_img_mem) {
          (*rf)->has_img_mem = false;

          uint8_t **d = (*f)->data + 0;
          if (d && (*d)) {
            av_freep(d);
            *d = nullptr;
          }
        }

        av_frame_free(f);
      }

      free(*rf);
      *rf = nullptr;
    }

    free(*r);
    *r = nullptr;
  }
}

/**
 * @brief to request memory for `pollux_frame_t`, actually,
 *  request a frame cache through ffmpeg's api and record
 *  it using the `priv_data` pointer
 */
int pollux_frame_alloc(const pollux_img_t *img,
                       pollux_frame_t **res) {
  if (!res) {
    return pollux_err_entry;
  }

  pollux_frame_t *r = calloc(1, sizeof(pollux_frame_t));
  if (!r) {
    sirius_error("calloc\n");
    return pollux_err_memory_alloc;
  }

  internal_frame_t *rf =
      calloc(1, sizeof(internal_frame_t));
  if (!rf) {
    sirius_error("calloc\n");
    goto label_free;
  }
  r->priv_data = (void *)rf;

  AVFrame *f = av_frame_alloc();
  if (!f) {
    sirius_error("av_frame_alloc\n");
    goto label_free;
  }
  rf->av_frame = f;

  if (img) {
    enum AVPixelFormat av_fmt;
    if (!cvt_pix_plx_to_ff(img->fmt, &av_fmt))
      goto label_free;

    int ret =
        av_image_alloc(f->data, f->linesize, img->width,
                       img->height, av_fmt, img->align);
    if (ret < 0) {
      internal_ff_error(ret, av_image_alloc);
      goto label_free;
    }

    memcpy(r->linesize, f->linesize, sizeof(f->linesize));
    memcpy(r->data, f->data, sizeof(f->data));
    r->width = f->width = img->width;
    r->height = f->height = img->height;
    f->format = av_fmt;
    r->fmt = img->fmt;
    r->pts = AV_NOPTS_VALUE;
    r->pkt_dts = AV_NOPTS_VALUE;

    rf->has_img_mem = true;
  } else {
    sirius_debg(
        "The parameter `img` is empty, so no memory "
        "related to the image is applied for\n");
    sirius_debg("Frame address: %p\n", r);
  }

  *res = r;
  return 0;

label_free:
  pollux_frame_free(&r);

  return pollux_err_memory_alloc;
}

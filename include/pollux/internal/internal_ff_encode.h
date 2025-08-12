#ifndef INTERNAL_FF_ENCODE_H
#define INTERNAL_FF_ENCODE_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <sirius/sirius_attributes.h>

#include "pollux_rational.h"

#if !defined(FF_AV1_STV_AV1) && !defined(FF_AV1_LIBAOM)
#define FF_AV1_STV_AV1
#endif

typedef struct {
  /* The average bitrate. */
  long int bit_rate;

  /* Width and height. */
  int width, height;

  /**
   * @brief Frame rate. In general,
   * frame rate = num / den
   */
  pollux_rational frame_rate;

  /* Gop size, keyframe interval. */
  int gop_size;

  /* Maximum number of B-frames between non-B-frames. */
  int max_b_frames;

  /**
   * Encoded image format, refer to `enum AVPixelFormat`.
   */
  enum AVPixelFormat pix_fmt;
} internal_ff_encode_param_t;

typedef struct {
  /* Format context. */
  AVFormatContext *fmt_ctx;

  /* Encode codec. */
  const AVCodec *codec;

  /* Encode codec context. */
  AVCodecContext *codec_ctx;

  /* Output stream. */
  AVStream *stream;
} internal_ff_encode_t;

void internal_ff_encode_deinit(internal_ff_encode_t *e);

[[nodiscard]] int internal_ff_encode_init(
    internal_ff_encode_t *e, const char *url,
    const char *cont_fmt);

void internal_ff_encode_codec_free(
    internal_ff_encode_t *e);

[[nodiscard]] int internal_ff_encode_codec_alloc(
    internal_ff_encode_t *e,
    internal_ff_encode_param_t *pm,
    enum AVCodecID codec_id);

#endif  // INTERNAL_FF_ENCODE_H

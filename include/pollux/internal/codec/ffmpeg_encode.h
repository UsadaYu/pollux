#ifndef POLLUX_INTERNAL_CODEC_FFMPEG_ENCODE_H
#define POLLUX_INTERNAL_CODEC_FFMPEG_ENCODE_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <sirius/sirius_attributes.h>

#include "pollux/pollux_rational.h"

typedef struct {
  /**
   * @brief The average bitrate.
   */
  int64_t bit_rate;

  /**
   * @brief Width and height.
   */
  int width, height;

  /**
   * @brief Frame rate. In general, frame rate = num / den.
   */
  pollux_rational frame_rate;

  /**
   * @brief Gop size, keyframe interval.
   */
  int gop_size;

  /**
   * @brief Maximum number of B-frames between non-B-frames.
   */
  int max_b_frames;

  /**
   * @brief Encoded image format.
   *
   * @see enum AVPixelFormat
   */
  enum AVPixelFormat pix_fmt;

  /**
   * @brief Thread count.
   */
  int thread_count;
} ffmpeg_encode_args_t;

typedef struct {
  /**
   * @brief Format context.
   */
  AVFormatContext *fmt_ctx;

  /**
   * @brief Encode codec.
   */
  const AVCodec *codec;

  /**
   * @brief Encode codec context.
   */
  AVCodecContext *codec_ctx;

  /**
   * @brief Output stream.
   */
  AVStream *stream;
} ffmpeg_encode_t;

void ffmpeg_encoder_deinit(ffmpeg_encode_t *e);
int ffmpeg_encoder_init(ffmpeg_encode_t *e, const char *url,
                        const char *cont_fmt);
void ffmpeg_encoder_ctx_free(ffmpeg_encode_t *e);
int ffmpeg_encoder_ctx_alloc(ffmpeg_encode_t *e, ffmpeg_encode_args_t *args,
                             enum AVCodecID codec_id);
void ffmpeg_encoder_close(ffmpeg_encode_t *e);
int ffmpeg_encoder_open(ffmpeg_encode_t *e);

#endif // POLLUX_INTERNAL_CODEC_FFMPEG_ENCODE_H

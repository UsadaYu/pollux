#ifndef INTERNAL_FF_DECODE_H
#define INTERNAL_FF_DECODE_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <sirius/sirius_attributes.h>

/* Maximum number of images cached. */
#define INTERNAL_FRAME_NR (128)

typedef struct {
  /* Format context. */
  AVFormatContext *fmt_ctx;

  /* Decode codec context. */
  AVCodecContext *codec_ctx;

  /* Stream index, such as video, audio, etc. */
  unsigned int stream_index;

  /* Frame data. */
  AVFrame *frame;
  /* Frame packet. */
  AVPacket *pkt;
} internal_ff_decode_t;

void internal_ff_decode_resource_free(
    internal_ff_decode_t *d);

int internal_ff_decode_resource_alloc(
    internal_ff_decode_t *d);

void internal_ff_decode_deinit(internal_ff_decode_t *d);

[[nodiscard]] int internal_ff_decode_init(
    internal_ff_decode_t *d, enum AVMediaType media_type,
    const char *url);

void internal_ff_decode_free(internal_ff_decode_t *d);

[[nodiscard]] int internal_ff_decode_alloc(
    internal_ff_decode_t *d, enum AVMediaType media_type,
    const char *url);

#endif  // INTERNAL_FF_DECODE_H

#include "pollux/internal/codec/ffmpeg_encode.h"

#include <sirius/sirius_time.h>

#include "pollux/internal/codec/codec.h"
#include "pollux/pollux_erron.h"

/**
 * @brief Deinitializes the encoder context. Safely closes `AVIO` context and
 * frees the format context.
 *
 * @param[in,out] e Internal encoder handle.
 */
void ffmpeg_encoder_deinit(ffmpeg_encode_t *e) {
  if (!e || !e->fmt_ctx) {
    return;
  }

  AVFormatContext *fc = e->fmt_ctx;

  if (fc->pb && !(fc->oformat->flags & AVFMT_NOFILE)) {
    avio_closep(&fc->pb);
  }

  avformat_free_context(fc);
  e->fmt_ctx = nullptr;
}

/**
 * @brief Initializes the ffmpeg encoder for a given output. The output can be
 * a local file or a network url.
 *
 * @param[out] e Internal encoder handle.
 * @param[in] url The output path or network url (e.g., "output.mp4" or
 * "rtmp://...").
 * @param[in] cont_fmt The name of the container format (e.g., "mp4", "mpegts").
 * If `nullptr`, ffmpeg will guess from the destination name.
 *
 * @return 0 on success, or an error code on failure.
 */
int ffmpeg_encoder_init(ffmpeg_encode_t *e, const char *url,
                        const char *cont_fmt) {
  int ret;

  ret = avformat_alloc_output_context2(&e->fmt_ctx, nullptr, cont_fmt, url);
  if (ret < 0) {
    ffmpeg_error(ret, "avformat_alloc_output_context2");
    return pollux_err_resource_alloc;
  }

  AVFormatContext *fc = e->fmt_ctx;

  sirius_infosp("Encode target url: %s\n", url);
  /**
   * @brief For formats that require it (i.e., those without AVFMT_NOFILE), open
   * the I/O context. Protocols like `RTMP` or `SRT` will handle their
   * connections later in the function `avformat_write_header`, so we skip
   * `avio_open` for them.
   */
  if (!(fc->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&fc->pb, url, AVIO_FLAG_WRITE);
    if (ret < 0) {
      ffmpeg_error(ret, "avio_open");
      goto label_free;
    }
    sirius_debgsp("Successfully opened `AVIO` for: %s\n", url);
  } else {
    sirius_debgsp(
      "Format `%s` does not require explicit "
      "`avio_open` for: %s\n",
      fc->oformat->name, url);
  }

  return 0;

label_free:
  ffmpeg_encoder_deinit(e);
  return pollux_err_resource_alloc;
}

void ffmpeg_encoder_ctx_free(ffmpeg_encode_t *e) {
  AVCodecContext **cc = &e->codec_ctx;
  if (*cc) {
    avcodec_free_context(cc);
    *cc = nullptr;
  }
}

/**
 * @details
 * - (1) The result of `avformat_new_stream` should be freed by the function
 * `avformat_free_context`, which is called in the function
 * `ffmpeg_encode_deinit`.
 */
int ffmpeg_encoder_ctx_alloc(ffmpeg_encode_t *e, ffmpeg_encode_args_t *args,
                             enum AVCodecID codec_id) {
  AVFormatContext *fc = e->fmt_ctx;

  if (!fc) {
    sirius_error("Null pointer: [fmt_ctx]\n");
    return pollux_err_not_init;
  }

  const AVCodec **c = &e->codec;
  *c = avcodec_find_encoder(codec_id);
  if (!*c) {
    sirius_error("avcodec_find_encoder\n");
    return pollux_err_resource_alloc;
  }

  AVStream **s = &e->stream;
  *s = avformat_new_stream(fc, *c);
  if (!*s) {
    sirius_error("avformat_new_stream\n");
    return pollux_err_resource_alloc;
  }

  AVCodecContext **cc = &e->codec_ctx;
  *cc = avcodec_alloc_context3(*c);
  if (!*cc) {
    sirius_error("avcodec_alloc_context3\n");
    return pollux_err_resource_alloc;
  }

  (*cc)->codec_id = codec_id;
  (*cc)->bit_rate = args->bit_rate;
  (*cc)->height = args->height;
  (*cc)->width = args->width;
  (*cc)->time_base = (AVRational) {1, args->frame_rate.num};
  (*cc)->framerate = av_make_q(args->frame_rate.num, args->frame_rate.den);
  (*cc)->gop_size = args->gop_size;
  (*cc)->max_b_frames = args->max_b_frames;
  (*cc)->pix_fmt = args->pix_fmt;
  (*cc)->thread_count = args->thread_count;

  if (fc->oformat->flags & AVFMT_GLOBALHEADER) {
    (*cc)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  return 0;
}

void ffmpeg_encoder_close(ffmpeg_encode_t *e) {
  // ...
}

int ffmpeg_encoder_open(ffmpeg_encode_t *e) {
  int ret;
  AVFormatContext *fc = e->fmt_ctx;
  AVCodecContext **cc = &e->codec_ctx;
  AVStream **s = &e->stream;
  const AVCodec *c = e->codec;

  ret = avcodec_open2(*cc, c, nullptr);
  if (ret < 0) {
    ffmpeg_error(ret, "avcodec_open2");
    return pollux_err_resource_alloc;
  }

  AVCodecParameters *cp = (*s)->codecpar;
  ret = avcodec_parameters_from_context(cp, *cc);
  if (ret < 0) {
    ffmpeg_error(ret, "avcodec_parameters_from_context");
    return pollux_err_resource_alloc;
  }

  (*s)->time_base = (*cc)->time_base;

  av_dump_format(fc, 0, fc->url, 1);
  return 0;
}

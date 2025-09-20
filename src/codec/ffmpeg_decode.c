#include "pollux/internal/codec/ffmpeg_decode.h"

#include "pollux/internal/codec/codec.h"
#include "pollux/pollux_erron.h"

ffmpeg_decode_t *ffmpeg_decoder_create(const char *url) {
  ffmpeg_decode_t *d = calloc(1, sizeof(ffmpeg_decode_t));
  if (!d) {
    sirius_error("calloc -> 'ffmpeg_decode_t'\n");
    return nullptr;
  }
  d->stream_index = AVERROR_STREAM_NOT_FOUND;

  int ret;

  /**
   * @brief Allocate format context and open input.
   */
  d->fmt_ctx = avformat_alloc_context();
  if (!d->fmt_ctx) {
    sirius_error("avformat_alloc_context failed\n");
    goto label_free1;
  }

  ret = avformat_open_input(&d->fmt_ctx, url, nullptr, nullptr);
  if (ret < 0) {
    ffmpeg_error(ret, "avformat_open_input");
    goto label_free2;
  }

  /**
   * @brief Find stream info.
   */
  ret = avformat_find_stream_info(d->fmt_ctx, nullptr);
  if (ret < 0) {
    ffmpeg_error(ret, "avformat_find_stream_info");
    goto label_free3;
  }

  sirius_infosp("Decoder created for url: %s\n", url);
  return d;

label_free3:
  avformat_close_input(&d->fmt_ctx);
label_free2:
  avformat_free_context(d->fmt_ctx);
label_free1:
  free(d);

  return nullptr;
}

void ffmpeg_decoder_destroy(ffmpeg_decode_t **d_ptr) {
  if (!d_ptr || !*d_ptr)
    return;

  ffmpeg_decode_t *d = *d_ptr;

  ffmpeg_decoder_free_buffers(d);

  if (d->codec_ctx)
    avcodec_free_context(&d->codec_ctx);

  if (d->fmt_ctx) {
    /**
     * @note `avformat_close_input` also frees the context, no need for
     * `avformat_free_context`.
     */
    avformat_close_input(&d->fmt_ctx);
  }

  free(d);
  *d_ptr = nullptr;
}

int ffmpeg_decoder_open_stream(ffmpeg_decode_t *d, enum AVMediaType media_type,
                               const ffmpeg_decode_args_t *args) {
  if (!d || !d->fmt_ctx)
    return pollux_err_entry;

  int ret;

  const AVCodec *codec = nullptr;
  ret = av_find_best_stream(d->fmt_ctx, media_type, -1, -1, &codec, 0);
  if (ret < 0) {
    ffmpeg_error(ret, "av_find_best_stream");
    return pollux_err_args;
  }
  d->stream_index = ret;
  AVStream *stream = d->fmt_ctx->streams[d->stream_index];

  d->codec_ctx = avcodec_alloc_context3(codec);
  if (!d->codec_ctx) {
    sirius_error("avcodec_alloc_context3 failed\n");
    return pollux_err_resource_alloc;
  }

  /**
   * @brief Copy codec parameters from stream to context.
   */
  ret = avcodec_parameters_to_context(d->codec_ctx, stream->codecpar);
  if (ret < 0) {
    ffmpeg_error(ret, "avcodec_parameters_to_context");
    goto label_free1;
  }

  if (args) {
    d->codec_ctx->thread_count = args->thread_count;
  }

  ret = avcodec_open2(d->codec_ctx, codec, nullptr);
  if (ret < 0) {
    ffmpeg_error(ret, "avcodec_open2");
    goto label_free1;
  }

  sirius_infosp("Decoder for stream %d opened successfully\n", d->stream_index);
  return pollux_err_ok;

label_free1:
  avcodec_free_context(&d->codec_ctx);

  return pollux_err_resource_alloc;
}

int ffmpeg_decoder_alloc_buffers(ffmpeg_decode_t *d) {
  if (!d)
    return pollux_err_entry;

  d->frame = av_frame_alloc();
  if (!d->frame) {
    sirius_error("av_frame_alloc failed\n");
    return pollux_err_resource_alloc;
  }

  d->pkt = av_packet_alloc();
  if (!d->pkt) {
    sirius_error("av_packet_alloc failed\n");
    goto label_free1;
  }

  return pollux_err_ok;

label_free1:
  av_frame_free(&d->frame);

  return pollux_err_resource_alloc;
}

void ffmpeg_decoder_free_buffers(ffmpeg_decode_t *d) {
  if (d) {
    av_packet_free(&d->pkt);
    av_frame_free(&d->frame);
  }
}

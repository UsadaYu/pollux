#include "internal/internal_ff_decode.h"

#include "internal/internal_ff_codec.h"
#include "internal/internal_ff_erron.h"

void internal_ff_decode_resource_free(
    internal_ff_decode_t *d) {
  av_packet_free(&d->pkt);

  av_frame_free(&d->frame);
}

int internal_ff_decode_resource_alloc(
    internal_ff_decode_t *d) {
  int ret = 0;

  d->frame = av_frame_alloc();
  if (!d->frame) {
    sirius_error("av_frame_alloc\n");
    return pollux_err_memory_alloc;
  }

  d->pkt = av_packet_alloc();
  if (!d->pkt) {
    sirius_error("av_packet_alloc\n");
    ret = pollux_err_memory_alloc;
    goto label_free;
  }

  return 0;

label_free:
  internal_ff_decode_resource_free(d);

  return ret;
}

static inline void fmt_ctx_free(AVFormatContext *fc) {
  avformat_close_input(&fc);

  avformat_free_context(fc);
}

static bool fmt_ctx_alloc(AVFormatContext **fc,
                          const char *url) {
  int ret;

  /**
   * Allocate the `av` context, which is used to store
   * audio and video information.
   */
  if (!(*fc = avformat_alloc_context())) {
    sirius_error("avformat_alloc_context\n");
    return false;
  }

  /**
   * Open the input url, and request the appropriate
   * resource for the members in the `fc` based on the url.
   */
  sirius_infosp("Decode input url: %s\n", url);
  ret = avformat_open_input(fc, url, nullptr, nullptr);
  if (ret) {
    internal_ff_error(ret, avformat_open_input);
    goto label_free1;
  }

  /**
   * Read information from input url, and fill them into
   * `fc`.
   */
  ret = avformat_find_stream_info(*fc, nullptr);
  if (ret < 0) {
    internal_ff_error(ret, avformat_find_stream_info);
    goto label_free2;
  }

  return true;

label_free2:
  avformat_close_input(fc);

label_free1:
  avformat_free_context(*fc);
  *fc = nullptr;

  return false;
}

static inline void codec_ctx_free(AVCodecContext *cc) {
  /* This function will close `codec` and free `codec`. */
  avcodec_free_context(&cc);
}

static AVCodecContext *codec_ctx_alloc(
    AVCodecParameters *cp) {
  /**
   * @brief Find a decoder based on `codec_id`, `codec_id`
   * stands for the decoder used, such as H.264, HEVC, etc.
   */
  const AVCodec *c = avcodec_find_decoder(cp->codec_id);
  if (!c) {
    sirius_error("avcodec_find_decoder, codec_id: %d\n",
                 cp->codec_id);
    return nullptr;
  }

  /**
   * Allocate decoder context based on `codec` information.
   */
  AVCodecContext *cc = avcodec_alloc_context3(c);
  if (!cc) {
    sirius_error("avcodec_alloc_context3\n");
    return nullptr;
  }

  int ret;
  /* Copy the parameters `cp` to `cc`. */
  ret = avcodec_parameters_to_context(cc, cp);
  if (ret < 0) {
    internal_ff_error(ret, avcodec_parameters_to_context);
    goto label_free;
  }

  /**
   * Open the decoder and associate the decoder with the
   * `cc`.
   */
  ret = avcodec_open2(cc, c, nullptr);
  if (ret < 0) {
    internal_ff_error(ret, avcodec_open2);
    goto label_free;
  }

  return cc;

label_free:
  avcodec_free_context(&cc);

  return nullptr;
}

void internal_ff_decode_deinit(internal_ff_decode_t *d) {
  AVFormatContext **fc = &(d->fmt_ctx);
  AVCodecContext **cc = &(d->codec_ctx);

  if (*cc) {
    codec_ctx_free(*cc);
    *cc = nullptr;
  }

  if (*fc) {
    fmt_ctx_free(*fc);
    *fc = nullptr;
  }
}

/**
 * @note Decoder initialization, but does not request
 *  resources.
 */
[[nodiscard]] int internal_ff_decode_init(
    internal_ff_decode_t *d, enum AVMediaType media_type,
    const char *url) {
  AVFormatContext **fc = &(d->fmt_ctx);
  if (!fmt_ctx_alloc(fc, url))
    return pollux_err_resource_alloc;

  unsigned int *s_idx = &(d->stream_index);
  if (!internal_ff_codec_stream_get(*fc, media_type,
                                    s_idx))
    goto label_free;

  AVCodecContext **cc = &(d->codec_ctx);
  AVStream *s = (*fc)->streams[*s_idx];
  *cc = codec_ctx_alloc(s->codecpar);
  if (!*cc) goto label_free;

  return 0;

label_free:
  fmt_ctx_free(*fc);

  return pollux_err_resource_alloc;
}

void internal_ff_decode_free(internal_ff_decode_t *d) {
  internal_ff_decode_resource_free(d);

  internal_ff_decode_deinit(d);
}

/**
 * @note Decoder initialization, and request related
 *  resources.
 */
[[nodiscard]] int internal_ff_decode_alloc(
    internal_ff_decode_t *d, enum AVMediaType media_type,
    const char *url) {
  int ret;

  ret = internal_ff_decode_init(d, media_type, url);
  if (ret) return ret;

  ret = internal_ff_decode_resource_alloc(d);
  if (ret) internal_ff_decode_deinit(d);

  return ret;
}

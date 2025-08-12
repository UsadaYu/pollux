#include "internal/internal_ff_encode.h"

#include <libavutil/opt.h>
#include <sirius/sirius_time.h>

#include "internal/internal_ff_erron.h"
#include "pollux_erron.h"

static void codec_priv_set(AVCodecContext *cc,
                           enum AVCodecID codec_id) {
  int ret;
  void *p = cc->priv_data;

#define E(f) \
  if (ret) internal_ff_warn(ret, f);

  switch (codec_id) {
    case AV_CODEC_ID_H264:
      ret = av_opt_set(p, "preset", "superfast", 0);
      E(av_opt_set)
      ret = av_opt_set(p, "tune", "zerolatency", 0);
      E(av_opt_set)
      break;
    case AV_CODEC_ID_HEVC:
      break;
    case AV_CODEC_ID_AV1:
#ifdef FF_AV1_STV_AV1
      ret = av_opt_set_int(p, "preset", 12, 0);
      E(av_opt_set_int)  // encoding speed, 0 ~ 13
#elif defined(FF_AV1_LIBAOM)
      ret = av_opt_set_int(p, "cpu-used", 8, 0);
      E(av_opt_set_int)  // encoding speed, 0 ~ 8
#endif
      break;
    default:
      break;
  }

#undef E
}

/**
 * @brief Deinitializes the encoder context.
 *  Safely closes `AVIO` context and frees the format
 * context.
 *
 * @param[in,out] e Internal encoder handle.
 */
void internal_ff_encode_deinit(internal_ff_encode_t *e) {
  if (!e || !e->fmt_ctx) {
    return;
  }

  AVFormatContext *fc = e->fmt_ctx;

  if (fc->pb && !(fc->oformat->flags & AVFMT_NOFILE)) {
    avio_closep(&(fc->pb));
  }

  avformat_free_context(fc);
  e->fmt_ctx = nullptr;
}

/**
 * @brief Initializes the ffmpeg encoder for a given
 *  output. The output can be a local file or a network
 *  url.
 *
 * @param[out] e: Internal encoder handle.
 * @param[in] url: The output path or network url (e.g.,
 *  "output.mp4" or "rtmp://...").
 * @param[in] cont_fmt: The name of the container format
 *  (e.g., "mp4", "mpegts"). If `nullptr`, ffmpeg will
 *  guess from the destination name.
 *
 * @return 0 on success, or an error code on failure.
 */
[[nodiscard]] int internal_ff_encode_init(
    internal_ff_encode_t *e, const char *url,
    const char *cont_fmt) {
  int ret;

  ret = avformat_alloc_output_context2(
      &e->fmt_ctx, nullptr, cont_fmt, url);
  if (ret < 0) {
    internal_ff_error(ret, avformat_alloc_output_context2);
    return pollux_err_resource_alloc;
  }

  AVFormatContext *fc = e->fmt_ctx;

  sirius_infosp("Encode target url: %s\n", url);
  /*
   * For formats that require it (i.e., those without
   * AVFMT_NOFILE), open the I/O context. Protocols like
   * `RTMP` or `SRT` will handle their connections later in
   * the function `avformat_write_header`, so we skip
   * `avio_open` for them.
   */
  if (!(fc->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&fc->pb, url, AVIO_FLAG_WRITE);
    if (ret < 0) {
      internal_ff_error(ret, avio_open);
      goto label_free;
    }
    sirius_debgsp("Successfully opened `AVIO` for: %s\n",
                  url);
  } else {
    sirius_debgsp(
        "Format `%s` does not require explicit "
        "`avio_open` for: %s\n",
        fc->oformat->name, url);
  }

  return 0;

label_free:
  internal_ff_encode_deinit(e);
  return pollux_err_resource_alloc;
}

void internal_ff_encode_codec_free(
    internal_ff_encode_t *e) {
  // sirius_usleep(50 * 1000);
  sirius_usleep(0);

  AVCodecContext **cc = &(e->codec_ctx);
  if (*cc) {
    avcodec_free_context(cc);
  }
}

/**
 * @details
 *  (1) The result of `avformat_new_stream` should be freed
 *  by the function `avformat_free_context`, which is
 *  called in the function `internal_ff_encode_deinit`.
 */
[[nodiscard]] int internal_ff_encode_codec_alloc(
    internal_ff_encode_t *e,
    internal_ff_encode_param_t *pm,
    enum AVCodecID codec_id) {
  int ret;
  AVFormatContext *fc = e->fmt_ctx;

  if (!fc) {
    sirius_error("Null pointer: [fmt_ctx]\n");
    return pollux_err_not_init;
  }

  const AVCodec **c = &(e->codec);
  *c = avcodec_find_encoder(codec_id);
  if (!*c) {
    sirius_error("avcodec_find_encoder\n");
    return pollux_err_resource_alloc;
  }

  AVStream **s = &(e->stream);
  *s = avformat_new_stream(fc, *c);
  if (!*s) {
    sirius_error("avformat_new_stream\n");
    return pollux_err_resource_alloc;
  }

  AVCodecContext **cc = &(e->codec_ctx);
  *cc = avcodec_alloc_context3(*c);
  if (!*cc) {
    sirius_error("avcodec_alloc_context3\n");
    return pollux_err_resource_alloc;
  }

  (*cc)->codec_id = codec_id;
  (*cc)->bit_rate = pm->bit_rate;
  (*cc)->height = pm->height;
  (*cc)->width = pm->width;
  (*cc)->time_base = (AVRational){1, pm->frame_rate.num};
  (*cc)->framerate =
      av_make_q(pm->frame_rate.num, pm->frame_rate.den);
  (*cc)->gop_size = pm->gop_size;
  (*cc)->max_b_frames = pm->max_b_frames;
  (*cc)->pix_fmt = pm->pix_fmt;
  codec_priv_set(*cc, codec_id);

  if (fc->oformat->flags & AVFMT_GLOBALHEADER) {
    (*cc)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  ret = avcodec_open2(*cc, *c, nullptr);
  if (ret < 0) {
    internal_ff_error(ret, avcodec_open2);
    goto label_free;
  }

  AVCodecParameters *cp = (*s)->codecpar;
  ret = avcodec_parameters_from_context(cp, *cc);
  if (ret < 0) {
    internal_ff_error(ret,
                      avcodec_parameters_from_context);
    goto label_free;
  }

  (*s)->time_base = (*cc)->time_base;

  av_dump_format(fc, 0, fc->url, 1);

  return 0;

label_free:
  internal_ff_encode_codec_free(e);

  return pollux_err_resource_alloc;
}

#include "pollux_encode.h"

#include <libavutil/imgutils.h>
#include <sirius/sirius_math.h>
#include <sirius/sirius_mutex.h>
#include <sirius/sirius_time.h>

#include "cvt/cvt_codec_id.h"
#include "cvt/cvt_frame.h"
#include "cvt/cvt_pixel.h"
#include "internal/internal_ff_encode.h"
#include "internal/internal_ff_erron.h"
#include "pollux_erron.h"

typedef struct {
  /* Used to store the frame data to be encoded. */
  AVFrame *av_frame;
  AVPacket *pkt;

  sirius_mutex_handle mtx;

  unsigned int g_pts;
  unsigned int b_pts;

  pollux_encode_args_t args;
  /* Parameter setting flag. */
  bool param_set_flag;

  internal_ff_encode_t encode;

#define FIRST_NS_DEFAULT (-1)
  long long int first_time_ns;
} pollux_ctx_t;

static inline bool param_set(
    internal_ff_encode_t *e,
    const pollux_encode_args_t *args) {
  internal_ff_encode_param_t ep = {0};
  const pollux_img_t *m = &args->img;

  ep.bit_rate = args->bit_rate;
  ep.width = m->width;
  ep.height = m->height;
  ep.frame_rate.num = args->frame_rate.num;
  ep.frame_rate.den = args->frame_rate.den;
  ep.gop_size = args->gop_size;
  ep.max_b_frames = args->max_b_frames;

  enum AVPixelFormat fmt;
  if (!cvt_pix_plx_to_ff(m->fmt, &fmt)) return false;
  ep.pix_fmt = fmt;

  enum AVCodecID id;
  if (!cvt_codec_id_plx_to_ff(args->codec_id, &id))
    return false;

  if (internal_ff_encode_codec_alloc(e, &ep, id))
    return false;

  return true;
}

static inline void ff_cache_free(pollux_ctx_t *ctx) {
  AVPacket **k = &ctx->pkt;
  if (*k) {
    av_packet_free(k);
  }

  AVFrame **f = &ctx->av_frame;
  if (*f) {
    av_frame_free(f);
  }
}

static inline bool ff_cache_alloc(pollux_ctx_t *ctx) {
  AVFrame **f = &ctx->av_frame;
  *f = av_frame_alloc();
  if (!*f) {
    sirius_error("av_frame_alloc\n");
    return false;
  }

  AVPacket **k = &ctx->pkt;
  *k = av_packet_alloc();
  if (!*k) {
    sirius_error("av_packet_alloc\n");
    goto label_free;
  }

  return true;

label_free:
  ff_cache_free(ctx);

  return false;
}

static inline void resource_free(pollux_ctx_t *ctx) {
  ff_cache_free(ctx);

  sirius_mutex_destroy(&ctx->mtx);
}

static inline bool resource_alloc(pollux_ctx_t *ctx) {
  if (sirius_mutex_init(&ctx->mtx, nullptr)) return false;

  if (!ff_cache_alloc(ctx)) goto label_free;

  return true;

label_free:
  resource_free(ctx);

  return false;
}

/**
 * The `pts_set` function must be called after the
 * `avformat_write_header` function, which writes the
 * timestamp `AVRational` into the `time_base` of the
 * `stream`.
 */
static inline void pts_set(pollux_ctx_t *ctx) {
  /**
   * @brief `time_base` is the time base used to define
   *  the units of measurement for pts，1/90000 is
   *  commonly used, which is 90000 ticks per second.
   *  To ensure that video frames are evenly distributed
   *  over the time line, pts are generally calculated in
   *  the following ways.
   *
   * AVRational *r = &stream->time_base;
   * r->den: 90000
   * r->num: 1
   *
   * AVCodecContext *cc;
   * cc->framerate.num: fps
   */
  internal_ff_encode_t *e = &ctx->encode;
  AVStream *s = e->stream;
  AVRational *r = &s->time_base;
  AVCodecContext *cc = e->codec_ctx;

  ctx->g_pts = 0;
  ctx->b_pts = r->den / r->num / cc->framerate.num;
}

static force_inline bool send_and_receive(
    internal_ff_encode_t *e, AVFrame *f, AVPacket *k) {
  int ret;
  AVFormatContext *fc = e->fmt_ctx;
  AVCodecContext *cc = e->codec_ctx;
  AVStream *s = e->stream;

  ret = avcodec_send_frame(cc, f);
  if (ret) {
    internal_ff_error(ret, avcodec_send_frame);
    return false;
  }

  ret = avcodec_receive_packet(cc, k);
  if (ret) {
    if (likely(ret == AVERROR(EAGAIN))) {
      internal_ff_debg(ret, avcodec_receive_packet);
    } else {
      internal_ff_error(ret, avcodec_receive_packet);
    }
    goto label_free;
  }

  k->stream_index = s->index;
  ret = av_interleaved_write_frame(fc, k);
  if (ret) {
    internal_ff_error(ret, av_interleaved_write_frame);
    goto label_free;
  }

  av_packet_unref(k);
  return true;

label_free:
  av_packet_unref(k);
  return false;
}

static inline void pkg_ahead_time(pollux_ctx_t *ctx) {
  if (unlikely(FIRST_NS_DEFAULT == ctx->first_time_ns)) {
    ctx->first_time_ns = sirius_get_time_ns();
  }

  long long int per_gap_ns = 1000000000ULL *
                             ctx->args.frame_rate.den /
                             ctx->args.frame_rate.num;
  long long int ept_crt_ns = per_gap_ns * ctx->g_pts;
  long long int act_crt_ns =
      sirius_get_time_ns() - ctx->first_time_ns;
  long long int diff_ns =
      ept_crt_ns - act_crt_ns -
      per_gap_ns * ctx->args.pkg_ahead_nr;

  if (diff_ns > 0) {
    sirius_nsleep(diff_ns);
  }
}

static force_inline void frame_pts_set(
    pollux_ctx_t *ctx, AVFrame *dst,
    const pollux_frame_t *src) {
  // if (ctx->args.custom_pts) {
  // } else {
  dst->pts = ctx->g_pts * ctx->b_pts;
  // }

  ctx->g_pts++;
}

static inline void encoder_free(pollux_ctx_t *ctx) {
  internal_ff_encode_t *e = &ctx->encode;

  internal_ff_encode_codec_free(e);
  internal_ff_encode_deinit(e);

  memset(&ctx->args, 0, sizeof(pollux_encode_args_t));
}

static int encode_release(pollux_encode_t *h) {
  if (!h || !h->priv_data) return pollux_err_entry;

  auto ctx = (pollux_ctx_t *)(h->priv_data);

  sirius_mutex_lock(&ctx->mtx);

  if (!ctx->param_set_flag) goto label_free;

  encoder_free(ctx);

  ctx->param_set_flag = false;

label_free:
  sirius_mutex_unlock(&ctx->mtx);
  return 0;
}

/**
 * The `av_image_alloc` function cannot be called here,
 * because the code will not make a deep memory copy of
 * the image data after the `encode_send_frame` function
 * passes the frame data.
 */
static int encode_param_set(
    pollux_encode_t *h, const char *url,
    const pollux_encode_args_t *args) {
  if (!h || !h->priv_data || !url || !args)
    return pollux_err_entry;

  int ret = pollux_err_resource_alloc;
  auto ctx = (pollux_ctx_t *)(h->priv_data);
  internal_ff_encode_t *e = &ctx->encode;
  pollux_encode_args_t *dp = &ctx->args;

  sirius_mutex_lock(&ctx->mtx);
  if (ctx->param_set_flag) {
    encoder_free(ctx);
  }
  ctx->param_set_flag = false;

  memcpy(dp, args, sizeof(pollux_encode_args_t));
  dp->pkg_ahead_nr = sirius_max(0, dp->pkg_ahead_nr);
  dp->pkg_ahead_nr = sirius_min(4096, dp->pkg_ahead_nr);

  const char *ctf = pollux_cont_to_string(args->cont_fmt);
  if (internal_ff_encode_init(e, url, ctf))
    goto label_free;
  if (!param_set(e, args)) goto label_free;

  ctx->param_set_flag = true;
  sirius_mutex_unlock(&ctx->mtx);
  return 0;

label_free:
  encoder_free(ctx);
  sirius_mutex_unlock(&ctx->mtx);

  return ret;
}

#define ES(func)                                     \
  if (!h || !h->priv_data) return pollux_err_entry;  \
                                                     \
  int ret;                                           \
  auto ctx = (pollux_ctx_t *)(h->priv_data);         \
  internal_ff_encode_t *e = &ctx->encode;            \
  AVFormatContext *fc = e->fmt_ctx;                  \
                                                     \
  if (unlikely(!ctx->param_set_flag)) {              \
    sirius_error("The resource is uninitialized\n"); \
    return pollux_err_not_init;                      \
  }                                                  \
                                                     \
  ret = ET(fc) if (ret < 0) {                        \
    internal_ff_error(ret, func);                    \
    return pollux_err_file_write;                    \
  }

static int encode_stop(pollux_encode_t *h) {
#define ET(fc) av_write_trailer(fc);
  ES(av_write_trailer)
  return 0;
#undef ET
}

static int encode_start(pollux_encode_t *h) {
#define ET(fc) avformat_write_header(fc, nullptr);
  ES(avformat_write_header)
  pts_set(ctx);
  return 0;
#undef ET
}

static int encode_send_frame(pollux_encode_t *h,
                             const pollux_frame_t *r) {
  if (unlikely(!h || !h->priv_data || !r)) {
    return pollux_err_entry;
  }

  auto ctx = (pollux_ctx_t *)(h->priv_data);
  internal_ff_encode_t *e = &ctx->encode;
  AVPacket *k = ctx->pkt;
  AVFrame *f = ctx->av_frame;
  pollux_img_t *m = &ctx->args.img;

  if (unlikely(!ctx->param_set_flag)) {
    sirius_error("The resource is uninitialized\n");
    return pollux_err_not_init;
  }
  if (unlikely(m->width != r->width ||
               m->height != r->height ||
               m->fmt != r->fmt)) {
    sirius_error(
        "Image parameter error\n"
        "\t[Width] Exp: %d; Act: %d\n"
        "\t[Height] Exp: %d; Act: %d\n"
        "\t[Format] Exp: %d; Act: %d\n",
        m->width, r->width, m->height, r->height, m->fmt,
        r->fmt);
    return pollux_err_args;
  }

  if (unlikely(!cvt_frame_plx_to_ff(r, f)))
    return pollux_err_args;

  frame_pts_set(ctx, f, r);
  if (ctx->args.pkg_ahead_nr > 0) pkg_ahead_time(ctx);

  return send_and_receive(e, f, k) ? 0 : -1;
}

void pollux_encode_deinit(pollux_encode_t *handle) {
  if (!handle) return;

  if (!handle->priv_data) goto label_free;

  auto ctx = (pollux_ctx_t **)(&handle->priv_data);

  (void)encode_release(handle);

  (*ctx)->first_time_ns = FIRST_NS_DEFAULT;

  resource_free(*ctx);

  free(*ctx);
  *ctx = nullptr;

label_free:
  free(handle);
}

int pollux_encode_init(pollux_encode_t **handle) {
  int ret;

#define AC(h, t)                    \
  t *h = (t *)calloc(1, sizeof(t)); \
  if (!(h)) {                       \
    sirius_error("calloc\n");       \
    ret = pollux_err_memory_alloc;  \
    goto label_free;                \
  }
  AC(h, pollux_encode_t)
  AC(ctx, pollux_ctx_t)

  ctx->first_time_ns = FIRST_NS_DEFAULT;

  if (!resource_alloc(ctx)) {
    ret = pollux_err_resource_alloc;
    goto label_free;
  }

  h->priv_data = (void *)ctx;
  h->release = encode_release;
  h->param_set = encode_param_set;
  h->start = encode_start;
  h->stop = encode_stop;
  h->send_frame = encode_send_frame;

  *handle = h;
  return 0;

label_free:
  (void)pollux_encode_deinit(h);

  return ret;

#undef AC
}

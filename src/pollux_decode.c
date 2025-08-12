#include "pollux_decode.h"

#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <sirius/sirius_cond.h>
#include <sirius/sirius_errno.h>
#include <sirius/sirius_math.h>
#include <sirius/sirius_mutex.h>
#include <sirius/sirius_queue.h>
#include <sirius/sirius_thread.h>
#include <sirius/sirius_time.h>

#include "cvt/cvt_codec_id.h"
#include "cvt/cvt_frame.h"
#include "cvt/cvt_pixel.h"
#include "internal/internal_ff_decode.h"
#include "internal/internal_ff_erron.h"
#include "internal/internal_frame.h"
#include "internal/internal_sys.h"
#include "pollux_erron.h"
#include "pollux_frame.h"

typedef enum {
  frame_state_null,
  frame_state_end_file,
} frame_state_t;

typedef struct {
  frame_state_t frame_state;
} frame_t;

typedef struct {
  sirius_thread_handle thread;
  atomic_bool is_running;
  atomic_bool exit_flag;

  sirius_cond_handle cond;
  sirius_mutex_handle mtx;
} thread_t;

typedef struct {
  sirius_que_handle que_free, que_rst;
  /* Decoding result */
  pollux_frame_t *result[INTERNAL_FRAME_NR];

  /* Configuration parameter cache. */
  pollux_decode_args_t args;
  /* Parameter setting flag. */
  bool param_set_flag;
  /* Whether to enable image format conversion. */
  bool cvt_enable;

  /* Decode information of ffmpeg. */
  internal_ff_decode_t decode;
  /* Sws context. */
  struct SwsContext *sws_ctx;

  sirius_mutex_handle mtx;

  thread_t thread;
} decode_ctx_t;

static force_inline int cache_put(sirius_que_handle q,
                                  pollux_frame_t *r) {
  int ret =
      sirius_que_put(q, (size_t)r, sirius_timeout_none);
  if (ret) {
    sirius_error(
        "sirius_que_put: %d, the queue is illegally "
        "occupied\n",
        ret);
    ret = pollux_err_cache_overflow;
  }

  return ret;
}

static inline bool stream_seek(decode_ctx_t *ctx) {
  pollux_frame_t *r;
  sirius_que_handle q = ctx->que_free;
  thread_t *thread = &ctx->thread;

  while (sirius_que_get(q, (size_t *)&r, 2500) || !r) {
    if (thread->exit_flag) return false;

    sirius_warn("sirius_que_get\n");
  }

  auto rf = (internal_frame_t *)(r->priv_data);
  auto fs = (frame_t *)(rf->user_priv_data);
  fs->frame_state = frame_state_end_file;
  q = ctx->que_rst;
  if (cache_put(q, r)) return false;

  bool ret = true;
  sirius_cond_wait(&thread->cond, &thread->mtx);

  if (thread->exit_flag) return false;

  return ret;
}

static inline bool send_and_receive(decode_ctx_t *ctx,
                                    AVFrame *f) {
  internal_ff_decode_t *d = &ctx->decode;
  AVCodecContext *cc = d->codec_ctx;
  AVPacket *pkt = d->pkt;
  struct SwsContext *sc = ctx->sws_ctx;
  AVFrame *f_rcv = ctx->cvt_enable ? d->frame : f;

  /* Send packet to the decoder. */
  int ret = avcodec_send_packet(cc, pkt);
  if (unlikely(ret)) {
    internal_ff_debg(ret, avcodec_send_packet);
    if (ret == AVERROR(EAGAIN)) {
      (void)avcodec_receive_frame(cc, f_rcv);
      return false;
    }
  }

  ret = avcodec_receive_frame(cc, f_rcv);
  if (unlikely(ret)) {
    internal_ff_debg(ret, avcodec_receive_frame);
    return false;
  }

  if (ctx->cvt_enable) {
    f->height = sws_scale(
        sc, (const uint8_t *const *)(f_rcv->data),
        f_rcv->linesize, 0, cc->height, f->data,
        f->linesize);
    f->nb_samples = f_rcv->nb_samples;
    f->pict_type = f_rcv->pict_type;
    f->pts = f_rcv->pts;
    f->pkt_dts = f_rcv->pkt_dts;
    f->time_base = f_rcv->time_base;
    f->sample_rate = f_rcv->sample_rate;
  }

  return true;
}

static pollux_frame_t *obtain_free_frame(
    decode_ctx_t *ctx, thread_t *thread) {
  int ret;
  pollux_frame_t *r;
  sirius_que_handle h = ctx->que_free;

  do {
    ret = sirius_que_get(h, (size_t *)&r, 1000);
    if (thread->exit_flag) return nullptr;
  } while (ret == sirius_err_timeout);

  if (ret || !r) {
    sirius_error("sirius_que_get, pkg skip\n");
    return nullptr;
  }
  return r;
}

/**
 * @brief Read a packet from fmt_ctx.
 *
 * @return
 *  - (1) 0 on success.
 *
 *  - (2) 1 indicates encountering `EOF` but `stream_seek`.
 *
 *  - (3) -1 indicates a fatal error has occurred and
 *  decoding needs to be exited.
 */
static int read_packet_locked(decode_ctx_t *ctx) {
  int ret;
  thread_t *thread = &ctx->thread;
  internal_ff_decode_t *d = &ctx->decode;

  sirius_mutex_lock(&thread->mtx);
  ret = av_read_frame(d->fmt_ctx, d->pkt);
  if (ret == 0) {
    sirius_mutex_unlock(&thread->mtx);
    return 0;
  }

  if (ret == AVERROR_EOF) {
    if (stream_seek(ctx)) {
      sirius_mutex_unlock(&thread->mtx);
      return 1;
    }
    sirius_mutex_unlock(&thread->mtx);
    return -1;
  }

  internal_ff_error(ret, av_read_frame);
  sirius_mutex_unlock(&thread->mtx);
  return -1;
}

static void decode_thread(void *args) {
  auto ctx = (decode_ctx_t *)args;
  internal_ff_decode_t *d = &ctx->decode;
  thread_t *thread = &ctx->thread;

  thread->is_running = true;
  while (!thread->exit_flag) {
    int r = read_packet_locked(ctx);
    if (r == 1) continue;
    if (r == -1) break;

    if (d->pkt->stream_index != d->stream_index)
      goto label_free;

    pollux_frame_t *frame = obtain_free_frame(ctx, thread);
    if (!frame) break;

    auto rf = (internal_frame_t *)(frame->priv_data);
    sirius_que_handle h = ctx->que_free;

    if (send_and_receive(ctx, rf->av_frame))
      h = ctx->que_rst;

    if (cache_put(h, frame)) break;

  label_free:
    av_packet_unref(d->pkt);
  }

  av_packet_unref(d->pkt);
  thread->is_running = false;
}

static void frame_free(decode_ctx_t *ctx) {
  for (int i = 0; i < INTERNAL_FRAME_NR; i++) {
    pollux_frame_t **r = ctx->result + i;

    if (r && (*r)) {
      auto rf = (internal_frame_t *)((*r)->priv_data);

      if (rf) {
        auto fs = (frame_t **)(&rf->user_priv_data);

        if (*fs) {
          free(*fs);
          *fs = nullptr;
        }
      }

      pollux_frame_free(r);
    }
  }

#define Q(q)                             \
  if (q) {                               \
    if (sirius_que_free(q)) {            \
      sirius_error("sirius_que_free\n"); \
    } else {                             \
      q = nullptr;                       \
    }                                    \
  }
  Q(ctx->que_rst);
  Q(ctx->que_free);

#undef Q
}

static bool frame_alloc(decode_ctx_t *ctx,
                        const pollux_img_t *img,
                        unsigned short cache_nr) {
  auto elem_nr = sirius_min(cache_nr, INTERNAL_FRAME_NR);
  elem_nr = sirius_max(1, elem_nr);

  sirius_que_t c = {0};
  c.elem_nr = elem_nr;
  c.que_type = sirius_que_type_mtx;
#define Q(q)                            \
  if (sirius_que_alloc(&c, &q)) {       \
    sirius_error("sirius_que_alloc\n"); \
    goto label_free;                    \
  }
  Q(ctx->que_free);
  Q(ctx->que_rst);

  pollux_frame_t *r;
  for (int i = 0; i < elem_nr; i++) {
    if (pollux_frame_alloc(img, &r)) goto label_free;
    ctx->result[i] = r;

    frame_t *fs = calloc(1, sizeof(frame_t));
    if (!fs) {
      sirius_error("calloc\n");
      goto label_free;
    }
    fs->frame_state = frame_state_null;

    auto rf = (internal_frame_t *)(r->priv_data);
    rf->user_priv_data = (void *)fs;

    if (cache_put(ctx->que_free, r)) goto label_free;
  }

  return true;

label_free:
  frame_free(ctx);

  return false;
#undef Q
}

static void resource_free(decode_ctx_t *ctx) {
  thread_t *thread = &ctx->thread;

  sirius_cond_destroy(&thread->cond);
  sirius_mutex_destroy(&thread->mtx);

  frame_free(ctx);
}

static bool resource_alloc(
    decode_ctx_t *ctx, const pollux_decode_args_t *args) {
  thread_t *thread = &ctx->thread;
  pollux_img_t *img =
      ctx->cvt_enable ? args->fmt_cvt_img : nullptr;

  if (!frame_alloc(ctx, img, args->result_cache_nr))
    return false;

  if (sirius_mutex_init(&thread->mtx, nullptr))
    goto label_free1;

  if (sirius_cond_init(&thread->cond, nullptr))
    goto label_free2;

  return true;

label_free2:
  sirius_mutex_destroy(&thread->mtx);

label_free1:
  frame_free(ctx);

  return false;
}

static void stream_fill(pollux_decode_stream_info_t *s,
                        const internal_ff_decode_t *d) {
  AVCodecContext *cc = d->codec_ctx;

  s->video_width = cc->width;
  s->video_height = cc->height;
  s->bit_rate = cc->bit_rate;
  s->video_frame_rate.num = cc->framerate.num;
  s->video_frame_rate.den = cc->framerate.den;
  s->max_b_frames = cc->max_b_frames;
  s->gop_size = cc->gop_size;

  cvt_pix_ff_to_plx(cc->pix_fmt, &s->video_img_fmt);
  cvt_codec_id_ff_to_plx(cc->codec_id, &s->video_codec_id);

  AVFormatContext *fc = d->fmt_ctx;
  s->duration = fc->duration;
}

static void decode_deinit(decode_ctx_t *ctx) {
  thread_t *thread = &ctx->thread;
  if (!thread->is_running) return;

  thread->exit_flag = true;

  for (int i = 20; i-- && thread->is_running;) {
    sirius_mutex_lock(&thread->mtx);
    sirius_cond_broadcast(&thread->cond);
    sirius_mutex_unlock(&thread->mtx);
    sirius_usleep(200 * 1000);
  }

  if (thread->is_running) {
    sirius_warnsp("Thread (%llu) will force exit\n",
                  sirius_thread_id);
    sirius_thread_cancel(thread->thread);
  }
  sirius_thread_join(thread->thread, nullptr);
  memset(thread, 0, sizeof(thread_t));
}

static bool decode_init(decode_ctx_t *ctx) {
  thread_t *thread = &ctx->thread;

  thread->exit_flag = false;
  thread->is_running = true;
  if (sirius_thread_create(&thread->thread, nullptr,
                           (void *)decode_thread,
                           (void *)ctx)) {
    thread->is_running = false;
    goto label_free;
  }
  return true;

label_free:
  decode_deinit(ctx);

  return false;
}

static void decode_sws_deinit(decode_ctx_t *ctx) {
  if (ctx->cvt_enable) {
    if (ctx->sws_ctx) {
      sws_freeContext(ctx->sws_ctx);
      ctx->sws_ctx = nullptr;
    }

    ctx->cvt_enable = false;
  }
}

static bool decode_sws_init(
    decode_ctx_t *ctx, const internal_ff_decode_t *d) {
  AVCodecContext *cc = d->codec_ctx;
  pollux_img_t *img = ctx->args.fmt_cvt_img;
  const char *err_str =
      "The decoding image configuration may be invalid:";
  const char *dft_str = "Use the default values";

  ctx->cvt_enable = false;

  if (!img) return true;

  enum AVPixelFormat fmt;
  if (!cvt_pix_plx_to_ff(img->fmt, &fmt)) {
    sirius_warnsp("%s [Format: %d] %s\n", img->fmt,
                  dft_str);
    fmt = cc->pix_fmt;
    cvt_pix_ff_to_plx(fmt, &img->fmt);
  }
  if (img->width <= 0 || img->height <= 0) {
    sirius_warnsp("%s [Width: %d; Height: %d] %s\n",
                  err_str, img->width, img->height,
                  dft_str);
    img->width = cc->width;
    img->height = cc->height;
  }
  if (img->align <= 0) {
    sirius_warnsp("%s [Align: %d] %s\n", err_str,
                  img->align, dft_str);
    img->align = 32;
  }

  sirius_infosp(
      "Decoding target information:\n"
      "\tImage width: %d\n"
      "\tImage height: %d\n"
      "\tImage format: %d\n"
      "\tImage align: %d\n",
      img->width, img->height, img->fmt, img->align);

  if (fmt == cc->pix_fmt && cc->width == img->width &&
      cc->height == img->height &&
      cc->width % img->align == 0) {
    sirius_infosp(
        "The format of the source video image is "
        "consistent with the configuration, so image "
        "conversion will not be enabled\n");
    return true;
  }

  struct SwsContext **sc = &ctx->sws_ctx;
  *sc = sws_getContext(cc->width, cc->height, cc->pix_fmt,
                       img->width, img->height, fmt,
                       SWS_BILINEAR, nullptr, nullptr,
                       nullptr);
  if (!*sc) {
    sirius_error("sws_getContext\n");
    return false;
  }

  ctx->cvt_enable = true;
  return true;
}

static void decode_internal_deinit(decode_ctx_t *ctx) {
  pollux_decode_args_t *args = &ctx->args;
  internal_ff_decode_t *d = &ctx->decode;

  decode_sws_deinit(ctx);

  if (args->fmt_cvt_img) {
    free(args->fmt_cvt_img);
    args->fmt_cvt_img = nullptr;
  }

  internal_ff_decode_free(d);
}

static bool decode_internal_init(
    decode_ctx_t *ctx, const char *url,
    const pollux_decode_args_t *src) {
  internal_ff_decode_t *d = &ctx->decode;
  pollux_decode_args_t *dst = &ctx->args;

  if (internal_ff_decode_alloc(d, AVMEDIA_TYPE_VIDEO,
                               url)) {
    return false;
  }

  dst->fmt_cvt_img = nullptr;
  if (src) {
    memcpy(dst, src, sizeof(pollux_decode_args_t));
    if (src->fmt_cvt_img) {
      dst->fmt_cvt_img = calloc(1, sizeof(pollux_img_t));
      if (!dst->fmt_cvt_img) {
        sirius_error("calloc\n");
        goto label_free;
      }
      memcpy(dst->fmt_cvt_img, src->fmt_cvt_img,
             sizeof(pollux_img_t));
    }
  }

  if (!decode_sws_init(ctx, d)) goto label_free;

  return true;

label_free:
  decode_internal_deinit(ctx);

  return false;
}

static inline void decoder_free(decode_ctx_t *ctx) {
  decode_deinit(ctx);
  resource_free(ctx);
  decode_internal_deinit(ctx);
}

static int decode_release(pollux_decode_t *h) {
  if (!h || !h->priv_data) return pollux_err_entry;

  auto ctx = (decode_ctx_t *)(h->priv_data);

  sirius_mutex_lock(&ctx->mtx);

  if (!ctx->param_set_flag) goto label_free;

  memset(&h->stream, 0, sizeof(h->stream));
  decoder_free(ctx);

  ctx->param_set_flag = false;

label_free:
  sirius_mutex_unlock(&ctx->mtx);
  return 0;
}

static int decode_param_set(
    pollux_decode_t *h, const char *url,
    const pollux_decode_args_t *args) {
  if (!h || !h->priv_data || !url) return pollux_err_entry;

  int ret = pollux_err_resource_alloc;
  auto ctx = (decode_ctx_t *)(h->priv_data);

  sirius_mutex_lock(&ctx->mtx);
  if (ctx->param_set_flag) {
    decoder_free(ctx);
  }
  ctx->param_set_flag = false;

  if (!decode_internal_init(ctx, url, args))
    goto label_free;
  if (!resource_alloc(ctx, &ctx->args)) goto label_free;
  if (!decode_init(ctx)) goto label_free;

  stream_fill(&h->stream, &ctx->decode);
  ctx->param_set_flag = true;
  sirius_mutex_unlock(&ctx->mtx);
  return 0;

label_free:
  decoder_free(ctx);
  sirius_mutex_unlock(&ctx->mtx);

  return ret;
}

static int decode_result_free(pollux_decode_t *h,
                              pollux_frame_t *rst) {
  if (!h || !h->priv_data || !rst) return pollux_err_entry;

  auto ctx = (decode_ctx_t *)(h->priv_data);

  if (unlikely(!ctx->param_set_flag)) {
    sirius_error("The resource is uninitialized\n");
    return pollux_err_not_init;
  }

  return cache_put(ctx->que_free, rst);
}

static force_inline void result_ret_debg(int ret) {
  switch (ret) {
    case 0:
      break;
    case pollux_err_url_read_end:
      sirius_infosp("Decoded to end of the url\n");
      break;
    case pollux_err_timeout:
      sirius_warn(
          "No valid decoding result, "
          "timeout for getting the result\n");
      break;
    case pollux_err_resource_alloc:
      sirius_error("Failed to get the result cache\n");
      break;
    case pollux_err_not_init:
      sirius_error(
          "No valid configuration, "
          "the decoding thread is not started\n");
      break;
    default:
      sirius_warn("Unknown error\n");
      break;
  }
}

static int decode_result_get(pollux_decode_t *h,
                             pollux_frame_t **rst,
                             unsigned long milliseconds) {
  if (!h || !h->priv_data || !rst) return pollux_err_entry;

  int ret = 0;
  auto ctx = (decode_ctx_t *)(h->priv_data);

  sirius_mutex_lock(&ctx->mtx);
  *rst = nullptr;

  pollux_frame_t *r = nullptr;
  ret = sirius_que_get(ctx->que_rst, (size_t *)&r,
                       milliseconds);
  if (unlikely(ret) || unlikely(!r)) {
    if (ctx->thread.is_running) {
      ret = ret == sirius_err_timeout
                ? pollux_err_timeout
                : pollux_err_resource_alloc;
    } else {
      ret = pollux_err_not_init;
    }
    goto label_free;
  }

  auto rf = (internal_frame_t *)(r->priv_data);
  auto fs = (frame_t *)(rf->user_priv_data);
  if (unlikely(fs->frame_state != frame_state_null)) {
    switch (fs->frame_state) {
      case frame_state_end_file:
        ret = pollux_err_url_read_end;
        break;
      default:
        ret = -1;
        break;
    }
    fs->frame_state = frame_state_null;
    cache_put(ctx->que_free, r);
    goto label_free;
  }

  AVFrame *f = rf->av_frame;
  if (unlikely(!cvt_frame_ff_to_plx(f, r))) {
    ret = pollux_err_args;
    goto label_free;
  }
  *rst = r;

label_free:
  sirius_mutex_unlock(&ctx->mtx);

  if (unlikely(ret)) result_ret_debg(ret);
  return ret;
}

static int decode_seek_file(pollux_decode_t *h,
                            long long int min_ts,
                            long long int ts,
                            long long int max_ts) {
  if (!h || !h->priv_data) return pollux_err_entry;

  auto ctx = (decode_ctx_t *)(h->priv_data);
  thread_t *thread = &ctx->thread;

  if (unlikely(!ctx->param_set_flag)) {
    sirius_error("The resource is uninitialized\n");
    return pollux_err_not_init;
  }

  int ret = 0;
  internal_ff_decode_t *d = &ctx->decode;

  sirius_mutex_lock(&thread->mtx);

  ret = avformat_seek_file(d->fmt_ctx, d->stream_index,
                           min_ts, ts, max_ts,
                           AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    internal_ff_error(ret, avformat_seek_file);
    thread->exit_flag = true;
    sirius_mutex_unlock(&thread->mtx);
    return -1;
  }

  sirius_cond_signal(&thread->cond);
  sirius_mutex_unlock(&thread->mtx);

  return ret;
}

void pollux_decode_deinit(pollux_decode_t *handle) {
  if (!handle) return;
  if (!handle->priv_data) goto label_free;

  auto ctx = (decode_ctx_t **)(&handle->priv_data);

  (void)decode_release(handle);

  sirius_mutex_destroy((&(*ctx)->mtx));

  free(*ctx);
  *ctx = nullptr;

label_free:
  free(handle);
}

int pollux_decode_init(pollux_decode_t **handle) {
  int ret = 0;

#define AC(h, t)                    \
  t *h = (t *)calloc(1, sizeof(t)); \
  if (!(h)) {                       \
    sirius_error("calloc\n");       \
    ret = pollux_err_memory_alloc;  \
    goto label_free;                \
  }
  AC(h, pollux_decode_t)
  AC(ctx, decode_ctx_t)

  if (sirius_mutex_init(&ctx->mtx, nullptr))
    goto label_free;

  h->priv_data = (void *)ctx;
  h->param_set = decode_param_set;
  h->release = decode_release;
  h->result_free = decode_result_free;
  h->result_get = decode_result_get;
  h->seek_file = decode_seek_file;

  *handle = h;
  return 0;

label_free:
  pollux_decode_deinit(h);

  return ret;
#undef AC
}

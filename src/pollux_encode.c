#include "pollux/pollux_encode.h"

#include <libavutil/imgutils.h>
#include <sirius/sirius_cond.h>
#include <sirius/sirius_errno.h>
#include <sirius/sirius_math.h>
#include <sirius/sirius_mutex.h>
#include <sirius/sirius_queue.h>
#include <sirius/sirius_time.h>

#include "pollux/internal/codec/codec.h"
#include "pollux/internal/codec/ffmpeg_encode.h"
#include "pollux/internal/ffmpeg_cvt/codec_id.h"
#include "pollux/internal/ffmpeg_cvt/frame.h"
#include "pollux/internal/ffmpeg_cvt/pixel.h"
#include "pollux/internal/thread.h"
#include "pollux/internal/util.h"
#include "pollux/pollux_erron.h"

typedef struct {
  thread_t thread;

  sirius_cond_handle cond;
  sirius_mutex_handle mtx;
} thread_s;

typedef struct {
  sirius_mutex_handle mtx;

  thread_s thread;

  AVFrame *av_frame;

  int64_t frame_index;
  int64_t base_pts;

  pollux_encode_args_t args;
  atomic_bool param_set_flag;

  ffmpeg_encode_t encode;

  atomic_bool encoder_eof_flag;
} encode_ctx_s;

/**
 * @note The `encoder_pts_init` function must be called after the
 * `avformat_write_header` function, which writes the timestamp `AVRational`
 * into the `time_base` of the `stream`.
 */
static inline void encoder_pts_init(encode_ctx_s *ctx) {
  /**
   * @brief `time_base` is the time base used to define the units of
   * measurement for pts，1/90000 is commonly used, which is 90000 ticks per
   * second. To ensure that video frames are evenly distributed over the time
   * line, pts are generally calculated in the following ways.
   *
   * AVRational *r = &stream->time_base;
   * r->den: 90000
   * r->num: 1
   *
   * AVCodecContext *cc;
   * cc->framerate.num: fps
   */
  ffmpeg_encode_t *e = &ctx->encode;
  AVStream *s = e->stream;
  AVCodecContext *cc = e->codec_ctx;
  AVRational r = s->time_base;

  ctx->frame_index = 0;
  ctx->base_pts = r.den / r.num / cc->framerate.num * cc->framerate.den;
}

static force_inline void frame_pts_set(encode_ctx_s *ctx, AVFrame *dst,
                                       const pollux_frame_t *src) {
  // if (ctx->args.custom_pts) {
  //   dst->pts = ...;
  // }

  dst->pts = ctx->frame_index * ctx->base_pts;
  ctx->frame_index++;
}

static void thread_receive_and_write(void *args) {
  encode_ctx_s *ctx = (encode_ctx_s *)args;
  ffmpeg_encode_t *e = &ctx->encode;
  thread_s *thread = &ctx->thread;
  thread_t *threadt = &thread->thread;
  AVPacket *pkt = av_packet_alloc();

  int ret;
  threadt->is_running = true;
  ctx->encoder_eof_flag = false;
  while (!threadt->exit_flag) {
    av_packet_unref(pkt);

    sirius_mutex_lock(&ctx->mtx);
    ret = avcodec_receive_packet(e->codec_ctx, pkt);
    sirius_mutex_unlock(&ctx->mtx);

    if (ret == AVERROR(EAGAIN)) {
      sirius_mutex_lock(&thread->mtx);
      sirius_cond_signal(&thread->cond);
      sirius_mutex_unlock(&thread->mtx);
      continue;
    } else if (unlikely(ret == AVERROR_EOF)) {
      ctx->encoder_eof_flag = true;
      break;
    } else if (unlikely(ret < 0)) {
      ffmpeg_error(ret, "avcodec_receive_packet");
      break;
    }

    pkt->stream_index = e->stream->index;

    sirius_debgsp("Packer pts: %lld\n", pkt->pts);
    sirius_mutex_lock(&ctx->mtx);
    ret = av_interleaved_write_frame(e->fmt_ctx, pkt);
    sirius_mutex_unlock(&ctx->mtx);
    if (unlikely(ret < 0)) {
      ffmpeg_error(ret, "av_interleaved_write_frame");
      break;
    }
  }

  av_packet_free(&pkt);
  sirius_infosp("Encoding thread has exited\n");
  threadt->is_running = false;
  sirius_mutex_lock(&thread->mtx);
  sirius_cond_broadcast(&thread->cond);
  sirius_mutex_unlock(&thread->mtx);
}

static inline void encoder_ffmpeg_deinit(encode_ctx_s *ctx) {
  ffmpeg_encode_t *e = &ctx->encode;

  ffmpeg_encoder_ctx_free(e);
  ffmpeg_encoder_deinit(e);
}

static inline bool encoder_ffmpeg_init(encode_ctx_s *ctx, const char *url,
                                       const char *cont_fmt,
                                       const pollux_encode_args_t *args) {
  ffmpeg_encode_t *e = &ctx->encode;
  ffmpeg_encode_args_t ep = {0};
  const pollux_img_t img = args->img;
  enum AVCodecID id;

  if (ffmpeg_encoder_init(e, url, cont_fmt))
    return false;

  ep.bit_rate = args->bit_rate;
  ep.width = img.width;
  ep.height = img.height;
  ep.frame_rate.num = args->frame_rate.num;
  ep.frame_rate.den = args->frame_rate.den;
  ep.gop_size = args->gop_size;
  ep.max_b_frames = args->max_b_frames;
  ep.thread_count = args->thread_count;

  if (!cvt_pix_plx_to_ff(img.fmt, &ep.pix_fmt) ||
      !cvt_codec_id_plx_to_ff(args->codec_id, &id) ||
      ffmpeg_encoder_ctx_alloc(e, &ep, id)) {
    goto label_free1;
  }

  return true;

label_free1:
  ffmpeg_encoder_deinit(e);

  return false;
}

static inline void ffmpeg_resource_free(encode_ctx_s *ctx) {
  AVFrame **f = &ctx->av_frame;
  if (*f)
    av_frame_free(f);
}

static inline bool ffmpeg_resource_alloc(encode_ctx_s *ctx) {
  AVFrame **f = &ctx->av_frame;
  *f = av_frame_alloc();
  if (!*f) {
    sirius_error("av_frame_alloc\n");
    return false;
  }

  return true;
}

static inline void sig_resource_free(encode_ctx_s *ctx) {
  thread_s *thread = &ctx->thread;

  sirius_cond_destroy(&thread->cond);
  sirius_mutex_destroy(&thread->mtx);
}

static inline bool sig_resource_alloc(encode_ctx_s *ctx) {
  thread_s *thread = &ctx->thread;

  if (sirius_mutex_init(&thread->mtx, nullptr))
    return false;
  if (sirius_cond_init(&thread->cond, nullptr))
    goto label_free1;

  return true;

label_free1:
  sirius_mutex_destroy(&thread->mtx);

  return false;
}

static inline void encoder_resource_free(encode_ctx_s *ctx) {
  sirius_mutex_destroy(&ctx->mtx);
  sig_resource_free(ctx);
  ffmpeg_resource_free(ctx);
}

static inline bool encoder_resource_alloc(encode_ctx_s *ctx) {
  if (!ffmpeg_resource_alloc(ctx))
    return false;
  if (!sig_resource_alloc(ctx))
    goto label_free1;
  if (sirius_mutex_init(&ctx->mtx, nullptr))
    goto label_free2;

  return true;

label_free2:
  sig_resource_free(ctx);
label_free1:
  encoder_resource_free(ctx);

  return false;
}

static inline void encoder_deinit(encode_ctx_s *ctx) {
  encoder_resource_free(ctx);
  encoder_ffmpeg_deinit(ctx);
}

static inline bool encoder_init(encode_ctx_s *ctx, const char *url,
                                const pollux_encode_args_t *args) {
  pollux_encode_args_t *eargs = &ctx->args;

  memcpy(eargs, args, sizeof(pollux_encode_args_t));

  if (!encoder_ffmpeg_init(ctx, url, pollux_cont_enum_to_string(args->cont_fmt),
                           eargs))
    return false;
  if (!encoder_resource_alloc(ctx))
    goto label_free1;

  return true;

label_free1:
  encoder_ffmpeg_deinit(ctx);

  return false;
}

static inline int send_frame(encode_ctx_s *ctx, AVFrame *f) {
  int ret;
  ffmpeg_encode_t *e = &ctx->encode;
  thread_s *thread = &ctx->thread;
  thread_t *threadt = &thread->thread;

  while (true) {
    if (unlikely(!threadt->is_running))
      return pollux_err_not_init;

    sirius_mutex_lock(&ctx->mtx);
    ret = avcodec_send_frame(e->codec_ctx, f);
    sirius_mutex_unlock(&ctx->mtx);

    if (ret == AVERROR(EAGAIN)) {
      sirius_mutex_lock(&thread->mtx);
      sirius_cond_wait(&thread->cond, &thread->mtx);
      sirius_mutex_unlock(&thread->mtx);
      continue;
    } else if (unlikely(ret < 0)) {
      ffmpeg_error(ret, "avcodec_send_frame");
      threadt->exit_flag = true;
      return pollux_err_resource_alloc;
    }
    break;
  }

  return 0;
}

static inline void encoder_receive_thread_stop(encode_ctx_s *ctx) {
  thread_s *thread = &ctx->thread;
  thread_t *threadt = &thread->thread;

  if (!threadt->create_flag)
    return;

  if (!threadt->is_running) {
    sirius_thread_join(threadt->thread, nullptr);
    goto label_free;
  }

  threadt->exit_flag = true;

  for (int i = 20; i-- && threadt->is_running;) {
    sirius_usleep(200 * 1000);
  }

  if (threadt->is_running) {
    sirius_warnsp("Thread (%llu) will force exit\n", sirius_thread_id);
    sirius_thread_cancel(threadt->thread);
  }
  sirius_thread_join(threadt->thread, nullptr);

label_free:
  memset(thread, 0, sizeof(thread_s));
}

static inline bool encoder_receive_thread_start(encode_ctx_s *ctx) {
  thread_s *thread = &ctx->thread;
  thread_t *threadt = &thread->thread;

  threadt->exit_flag = false;
  threadt->is_running = true;
  if (sirius_thread_create(&threadt->thread, nullptr,
                           (void *)thread_receive_and_write, (void *)ctx)) {
    threadt->is_running = false;
    return false;
  } else {
    threadt->create_flag = true;
  }

  return true;
}

/**
 * @return `pollux_err_t`.
 */
static inline int flush_last_frames(encode_ctx_s *ctx) {
  int ret;
  ffmpeg_encode_t *e = &ctx->encode;
  thread_s *thread = &ctx->thread;
  thread_t *threadt = &thread->thread;

  while (true) {
    if (unlikely(!threadt->is_running))
      return pollux_err_not_init;

    sirius_mutex_lock(&ctx->mtx);
    ret = avcodec_send_frame(e->codec_ctx, nullptr);
    sirius_mutex_unlock(&ctx->mtx);

    if (ret != AVERROR(EAGAIN))
      break;
  }

  if (ret < 0) {
    ffmpeg_error(ret, "avcodec_send_frame (flushing)");
    return pollux_err_stream_flush;
  }

  while (threadt->is_running) {
    if (ctx->encoder_eof_flag)
      return 0;
    sirius_nsleep(100);
  }

  return ctx->encoder_eof_flag ? 0 : pollux_err_stream_flush;
}

/**
 * @return `pollux_err_t`.
 */
static inline int write_url_tail(ffmpeg_encode_t *e) {
  int ret;
  AVFormatContext *fc = e->fmt_ctx;

  ret = av_write_trailer(fc);
  if (ret != 0) {
    ffmpeg_error(ret, "av_write_trailer");
    return pollux_err_file_write;
  }

  return 0;
}

static inline void encoder_release(encode_ctx_s *ctx) {
  if (!ctx->param_set_flag)
    return;

  encoder_deinit(ctx);

  ctx->param_set_flag = false;
}

/**
 * @note The `av_image_alloc` function cannot be called here, because the code
 * will not make a deep memory copy of the image data after the
 * `encode_send_frame` function passes the frame data.
 */
static inline int encoder_param_set(encode_ctx_s *ctx, const char *url,
                                    const pollux_encode_args_t *args) {
  if (ctx->param_set_flag)
    encoder_deinit(ctx);

  ctx->param_set_flag = false;

  if (!encoder_init(ctx, url, args))
    return pollux_err_resource_alloc;

  ctx->param_set_flag = true;

  return 0;
}

static inline int encoder_start(encode_ctx_s *ctx) {
  int ret;
  ffmpeg_encode_t *e = &ctx->encode;

  if (unlikely(!ctx->param_set_flag)) {
    sirius_error("The resource is uninitialized\n");
    return pollux_err_not_init;
  }

  if ((ret = ffmpeg_encoder_open(e)) != 0)
    return pollux_err_resource_alloc;

  if (!encoder_receive_thread_start(ctx)) {
    ret = pollux_err_resource_alloc;
    goto label_free1;
  }

  if ((ret = avformat_write_header(e->fmt_ctx, nullptr)) < 0) {
    ffmpeg_error(ret, "avformat_write_header");
    ret = pollux_err_file_write;
    goto label_free2;
  }

  encoder_pts_init(ctx);

  return 0;

label_free2:
  encoder_receive_thread_stop(ctx);
label_free1:
  ffmpeg_encoder_close(e);

  return ret;
}

static inline int encoder_stop(encode_ctx_s *ctx) {
  int ret;
  ffmpeg_encode_t *e = &ctx->encode;

  if (unlikely(!ctx->param_set_flag)) {
    sirius_error("The resource is uninitialized\n");
    return pollux_err_not_init;
  }

  if ((ret = flush_last_frames(ctx)) != 0)
    return ret;

  if ((ret = write_url_tail(e)) != 0)
    return ret;

  encoder_receive_thread_stop(ctx);
  ffmpeg_encoder_close(e);

  return 0;
}

static inline int encoder_send_frame(encode_ctx_s *ctx,
                                     const pollux_frame_t *r) {
  AVFrame *f = ctx->av_frame;
  pollux_img_t *img = &ctx->args.img;

  if (unlikely(!ctx->param_set_flag)) {
    sirius_error("The resource is uninitialized\n");
    return pollux_err_not_init;
  }

  if (unlikely(img->width != r->width || img->height != r->height ||
               img->fmt != r->fmt)) {
    sirius_error(
      "Invalid image parameter\n"
      "\t[Width] Exp: %d; Act: %d\n"
      "\t[Height] Exp: %d; Act: %d\n"
      "\t[Format] Exp: %d; Act: %d\n",
      img->width, r->width, img->height, r->height, img->fmt, r->fmt);
    return pollux_err_args;
  }

  if (unlikely(!cvt_frame_plx_to_ff(r, f)))
    return pollux_err_args;

  frame_pts_set(ctx, f, r);

  return send_frame(ctx, f);
}

static inline void encoder_priv_set(encode_ctx_s *ctx,
                                    pollux_codec_id_t codec_id,
                                    const void *args) {
  ffmpeg_encode_t *e = &ctx->encode;
  AVCodecContext *cc = e->codec_ctx;
  void *priv = cc->priv_data;

  codec_priv_set(priv, codec_id, args);
}

static int ptr_release(pollux_encode_t *h) {
  if (!h || !h->priv_data)
    return pollux_err_entry;

  encode_ctx_s *ctx = (encode_ctx_s *)h->priv_data;
  encoder_release(ctx);
  return 0;
}

static int ptr_param_set(pollux_encode_t *h, const char *url,
                         const pollux_encode_args_t *args) {
  if (!h || !h->priv_data || !url || !args)
    return pollux_err_entry;

  encode_ctx_s *ctx = (encode_ctx_s *)(h->priv_data);
  return encoder_param_set(ctx, url, args);
}

static int ptr_start(pollux_encode_t *h) {
  if (!h || !h->priv_data)
    return pollux_err_entry;

  encode_ctx_s *ctx = (encode_ctx_s *)h->priv_data;
  return encoder_start(ctx);
}

static int ptr_stop(pollux_encode_t *h) {
  if (!h || !h->priv_data)
    return pollux_err_entry;

  encode_ctx_s *ctx = (encode_ctx_s *)h->priv_data;
  return encoder_stop(ctx);
}

static int ptr_send_frame(pollux_encode_t *h, const pollux_frame_t *r) {
  if (unlikely(!h || !h->priv_data || !r))
    return pollux_err_entry;

  encode_ctx_s *ctx = (encode_ctx_s *)h->priv_data;
  return encoder_send_frame(ctx, r);
}

static inline void ptr_copy(pollux_encode_t *h, encode_ctx_s *ctx) {
  h->priv_data = (void *)ctx;

  h->release = ptr_release;
  h->param_set = ptr_param_set;
  h->start = ptr_start;
  h->stop = ptr_stop;
  h->send_frame = ptr_send_frame;
}

pollux_api int _pollux_encode_priv_set(pollux_encode_t *handle,
                                       pollux_codec_id_t codec_id,
                                       const void *args) {
  if (!handle || !handle->priv_data || !args)
    return pollux_err_entry;

  encode_ctx_s *ctx = (encode_ctx_s *)(handle->priv_data);

  if (ctx->args.codec_id != codec_id) {
    sirius_error(
      "Invalid codec id\n"
      "[Codec id] Exp: %lld; Act: %lld\n",
      (int64_t)ctx->args.codec_id, (int64_t)codec_id);
    return pollux_err_args;
  }

  encoder_priv_set(ctx, codec_id, args);
  return 0;
}

pollux_api void pollux_encode_deinit(pollux_encode_t *handle) {
  if (!handle)
    return;
  if (!handle->priv_data)
    goto label_free;

  encode_ctx_s **ctx = (encode_ctx_s **)(&handle->priv_data);

  encoder_release(*ctx);

  free(*ctx);
  *ctx = nullptr;

label_free:
  free(handle);
}

pollux_api int pollux_encode_init(pollux_encode_t **handle) {
  int ret;

  pollux_encode_t *h = (pollux_encode_t *)calloc(1, sizeof(pollux_encode_t));
  if (!h) {
    sirius_error("calloc -> 'pollux_encode_t'\n");
    return pollux_err_memory_alloc;
  }

  encode_ctx_s *ctx = (encode_ctx_s *)calloc(1, sizeof(encode_ctx_s));
  if (!ctx) {
    sirius_error("calloc -> 'encode_ctx_s'\n");
    ret = pollux_err_memory_alloc;
    goto label_free1;
  }

  ptr_copy(h, ctx);
  *handle = h;
  return 0;

label_free1:
  free(h);

  return ret;
}

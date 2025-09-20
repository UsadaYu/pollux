#include "pollux/pollux_decode.h"

#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <sirius/sirius_cond.h>
#include <sirius/sirius_errno.h>
#include <sirius/sirius_mutex.h>
#include <sirius/sirius_queue.h>
#include <sirius/sirius_thread.h>
#include <sirius/sirius_time.h>

#include "pollux/internal/align.h"
#include "pollux/internal/codec/ffmpeg_decode.h"
#include "pollux/internal/ffmpeg_cvt/codec_id.h"
#include "pollux/internal/ffmpeg_cvt/frame.h"
#include "pollux/internal/ffmpeg_cvt/pixel.h"
#include "pollux/internal/frame.h"
#include "pollux/internal/thread.h"
#include "pollux/internal/util.h"
#include "pollux/pollux_erron.h"
#include "pollux/pollux_frame.h"

#define FRAME_CACHE_MAX (1024)

typedef enum {
  uf_state_null,
  uf_state_end_url,
} user_frame_state_s;

typedef struct {
  user_frame_state_s state;
} frame_priv_s;

typedef struct {
  thread_t thread;

  sirius_cond_handle cond;
  sirius_mutex_handle mtx;
} thread_s;

typedef struct {
  sirius_que_handle que_free, que_rst;
  pollux_frame_t *result[FRAME_CACHE_MAX];

  atomic_bool param_set_flag;
  pollux_decode_args_t args;

  /**
   * @brief Whether to enable image format conversion.
   */
  bool cvt_enable;
  struct SwsContext *sws_ctx;

  ffmpeg_decode_t *decode;

  thread_s thread;
} decode_ctx_s;

static force_inline frame_t *get_pxf_ptr(pollux_frame_t *r) {
  return (frame_t *)r->priv_data;
}

static force_inline frame_priv_s *get_pxf_priv_ptr1(frame_t *pxf) {
  return (frame_priv_s *)pxf->user_priv_data;
}

static force_inline frame_priv_s *get_pxf_priv_ptr2(pollux_frame_t *r) {
  return get_pxf_priv_ptr1(get_pxf_ptr(r));
}

static force_inline int frame_put(sirius_que_handle q, pollux_frame_t *r) {
  int ret = sirius_que_put(q, (size_t)r, sirius_timeout_none);
  if (unlikely(ret)) {
    sirius_error("sirius_que_put: %d, the queue is illegally occupied\n", ret);
    ret = pollux_err_cache_overflow;
  }

  return 0;
}

static inline pollux_frame_t *frame_get(sirius_que_handle q, thread_s *thread) {
  int ret;
  pollux_frame_t *r;
  thread_t *threadt = &thread->thread;

  do {
    ret = sirius_que_get(q, (size_t *)&r, 1);
    if (threadt->exit_flag) {
      sirius_infosp("The decoding thread has exited\n");
      return nullptr;
    }
  } while (ret == sirius_err_timeout);

  if (ret || unlikely(!r)) {
    sirius_error("Fail to get frame\n");
    return nullptr;
  }
  return r;
}

static inline bool stream_seek(decode_ctx_s *ctx) {
  pollux_frame_t *r;
  frame_priv_s *pxf_priv;
  thread_s *thread = &ctx->thread;
  thread_t *threadt = &thread->thread;

  if (!(r = frame_get(ctx->que_free, thread)))
    return false;

  pxf_priv = get_pxf_priv_ptr2(r);
  pxf_priv->state = uf_state_end_url;

  if (frame_put(ctx->que_rst, r))
    return false;

  sirius_mutex_lock(&thread->mtx);
  sirius_cond_wait(&thread->cond, &thread->mtx);
  sirius_mutex_unlock(&thread->mtx);

  return threadt->exit_flag ? false : true;
}

/**
 * @return
 * - (1) 0 indicates Zero or more frames were successfully received.
 *
 * - (2) A negative error code indicates a fatal mistake occurred.
 */
static int receive_and_queue_frames(decode_ctx_s *ctx) {
  int ret;
  ffmpeg_decode_t *d = ctx->decode;
  AVCodecContext *cc = d->codec_ctx;
  thread_s *thread = &ctx->thread;
  thread_t *threadt = &thread->thread;

  while (!threadt->exit_flag) {
    pollux_frame_t *r = frame_get(ctx->que_free, thread);
    if (!r)
      return -1;

    AVFrame *avf = get_pxf_ptr(r)->av_frame;
    AVFrame *rcv_frame = ctx->cvt_enable ? d->frame : avf;

    ret = avcodec_receive_frame(cc, rcv_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      if (frame_put(ctx->que_free, r)) {
        sirius_error("Failed to put unused frame back to free queue\n");
        return -1;
      }
      return 0;
    } else if (ret < 0) {
      ffmpeg_error(ret, "avcodec_receive_frame");
      if (frame_put(ctx->que_free, r)) {
        sirius_error("Failed to put unused frame back to free queue\n");
      }
      return -1;
    }

    if (ctx->cvt_enable) {
      avf->height =
        sws_scale(ctx->sws_ctx, (const uint8_t *const *)rcv_frame->data,
                  rcv_frame->linesize, 0, cc->height, avf->data, avf->linesize);
      av_frame_copy_props(avf, rcv_frame);
    }

    sirius_debgsp("Decode cur pts: %" PRId64 "\n", rcv_frame->pts);
    if (frame_put(ctx->que_rst, r)) {
      sirius_error("Failed to put decoded frame into result queue\n");
      av_frame_unref(rcv_frame);
      return -1;
    }
  }
  return pollux_err_not_init;
}

/**
 * @brief Read a packet from fmt_ctx.
 *
 * @return
 * - (1) 0 on success.
 *
 * - (2) 1 indicates encountering `EOF` but `stream_seek`.
 *
 * - (3) -1 indicates a fatal error has occurred and decoding needs to be
 * exited.
 */
static int read_packet(decode_ctx_s *ctx) {
  int ret;
  ffmpeg_decode_t *d = ctx->decode;

  if ((ret = av_read_frame(d->fmt_ctx, d->pkt)) == 0)
    return 0;

  if (ret == AVERROR_EOF) {
    ret = avcodec_send_packet(d->codec_ctx, nullptr);
    if (ret < 0) {
      ffmpeg_error(ret, "avcodec_send_packet (flushing)");
    } else {
      receive_and_queue_frames(ctx);
    }

    return stream_seek(ctx) ? 1 : -1;
  }

  ffmpeg_error(ret, "av_read_frame");
  return -1;
}

static void thread_decode(void *args) {
  decode_ctx_s *ctx = (decode_ctx_s *)args;
  ffmpeg_decode_t *d = ctx->decode;
  thread_s *thread = &ctx->thread;
  thread_t *threadt = &thread->thread;

  int ret;
  threadt->is_running = true;
  while (!threadt->exit_flag) {
    ret = read_packet(ctx);
    if (unlikely(ret == 1))
      continue;
    if (unlikely(ret == -1))
      break;

    AVPacket *pkt = d->pkt;
    if (pkt->stream_index != d->stream_index) {
      av_packet_unref(pkt);
      continue;
    }

    ret = avcodec_send_packet(d->codec_ctx, pkt);
    av_packet_unref(pkt);
    if (ret < 0) {
      ffmpeg_error(ret, "avcodec_send_packet");
      break;
    }

    if (receive_and_queue_frames(ctx) != 0)
      break;
  }

  threadt->is_running = false;
}

static void decoder_ffmpeg_deinit(decode_ctx_s *ctx) {
  ffmpeg_decode_t *d = ctx->decode;

  ffmpeg_decoder_free_buffers(d);
  ffmpeg_decoder_destroy(&d);
}

static bool decoder_ffmpeg_init(decode_ctx_s *ctx, const char *url,
                                const pollux_decode_args_t *args) {
  ffmpeg_decode_t **d_ptr = &ctx->decode;
  ffmpeg_decode_args_t ffmpeg_args = {0};

  if (!(*d_ptr = ffmpeg_decoder_create(url)))
    return false;

  if (args) {
    ffmpeg_args.thread_count = args->thread_count;
  }

  /**
   * @note `AVMEDIA_TYPE_VIDEO` default, Subsequent improvement.
   */
  if (ffmpeg_decoder_open_stream(*d_ptr, AVMEDIA_TYPE_VIDEO, &ffmpeg_args))
    goto label_free1;
  if (ffmpeg_decoder_alloc_buffers(*d_ptr))
    goto label_free1;

  return true;

label_free1:
  ffmpeg_decoder_destroy(d_ptr);

  return false;
}

static void decoder_sws_deinit(decode_ctx_s *ctx) {
  if (ctx->cvt_enable) {
    if (ctx->sws_ctx) {
      sws_freeContext(ctx->sws_ctx);
      ctx->sws_ctx = nullptr;
    }

    ctx->cvt_enable = false;
  }
}

static bool decoder_sws_init(decode_ctx_s *ctx, const ffmpeg_decode_t *d) {
  AVCodecContext *cc = d->codec_ctx;
  pollux_img_t *img = ctx->args.fmt_cvt_img;
  [[maybe_unused]] const char *str_e =
    "The decoding image configuration may be invalid:";
  [[maybe_unused]] const char *str_d = "Use the default values";

  ctx->cvt_enable = false;
  if (!img)
    return true;

  enum AVPixelFormat fmt;
  if (!cvt_pix_plx_to_ff(img->fmt, &fmt)) {
    sirius_warnsp("%s [Format: %d] %s\n", img->fmt, str_d);
    fmt = cc->pix_fmt;
    cvt_pix_ff_to_plx(fmt, &img->fmt);
  }
  if (img->width <= 0 || img->height <= 0) {
    sirius_warnsp("%s [Width: %d; Height: %d] %s\n", str_e, img->width,
                  img->height, str_d);
    img->width = cc->width;
    img->height = cc->height;
  }

  if (img->align <= 0) {
    sirius_warnsp("%s [Align: %d] %s\n", str_e, img->align, str_d);
    img->align = align_get_alignment();
  }

  sirius_infosp(
    "Decoding target information:\n"
    "\tImage width: %d\n"
    "\tImage height: %d\n"
    "\tImage format: %d\n"
    "\tImage align: %d\n",
    img->width, img->height, img->fmt, img->align);

  if (fmt == cc->pix_fmt && cc->width == img->width &&
      cc->height == img->height && cc->width % img->align == 0) {
    sirius_infosp(
      "The format of the source video image is consistent with the "
      "configuration, so image conversion will not be enabled\n");
    return true;
  }

  struct SwsContext **sc = &ctx->sws_ctx;
  *sc =
    sws_getContext(cc->width, cc->height, cc->pix_fmt, img->width, img->height,
                   fmt, SWS_BILINEAR, nullptr, nullptr, nullptr);
  if (!*sc) {
    sirius_error("sws_getContext\n");
    return false;
  }

  ctx->cvt_enable = true;
  return true;
}

static void decoder_priv_args_free(decode_ctx_s *ctx) {
  pollux_decode_args_t *args = &ctx->args;

  decoder_sws_deinit(ctx);

  if (args->fmt_cvt_img) {
    free(args->fmt_cvt_img);
    args->fmt_cvt_img = nullptr;
  }
}

static bool decoder_priv_args_alloc(decode_ctx_s *ctx,
                                    const pollux_decode_args_t *src) {
  pollux_decode_args_t *dst = &ctx->args;

  dst->fmt_cvt_img = nullptr;

  if (src) {
    memcpy(dst, src, sizeof(pollux_decode_args_t));
    if (src->fmt_cvt_img) {
      dst->fmt_cvt_img = calloc(1, sizeof(pollux_img_t));
      if (!dst->fmt_cvt_img) {
        sirius_error("calloc -> 'pollux_img_t'\n");
        return false;
      }
      memcpy(dst->fmt_cvt_img, src->fmt_cvt_img, sizeof(pollux_img_t));
    }
  }

  if (!decoder_sws_init(ctx, ctx->decode))
    goto label_free1;

  return true;

label_free1:
  if (dst->fmt_cvt_img) {
    free(dst->fmt_cvt_img);
    dst->fmt_cvt_img = nullptr;
  }

  return false;
}

static void decoder_frame_free(decode_ctx_s *ctx) {
  for (int i = 0; i < FRAME_CACHE_MAX; ++i) {
    pollux_frame_t **r = ctx->result + i;

    if (r && *r) {
      frame_t *pxf = (frame_t *)((*r)->priv_data);

      if (pxf) {
        frame_priv_s **pxf_priv = (frame_priv_s **)(&pxf->user_priv_data);

        if (*pxf_priv) {
          free(*pxf_priv);
          *pxf_priv = nullptr;
        }
      }

      pollux_frame_free(r);
    }
  }

#define Q(q) \
  if (q) { \
    if (sirius_que_free(q)) { \
      sirius_error("sirius_que_free\n"); \
    } else { \
      q = nullptr; \
    } \
  }
  Q(ctx->que_rst);
  Q(ctx->que_free);

#undef Q
}

static bool decoder_frame_alloc(decode_ctx_s *ctx, const pollux_img_t *img,
                                int cache_count) {
  int count = sirius_min(cache_count, FRAME_CACHE_MAX);
  count = sirius_max(1, count);

  sirius_que_t c = {.elem_nr = count, .que_type = sirius_que_type_mtx};
#define Q(q) \
  if (sirius_que_alloc(&c, &q)) { \
    sirius_error("sirius_que_alloc\n"); \
    goto label_free; \
  }
  Q(ctx->que_free);
  Q(ctx->que_rst);

  pollux_frame_t *r;
  for (int i = 0; i < count; ++i) {
    if (pollux_frame_alloc(img, &r))
      goto label_free;
    ctx->result[i] = r;

    frame_priv_s *pxf_priv = calloc(1, sizeof(frame_priv_s));
    if (!pxf_priv) {
      sirius_error("calloc -> 'frame_priv_s'\n");
      goto label_free;
    }
    pxf_priv->state = uf_state_null;

    frame_t *pxf = get_pxf_ptr(r);
    pxf->user_priv_data = (void *)pxf_priv;

    if (frame_put(ctx->que_free, r))
      goto label_free;
  }

  return true;

label_free:
  decoder_frame_free(ctx);

  return false;
#undef Q
}

static void decoder_resource_free(decode_ctx_s *ctx) {
  thread_s *thread = &ctx->thread;

  sirius_cond_destroy(&thread->cond);
  sirius_mutex_destroy(&thread->mtx);

  decoder_frame_free(ctx);
}

static bool decoder_resource_alloc(decode_ctx_s *ctx,
                                   const pollux_decode_args_t *args) {
  thread_s *thread = &ctx->thread;
  pollux_img_t *img = ctx->cvt_enable ? args->fmt_cvt_img : nullptr;

  if (!decoder_frame_alloc(ctx, img, args->cache_count))
    return false;
  if (sirius_mutex_init(&thread->mtx, nullptr))
    goto label_free1;
  if (sirius_cond_init(&thread->cond, nullptr))
    goto label_free2;

  return true;

label_free2:
  sirius_mutex_destroy(&thread->mtx);
label_free1:
  decoder_frame_free(ctx);

  return false;
}

static void decoder_thread_stop(decode_ctx_s *ctx) {
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
    sirius_mutex_lock(&thread->mtx);
    sirius_cond_broadcast(&thread->cond);
    sirius_mutex_unlock(&thread->mtx);
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

static bool decoder_thread_start(decode_ctx_s *ctx) {
  thread_s *thread = &ctx->thread;
  thread_t *threadt = &thread->thread;

  threadt->exit_flag = false;
  threadt->is_running = true;
  if (sirius_thread_create(&threadt->thread, nullptr, (void *)thread_decode,
                           (void *)ctx)) {
    threadt->is_running = false;
    return false;
  } else {
    threadt->create_flag = true;
  }

  return true;
}

static inline void decoder_deinit(decode_ctx_s *ctx) {
  decoder_thread_stop(ctx);
  decoder_resource_free(ctx);
  decoder_priv_args_free(ctx);
  decoder_ffmpeg_deinit(ctx);
}

static inline bool decoder_init(decode_ctx_s *ctx, const char *url,
                                const pollux_decode_args_t *args) {
  if (!decoder_ffmpeg_init(ctx, url, args))
    return false;
  if (!decoder_priv_args_alloc(ctx, args))
    goto label_free1;
  if (!decoder_resource_alloc(ctx, &ctx->args))
    goto label_free2;
  if (!decoder_thread_start(ctx))
    goto label_free3;

  return true;

label_free3:
  decoder_resource_free(ctx);
label_free2:
  decoder_priv_args_free(ctx);
label_free1:
  decoder_ffmpeg_deinit(ctx);

  return false;
}

static inline void stream_fill(pollux_decode_stream_info_t *s,
                               const ffmpeg_decode_t *d) {
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

static force_inline void result_ret_debg(int ret) {
  switch (ret) {
  case 0:
    break;
  case pollux_err_stream_end:
    sirius_infosp("Decoded to end of the url\n");
    break;
  case pollux_err_timeout:
    sirius_debgsp("No valid decoding result, timeout for getting the result\n");
    break;
  case pollux_err_resource_alloc:
    sirius_error("Failed to get the result cache\n");
    break;
  case pollux_err_not_init:
    sirius_error(
      "No valid configuration, the decoding thread is not started\n");
    break;
  default:
    sirius_warn("Unknown error\n");
    break;
  }
}

static inline void decoder_release(decode_ctx_s *ctx) {
  if (!ctx->param_set_flag)
    return;

  decoder_deinit(ctx);

  ctx->param_set_flag = false;
}

static inline int decoder_param_set(decode_ctx_s *ctx, const char *url,
                                    const pollux_decode_args_t *args) {
  if (ctx->param_set_flag)
    decoder_deinit(ctx);

  ctx->param_set_flag = false;

  if (!decoder_init(ctx, url, args))
    return pollux_err_resource_alloc;

  ctx->param_set_flag = true;

  return 0;
}

static inline int decoder_result_free(decode_ctx_s *ctx, pollux_frame_t *rst) {
  if (likely(ctx->param_set_flag))
    return frame_put(ctx->que_free, rst);

  sirius_error("The resource is uninitialized\n");
  return pollux_err_not_init;
}

static inline int decoder_result_get(decode_ctx_s *ctx, pollux_frame_t **rst,
                                     uint64_t milliseconds) {
  int ret = 0;

  *rst = nullptr;

  pollux_frame_t *r = nullptr;
  ret = sirius_que_get(ctx->que_rst, (size_t *)&r, milliseconds);
  if (ret || unlikely(!r)) {
    if (likely(ctx->thread.thread.is_running)) {
      ret = likely(ret == sirius_err_timeout) ? pollux_err_timeout
                                              : pollux_err_resource_alloc;
    } else {
      ret = pollux_err_not_init;
    }
    goto label_free;
  }

  frame_t *pxf = get_pxf_ptr(r);
  frame_priv_s *pxf_priv = get_pxf_priv_ptr1(pxf);
  if (unlikely(pxf_priv->state != uf_state_null)) {
    ret = pxf_priv->state == uf_state_end_url ? pollux_err_stream_end : -1;
    pxf_priv->state = uf_state_null;
    frame_put(ctx->que_free, r);
    goto label_free;
  }

  AVFrame *avf = pxf->av_frame;
  if (unlikely(!cvt_frame_ff_to_plx(avf, r))) {
    ret = pollux_err_args;
    goto label_free;
  }
  *rst = r;

label_free:
  result_ret_debg(ret);
  return ret;
}

static inline int decoder_seek_file(decode_ctx_s *ctx, int64_t min_ts,
                                    int64_t ts, int64_t max_ts) {
  int ret = 0;
  thread_s *thread = &ctx->thread;
  thread_t *threadt = &thread->thread;
  ffmpeg_decode_t *d = ctx->decode;

  if (unlikely(!ctx->param_set_flag)) {
    sirius_error("The resource is uninitialized\n");
    return pollux_err_not_init;
  }

  ret = avformat_seek_file(d->fmt_ctx, d->stream_index, min_ts, ts, max_ts,
                           AVSEEK_FLAG_BACKWARD);
  avcodec_flush_buffers(d->codec_ctx);
  if (ret < 0) {
    ffmpeg_error(ret, "avformat_seek_file");
    threadt->exit_flag = true;
    return -1;
  }

  sirius_mutex_lock(&thread->mtx);
  sirius_cond_signal(&thread->cond);
  sirius_mutex_unlock(&thread->mtx);

  return ret;
}

static int ptr_release(pollux_decode_t *h) {
  if (unlikely(!h || !h->priv_data))
    return pollux_err_entry;

  decode_ctx_s *ctx = (decode_ctx_s *)h->priv_data;
  decoder_release(ctx);

  return 0;
}

static int ptr_param_set(pollux_decode_t *h, const char *url,
                         const pollux_decode_args_t *args) {
  if (unlikely(!h || !h->priv_data || !url))
    return pollux_err_entry;

  int ret;
  decode_ctx_s *ctx = (decode_ctx_s *)h->priv_data;
  if ((ret = decoder_param_set(ctx, url, args)) != 0)
    return ret;

  stream_fill(&h->stream, ctx->decode);
  return 0;
}

static int ptr_result_free_ptr(pollux_decode_t *h, pollux_frame_t *rst) {
  if (unlikely(!h || !h->priv_data || !rst))
    return pollux_err_entry;

  decode_ctx_s *ctx = (decode_ctx_s *)h->priv_data;
  return decoder_result_free(ctx, rst);
}

static int ptr_result_get_ptr(pollux_decode_t *h, pollux_frame_t **rst,
                              uint64_t milliseconds) {
  if (unlikely(!h || !h->priv_data || !rst))
    return pollux_err_entry;

  decode_ctx_s *ctx = (decode_ctx_s *)h->priv_data;
  return decoder_result_get(ctx, rst, milliseconds);
}

static int ptr_seek_file_ptr(pollux_decode_t *h, int64_t min_ts, int64_t ts,
                             int64_t max_ts) {
  if (unlikely(!h || !h->priv_data))
    return pollux_err_entry;

  decode_ctx_s *ctx = (decode_ctx_s *)h->priv_data;
  return decoder_seek_file(ctx, min_ts, ts, max_ts);
}

static inline void ptr_copy(pollux_decode_t *h, decode_ctx_s *ctx) {
  h->priv_data = (void *)ctx;

  h->release = ptr_release;
  h->param_set = ptr_param_set;
  h->result_free = ptr_result_free_ptr;
  h->result_get = ptr_result_get_ptr;
  h->seek_file = ptr_seek_file_ptr;
}

pollux_api void pollux_decode_deinit(pollux_decode_t *handle) {
  if (!handle)
    return;
  if (!handle->priv_data)
    goto label_free;

  decode_ctx_s **ctx = (decode_ctx_s **)(&handle->priv_data);

  decoder_release(*ctx);

  free(*ctx);
  *ctx = nullptr;

label_free:
  free(handle);
}

pollux_api int pollux_decode_init(pollux_decode_t **handle) {
  int ret = 0;

  pollux_decode_t *h = (pollux_decode_t *)calloc(1, sizeof(pollux_decode_t));
  if (!h) {
    sirius_error("calloc -> 'pollux_decode_t'\n");
    return pollux_err_memory_alloc;
  }

  decode_ctx_s *ctx = (decode_ctx_s *)calloc(1, sizeof(decode_ctx_s));
  if (!ctx) {
    sirius_error("calloc -> 'decode_ctx_s'\n");
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

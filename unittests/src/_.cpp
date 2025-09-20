/**
 * @note This file is only for debugging.
 */

/**
 * --------------------------------
 * hevc -> libx265
 * --------------------------------
ffmpeg \
-stream_loop 3 \
-i input1_2560-1440_video.mp4 \
-c:v libx265 \
-b:v 6M \
-pix_fmt yuv420p \
-preset ultrafast \
-g 12 \
-r 60 \
-threads 4 \
-an \
-c:a copy \
-y \
ffmpeg_out.mp4

 * --------------------------------
 * hevc -> libsvtav1
 * --------------------------------
ffmpeg \
-stream_loop 0 \
-i input1_2560-1440_video.mp4 \
-c:v libsvtav1 \
-b:v 4M \
-pix_fmt yuv420p \
-preset 13 \
-g 12 \
-r 60 \
-threads 4 \
-an \
-c:a copy \
-y \
ffmpeg_out.mp4
--------------------------------
 */
#include "pollux/codec/codec.h"
#include "pollux/codec/hevc.h"
#include "pollux/pollux_decode.h"
#include "pollux/pollux_encode.h"
#include "pollux/pollux_erron.h"
#include "test.h"

static const char *INPUT_FILE = "input1_2560-1440_video.mp4";
static const char *OUTPUT_FILE = "out.mp4";

static const int FPS = 60;
static const pollux_codec_id_t ENCODE_ID = pollux_codec_id_hevc;
static const pollux_pix_fmt_t EN_FMT = pollux_pix_fmt_yuv420p;
static const int DE_THREAD_COUNT = 4;
static const int EN_THREAD_COUNT = 4;
static const int BIT_RATE = 6 * 1000 * 1000;
static const int T_GROUP = 4;

int main() {
  uint64_t start = sirius_get_time_us();

  pollux_decode_t *d;
  pollux_decode_init(&d);
  pollux_decode_args_t dargs {};
  pollux_img_t img {};
  img.fmt = EN_FMT;
  dargs.fmt_cvt_img = &img;
  dargs.cache_count = 16;
  dargs.thread_count = DE_THREAD_COUNT;
  d->param_set(d, INPUT_FILE, &dargs);
  pollux_decode_stream_info_t s = d->stream;

  pollux_encode_t *e;
  pollux_encode_init(&e);
  pollux_encode_args_t eargs {};
  eargs.bit_rate = BIT_RATE;
  eargs.img.width = s.video_width;
  eargs.img.height = s.video_height;
  eargs.img.fmt = EN_FMT;
  eargs.frame_rate.num = FPS;
  eargs.frame_rate.den = 1;
  eargs.gop_size = s.gop_size;
  eargs.max_b_frames = 3;
  eargs.codec_id = ENCODE_ID;
  eargs.thread_count = EN_THREAD_COUNT;
  e->param_set(e, OUTPUT_FILE, &eargs);

  hevc_encode_args_t priv_args {};
  priv_args.speed_level = codec_range_max;

  // av1_encode_args_t priv_args {};
  // priv_args.speed_level = codec_range_max;

  pollux_encode_priv_set(e, &priv_args);

  e->start(e);
  pollux_frame_t *f;
  for (int i = 0; i < T_GROUP; ++i) {
    while (true) {
      int ret = d->result_get(d, &f, 0);
      if (ret == pollux_err_timeout) {
        continue;
      } else if (unlikely(ret == pollux_err_stream_end)) {
        if (i != T_GROUP - 1)
          d->seek_file(d, 0, 0, 0);
        break;
      } else if (unlikely(ret != pollux_err_ok)) {
        sirius_warnsp("result_get: %d\n", ret);
        break;
      }

      if (unlikely(e->send_frame(e, f)))
        break;
      d->result_free(d, f);
    }
  }
  e->stop(e);

  uint64_t end = sirius_get_time_us();
  sirius_infosp("Gap: %llu s\n",
                (unsigned long long)(end - start) / 1000 / 1000);

  e->release(e);
  pollux_encode_deinit(e);

  d->release(d);
  pollux_decode_deinit(d);

  return 0;
}

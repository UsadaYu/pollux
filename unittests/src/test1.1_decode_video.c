#include "pollux/pollux_decode.h"
#include "pollux/pollux_erron.h"
#include "pollux/pollux_pixel_fmt.h"
#include "test.h"

#define SLEEP_MS (500)

static const char *INPUT_URL = "input1_2560-1440_video.mp4";
static const int LOOP_COUNT = 256;
static pollux_decode_t *DECODER;
static pollux_frame_t *FRAME;
static sirius_mutex_handle MTX;

void result_thread() {
  for (int i = 0; i < LOOP_COUNT; ++i) {
    sirius_mutex_lock(&MTX);
    int ret = DECODER->result_get(DECODER, &FRAME, 1500);
    if (ret) {
      sirius_error("result_get: %d\n", ret);
      break;
    }

    sirius_infosp("Decode image information, index->%d:\n", i);
    sirius_infosp("\tHeight: %d; width: %d\n", FRAME->height, FRAME->width);
    sirius_infosp("\tImage pixel format: %d\n", FRAME->fmt);
    for (int j = 0; j < POLLUX_DECODE_DATA_NR && FRAME->linesize[j]; ++j) {
      sirius_infosp("\tLinesize[%d]: %d\n", j, FRAME->linesize[j]);
    }
    sirius_infosp("\n");

    DECODER->result_free(DECODER, FRAME);
    sirius_mutex_unlock(&MTX);
    sirius_nsleep(80); // Try to give up the mutex lock.
  }
}

int main() {
  test_init();
  sirius_mutex_init(&MTX, nullptr);

  int ret = pollux_decode_init(&DECODER);
  if (ret)
    goto label_free1;

  pollux_img_t img = {0};
  pollux_decode_args_t args = {.cache_count = 16};

  ret = DECODER->param_set(DECODER, INPUT_URL, nullptr);
  if (ret) {
    sirius_error("param_set: %d\n", ret);
    goto label_free2;
  }

  sirius_thread_handle thread;
  ret = sirius_thread_create(&thread, nullptr, (void *)result_thread, nullptr);
  if (ret) {
    sirius_error("sirius_thread_create: %d\n", ret);
    goto label_free3;
  }

#define C(f, h, w, a) \
  do { \
    sirius_usleep(SLEEP_MS * 1000); \
    img.fmt = f; \
    img.height = h; \
    img.width = w; \
    img.align = a; \
    args.fmt_cvt_img = &img; \
    sirius_mutex_lock(&MTX); \
    DECODER->release(DECODER); \
    ret = DECODER->param_set(DECODER, INPUT_URL, &args); \
    sirius_mutex_unlock(&MTX); \
    if (ret) { \
      sirius_error("param_set: %d\n", ret); \
      goto label_free4; \
    } \
  } while (0)

  C(pollux_pix_fmt_yuv420p, 1440, 2560, 64);
  C(pollux_pix_fmt_rgb24, 720, 1280, 32);
  C(pollux_pix_fmt_yuv444p, 1080, 1920, 32);
  C(pollux_pix_fmt_nv12, 1080, 1920, 16);

  sirius_thread_join(thread, nullptr);
  DECODER->release(DECODER);
  pollux_decode_deinit(DECODER);
  return 0;

label_free4:
  sirius_thread_cancel(thread);
  sirius_thread_join(thread, nullptr);
label_free3:
  DECODER->release(DECODER);
label_free2:
  pollux_decode_deinit(DECODER);
label_free1:
  sirius_mutex_destroy(&MTX);
  test_deinit();

  return ret;
}

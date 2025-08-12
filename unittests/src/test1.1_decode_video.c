#include "pollux/pollux_decode.h"
#include "pollux/pollux_erron.h"
#include "pollux/pollux_pixel_fmt.h"
#include "test.h"

#define RCNT (256)
#define SLEEP_MS (500)

const static char *input_file =
    "./input1_2560-1440_video.mp4";
static int count = RCNT;
static pollux_decode_t *pollux_ = nullptr;
static pollux_frame_t *frame = nullptr;

void result_thread() {
  int ret;
  pollux_pix_fmt_t fmt_flag = -1;

  while (count--) {
    ret = pollux_->result_get(pollux_, &frame, 1500);
    if (ret) {
      sirius_error("result_get: %d\n", ret);
      continue;
    }

    if (unlikely(fmt_flag != frame->fmt)) {
      fmt_flag = frame->fmt;
      sirius_warnsp("==============================\n");
    }

    sirius_infosp("Decode image information (idx-%d):\n",
                  RCNT - count);
    sirius_infosp("\tHeight: %d; width: %d\n",
                  frame->height, frame->width);
    for (int i = 0; frame->linesize[i]; i++) {
      sirius_infosp("\tLinesize[%d]: %d\n", i,
                    frame->linesize[i]);
    }
    sirius_infosp("\tImage pixel format: %d\n",
                  frame->fmt);
    sirius_infosp("\n");

    pollux_->result_free(pollux_, frame);
  }
}

int main() {
  test_init();

  int ret = pollux_decode_init(&pollux_);
  if (ret) goto label_free1;

  pollux_img_t img = {0};
  pollux_decode_args_t args = {0};
  args.result_cache_nr = 4;

  ret = pollux_->param_set(pollux_, input_file, nullptr);
  if (ret) {
    sirius_error("param_set: %d\n", ret);
    goto label_free2;
  }

  sirius_thread_handle thread;
  ret = sirius_thread_create(
      &thread, nullptr, (void *)result_thread, nullptr);
  if (ret) {
    sirius_error("sirius_thread_create: %d\n", ret);
    goto label_free3;
  }

#define C(f, h, w, a)                                     \
  do {                                                    \
    sirius_usleep(SLEEP_MS * 1000);                       \
    img.fmt = f;                                          \
    img.height = h;                                       \
    img.width = w;                                        \
    img.align = a;                                        \
    args.fmt_cvt_img = &img;                              \
    ret = pollux_->param_set(pollux_, input_file, &args); \
    if (ret) {                                            \
      sirius_error("param_set: %d\n", ret);               \
      goto label_free4;                                   \
    }                                                     \
  } while (0)

  C(pollux_pix_fmt_yuv420p, 1440, 2560, 64);
  C(pollux_pix_fmt_rgb24, 720, 1280, 32);
  C(pollux_pix_fmt_yuv444p, 1080, 1920, 32);
  C(pollux_pix_fmt_nv12, 1080, 1920, 16);

  sirius_thread_join(thread, nullptr);
  pollux_->release(pollux_);
  pollux_decode_deinit(pollux_);
  return 0;

label_free4:
  sirius_thread_cancel(thread);
  sirius_thread_join(thread, nullptr);

label_free3:
  pollux_->release(pollux_);

label_free2:
  pollux_decode_deinit(pollux_);

label_free1:
  test_deinit();
  return ret;
}

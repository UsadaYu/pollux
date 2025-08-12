#include "pollux/pollux_encode.h"
#include "pollux/pollux_erron.h"
#include "pollux/pollux_swscale.h"
#include "test.h"

static const char *output_file =
    test_generated_pre "2.99_invalid.mp4";
pollux_encode_t *encoder_;

int main() {
  test_init();
  int ret = pollux_encode_init(&encoder_);
  if (ret) goto label_free1;

  pollux_encode_args_t eargs = {0};
  eargs.codec_id = pollux_codec_id_h264;
  eargs.frame_rate = (pollux_rational){1, 1};
  eargs.img =
      (pollux_img_t){10, 10, 1, pollux_pix_fmt_yuv420p};
  ret = encoder_->param_set(encoder_, output_file, &eargs);
  if (ret) goto label_free2;
  ret = encoder_->release(encoder_);
  ret |= encoder_->release(encoder_);
  if (ret) goto label_free2;

  sirius_infosp("---------------------\n");
  sirius_infosp("start\n");
  ret = encoder_->start(encoder_);
  sirius_infosp("ret: %d\n", ret);
  sirius_infosp("---------------------\n\n");
  if (ret == pollux_err_ok) {
    ret = -1;
    goto label_free2;
  }

  sirius_infosp("---------------------\n");
  sirius_infosp("stop\n");
  ret = encoder_->stop(encoder_);
  sirius_infosp("ret: %d\n", ret);
  sirius_infosp("---------------------\n\n");
  if (ret == pollux_err_ok) {
    ret = -1;
    goto label_free2;
  }

  sirius_infosp("---------------------\n");
  sirius_infosp("send_frame\n");
  pollux_frame_t frame1 = {0};
  ret = encoder_->send_frame(encoder_, &frame1);
  sirius_infosp("ret: %d\n", ret);
  sirius_infosp("---------------------\n\n");
  if (ret == pollux_err_ok) {
    ret = -1;
    goto label_free2;
  }

  ret = 0;

label_free2:
  if (ret) {
    sirius_error("Unexpected result\n");
  }
  pollux_encode_deinit(encoder_);

label_free1:
  test_deinit();
  return ret;
}

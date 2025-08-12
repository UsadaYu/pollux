#include "pollux/pollux_decode.h"
#include "pollux/pollux_erron.h"
#include "pollux/pollux_pixel_fmt.h"
#include "test.h"

const static char *input_file =
    "./input1_2560-1440_video.mp4";

static pollux_decode_t *decoder_;

int main() {
  test_init();
  int ret = pollux_decode_init(&decoder_);
  if (ret) goto label_free1;

  pollux_decode_args_t args = {0};
  ret = decoder_->param_set(decoder_, input_file, &args);
  if (ret) goto label_free2;
  ret = decoder_->release(decoder_);
  ret |= decoder_->release(decoder_);
  if (ret) goto label_free2;

  sirius_infosp("---------------------\n");
  sirius_infosp("result_free\n");
  pollux_frame_t frame1 = {0};
  ret = decoder_->result_free(decoder_, &frame1);
  sirius_infosp("ret: %d\n", ret);
  sirius_infosp("---------------------\n\n");
  if (ret == pollux_err_ok) {
    ret = -1;
    goto label_free2;
  }

  sirius_infosp("---------------------\n");
  sirius_infosp("result_get\n");
  pollux_frame_t *frame2;
  ret = decoder_->result_get(decoder_, &frame2, 1000);
  sirius_infosp("ret: %d\n", ret);
  sirius_infosp("---------------------\n\n");
  if (ret == pollux_err_ok) {
    ret = -1;
    goto label_free2;
  }

  sirius_infosp("---------------------\n");
  sirius_infosp("seek_file\n");
  ret = decoder_->seek_file(decoder_, 0, 0, 0);
  sirius_infosp("ret: %d\n", ret);
  sirius_infosp("---------------------\n\n");
  if (ret == pollux_err_ok) {
    ret = -1;
    goto label_free2;
  }

  ret = decoder_->release(decoder_);
  ret |= decoder_->release(decoder_);
  ret |= decoder_->release(decoder_);
  if (ret) goto label_free2;

label_free2:
  if (ret) {
    sirius_error("Unexpected result\n");
  }
  pollux_decode_deinit(decoder_);

label_free1:
  test_deinit();
  return ret;
}

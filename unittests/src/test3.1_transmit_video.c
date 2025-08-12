#include "pollux/pollux_decode.h"
#include "pollux/pollux_encode.h"
#include "pollux/pollux_erron.h"
#include "pollux/pollux_swscale.h"
#include "test.h"

static const char *INPUT_FILE =
    "./input5_1920-1080_video.dav";
static const int WIDTH = 2560;
static const int HEIGHT = 1440;
static const int FPS = 25;

static const int ALIGN = 4;
static const pollux_pix_fmt_t FMT = pollux_pix_fmt_yuv420p;
static const pollux_codec_id_t ENCODE_ID =
    pollux_codec_id_h264;
static const int BIT_RATE = 12 * 1024 * 1024;
static const int GOP_SIZE = 10;
static const int MAX_B_FRAMES = 1;

int main(int argc, char *argv[]) {
  test_init();
  int ret;

  if (argc != 2) {
    sirius_error(
        "The parameter input is incorrect\n"
        "Example of usage:\n"
        "./exe_file tcp://192.168.1.10:1234\n"
        "./exe_file udp://192.168.1.10:12345\n");
    ret = -1;
    goto label_free1;
  }
  const char *url = argv[1];
  sirius_infosp("Output url: %s\n", url);

  pollux_decode_t *d;
  if (pollux_decode_init(&d)) {
    ret = -1;
    goto label_free1;
  }
  pollux_decode_args_t dargs = {0};
  pollux_img_t dimg;
  dargs.result_cache_nr = 32;
  dimg.align = ALIGN;
  dimg.width = WIDTH;
  dimg.height = HEIGHT;
  dimg.fmt = FMT;
  dargs.fmt_cvt_img = &dimg;
  ret = d->param_set(d, INPUT_FILE, &dargs);
  if (ret) {
    goto label_free2;
  }

  pollux_encode_t *e;
  ret = pollux_encode_init(&e);
  if (ret) {
    goto label_free3;
  }
  pollux_encode_args_t eargs = {0};
  eargs.cont_fmt = pollux_cont_fmt_mpegts;
  eargs.bit_rate = BIT_RATE;
  eargs.img.width = WIDTH;
  eargs.img.height = HEIGHT;
  eargs.img.fmt = FMT;
  eargs.frame_rate.num = FPS;
  eargs.frame_rate.den = 1;
  eargs.gop_size = GOP_SIZE;
  eargs.max_b_frames = MAX_B_FRAMES;
  eargs.codec_id = ENCODE_ID;
  eargs.pkg_ahead_nr = 64;
  ret = e->param_set(e, url, &eargs);
  if (ret) {
    goto label_free4;
  }

  e->start(e);
  pollux_frame_t *f;
  while (true) {
    int ret = d->result_get(d, &f, 2000);
    switch (ret) {
      case 0:
        break;
      case pollux_err_url_read_end:
        if (d->seek_file(d, 0, 0, 0)) {
          goto label_free5;
        }
        continue;
      case pollux_err_not_init:
        goto label_free5;
      default:
        sirius_warnsp("result_get: %d\n", ret);
        continue;
    }

    e->send_frame(e, f);
    d->result_free(d, f);
  }

label_free5:
  e->stop(e);
  e->release(e);

label_free4:
  pollux_encode_deinit(e);

label_free3:
  d->release(d);

label_free2:
  pollux_decode_deinit(d);

label_free1:
  test_deinit();
  return ret;
}

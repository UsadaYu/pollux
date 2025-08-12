#include "pollux/pollux_decode.h"
#include "pollux/pollux_encode.h"
#include "pollux/pollux_erron.h"
#include "pollux/pollux_swscale.h"
#include "test.h"

static const char *INPUT_FILE =
    "./input1_2560-1440_video.mp4";
static const int WIDTH = 2560;
static const int HEIGHT = 1440;
static const int FPS = 60;
static const char *OUTPUT_FILE =
    test_generated_pre "2.2_2560-1440_60fps.mp4";

static const int ALIGN = 1;
static const pollux_pix_fmt_t DE_FMT =
    pollux_pix_fmt_rgb24;
static const pollux_pix_fmt_t EN_FMT =
    pollux_pix_fmt_yuv420p;
static const pollux_codec_id_t ENCODE_ID =
    pollux_codec_id_hevc;
static const int BIT_RATE = 12 * 1024 * 1024;
static const int GOP_SIZE = 10;
static const int MAX_B_FRAMES = 1;

#define RECT_NR (25)

typedef struct {
  int left_top_x;
  int left_top_y;
  int right_bottom_x;
  int right_bottom_y;
  int dx;
  int dy;
} move_rect_t;

void move_rectangle(move_rect_t *rect) {
  int img_width = WIDTH - 4;
  int img_height = HEIGHT - 4;

  rect->left_top_x += rect->dx;
  rect->left_top_y += rect->dy;
  rect->right_bottom_x += rect->dx;
  rect->right_bottom_y += rect->dy;

  /* check if it hits the left and right borders */
  if (rect->left_top_x <= 0 ||
      rect->right_bottom_x >= img_width) {
    rect->dx = -(rect->dx);
    if (rect->left_top_x < 0) {
      rect->left_top_x = 0;
      rect->right_bottom_x =
          rect->left_top_x +
          (rect->right_bottom_x - rect->left_top_x);
    } else if (rect->right_bottom_x > img_width) {
      rect->right_bottom_x = img_width;
      rect->left_top_x =
          rect->right_bottom_x -
          (rect->right_bottom_x - rect->left_top_x);
    }
  }

  /* check if it hits the top and bottom borders */
  if (rect->left_top_y <= 0 ||
      rect->right_bottom_y >= img_height) {
    rect->dy = -(rect->dy);
    if (rect->left_top_y < 0) {
      rect->left_top_y = 0;
      rect->right_bottom_y =
          rect->left_top_y +
          (rect->right_bottom_y - rect->left_top_y);
    } else if (rect->right_bottom_y > img_height) {
      rect->right_bottom_y = img_height;
      rect->left_top_y =
          rect->right_bottom_y -
          (rect->right_bottom_y - rect->left_top_y);
    }
  }
}

static void draw_rect_rgb24(unsigned char *rgb_data,
                            int left_top_x, int left_top_y,
                            int right_bottom_x,
                            int right_bottom_y, int r,
                            int g, int b) {
  const int width = WIDTH;
  const int height = HEIGHT;
  const int line_width = 5;  // line-width pixel

/* calculate data offset based on coordinates */
#define PIX_OFFSET(x, y) (((y) * width + (x)) * 3)

  /* top */
  for (int dy = 0; dy < line_width; dy++) {
    int y = left_top_y + dy;
    if (y >= 0 && y < height) {
      for (int x = left_top_x; x <= right_bottom_x; x++) {
        if (x >= 0 && x < width) {
          int offset = PIX_OFFSET(x, y);
          rgb_data[offset] = r;
          rgb_data[offset + 1] = g;
          rgb_data[offset + 2] = b;
        }
      }
    }
  }

  /* bottom */
  for (int dy = 0; dy < line_width; dy++) {
    int y = right_bottom_y - dy;
    if (y >= 0 && y < height) {
      for (int x = left_top_x; x <= right_bottom_x; x++) {
        if (x >= 0 && x < width) {
          int offset = PIX_OFFSET(x, y);
          rgb_data[offset] = r;
          rgb_data[offset + 1] = g;
          rgb_data[offset + 2] = b;
        }
      }
    }
  }

  /* left */
  for (int dx = 0; dx < line_width; dx++) {
    int x = left_top_x + dx;
    if (x >= 0 && x < width) {
      for (int y = left_top_y; y <= right_bottom_y; y++) {
        if (y >= 0 && y < height) {
          int offset = PIX_OFFSET(x, y);
          rgb_data[offset] = r;
          rgb_data[offset + 1] = g;
          rgb_data[offset + 2] = b;
        }
      }
    }
  }

  /* right */
  for (int dx = 0; dx < line_width; dx++) {
    int x = right_bottom_x - dx;
    if (x >= 0 && x < width) {
      for (int y = left_top_y; y <= right_bottom_y; y++) {
        if (y >= 0 && y < height) {
          int offset = PIX_OFFSET(x, y);
          rgb_data[offset] = r;
          rgb_data[offset + 1] = g;
          rgb_data[offset + 2] = b;
        }
      }
    }
  }

#undef PIX_OFFSET
}

int main() {
  test_init();
  srand(time(nullptr));

  pollux_decode_t *d;
  pollux_decode_init(&d);
  pollux_decode_args_t dargs = {0};
  pollux_img_t dimg;
  dargs.result_cache_nr = 8;
  dimg.align = ALIGN;
  dimg.width = WIDTH;
  dimg.height = HEIGHT;
  dimg.fmt = DE_FMT;
  dargs.fmt_cvt_img = &dimg;
  d->param_set(d, INPUT_FILE, &dargs);

  pollux_encode_t *e;
  pollux_encode_init(&e);
  pollux_encode_args_t eargs = {0};
  eargs.bit_rate = BIT_RATE;
  eargs.img.width = WIDTH;
  eargs.img.height = HEIGHT;
  eargs.img.fmt = EN_FMT;
  eargs.frame_rate.num = FPS;
  eargs.frame_rate.den = 1;
  eargs.gop_size = GOP_SIZE;
  eargs.max_b_frames = MAX_B_FRAMES;
  eargs.codec_id = ENCODE_ID;
  eargs.pkg_ahead_nr = 0;
  e->param_set(e, OUTPUT_FILE, &eargs);

  pollux_img_t sws_img = {0};
  pollux_frame_t *sws_frame;
  sws_img.align = ALIGN;
  sws_img.width = eargs.img.width;
  sws_img.height = eargs.img.height;
  sws_img.fmt = eargs.img.fmt;
  pollux_frame_alloc(&sws_img, &sws_frame);

  pollux_sws_handle sws_handle = pollux_sws_context_alloc(
      dimg.width, dimg.height, dimg.fmt, sws_img.width,
      sws_img.height, sws_img.fmt);

  int max_wd = dimg.width / 5;
  int max_hgt = dimg.height / 5;
  move_rect_t rect[RECT_NR] = {0};
  for (int i = 0; i < RECT_NR; i++) {
    rect[i].left_top_x =
        rand() % (dimg.width - max_wd) + 1;
    rect[i].left_top_y =
        rand() % (dimg.height - max_hgt) + 1;
    int wd = rand() % (max_wd - 50 + 1) + 50;
    int hgt = rand() % (max_hgt - 50 + 1) + 50;
    rect[i].right_bottom_x = rect[i].left_top_x + wd;
    rect[i].right_bottom_y = rect[i].left_top_y + hgt;
    rect[i].dx = rand() % (12 - 2 + 1) + 2;
    rect[i].dy = rand() % (10 - 2 + 1) + 2;
  }

  e->start(e);
  pollux_frame_t *f;
  while (true) {
    int ret = d->result_get(d, &f, 2000);
    if (ret) {
      if (ret == pollux_err_url_read_end ||
          ret == pollux_err_not_init) {
        break;
      }
      sirius_warnsp("warning: result_get->%d\n", ret);
      continue;
    }

    for (int i = 0; i < RECT_NR; i++) {
      move_rect_t *rt = rect + i;
      move_rectangle(rt);
      int r, g, b;
      switch (i % 4) {
        case 0:
          r = 250, g = 0, b = 0;
          break;
        case 1:
          r = 236, g = 135, b = 243;
          break;
        default:
          r = 241, g = 231, b = 206;
          break;
      }
      draw_rect_rgb24((unsigned char *)(f->data[0]),
                      rt->left_top_x, rt->left_top_y,
                      rt->right_bottom_x,
                      rt->right_bottom_y, r, g, b);
    }

    pollux_sws_scale(sws_handle, f, sws_frame);
    d->result_free(d, f);

    e->send_frame(e, sws_frame);
  }
  e->stop(e);

  pollux_sws_context_free(&sws_handle);
  pollux_frame_free(&sws_frame);

  e->release(e);
  pollux_encode_deinit(e);

  d->release(d);
  pollux_decode_deinit(d);

  test_deinit();
  return 0;
}

#include "pollux/pollux_decode.h"
#include "pollux/pollux_erron.h"
#include "pollux/pollux_pixel_fmt.h"
#include "test.h"

static constexpr auto INPUT_FILE_1 = "./input1_2560-1440_video.mp4";
static constexpr auto INPUT_FILE_2 = "./input2_3506-2200_video.avi";

static constexpr char name_[] = "1.2_";
static const auto OUT_DIR = testcxx::out_dir<name_>::make();

static constexpr size_t IMG_GROUP = 128;
static int T_GROUP = 1;

class PolluxDecoder {
 public:
  PolluxDecoder() {
    if (pollux_decode_init(&pollux_)) {
      throw std::runtime_error("pollux_decode_init");
    }
  }

  ~PolluxDecoder() {
    if (pollux_) {
      pollux_->release(pollux_);
      pollux_decode_deinit(pollux_);
    }
  }

 private:
  std::string fmt_to_file_suffix(pollux_pix_fmt_t fmt) const {
    static const std::map<pollux_pix_fmt_t, std::string> suffix = {
      {pollux_pix_fmt_yuv444p, ".yuv444p"},
      {pollux_pix_fmt_nv21,    ".nv21"   },
      {pollux_pix_fmt_nv12,    ".nv12"   },
      {pollux_pix_fmt_rgb24,   ".rgb24"  },
      {pollux_pix_fmt_bgr24,   ".bgr24"  },
      {pollux_pix_fmt_yuv420p, ".yuv420p"}
    };
    return suffix.count(fmt) ? suffix.at(fmt) : "";
  }

  bool write_frame_to_file(const pollux_pix_fmt_t fmt, pollux_frame_t *rst,
                           const std::string &file_name) const {
    std::ofstream f(file_name, std::ios::binary);
    if (!f.is_open()) {
      sirius_warnsp("Failed to open file: %s\n", file_name.c_str());
      return false;
    }

#define W(idx, s_split) \
  f.write(reinterpret_cast<char *>(rst->data[idx]), \
          rst->linesize[idx] * rst->height >> s_split);

    switch (fmt) {
    case pollux_pix_fmt_yuv420p:
      W(0, 0) W(1, 1) W(2, 1) break;
    case pollux_pix_fmt_yuv444p:
      W(0, 0) W(1, 0) W(2, 0) break;
    case pollux_pix_fmt_nv21:
    case pollux_pix_fmt_nv12:
      W(0, 0) W(1, 1) break;
    case pollux_pix_fmt_rgb24:
    case pollux_pix_fmt_bgr24:
      W(0, 0) break;
    default:
      sirius_error("Invalid pixel format: %d\n", fmt);
      return false;
    }

    return true;
#undef W
  }

  int process_frames(const pollux_pix_fmt_t fmt) {
    int ret = 0;
    int n = T_GROUP++;
    pollux_frame_t *rst = nullptr;

    for (unsigned int i = 0; i < IMG_GROUP; i++) {
      ret = pollux_->result_get(pollux_, &rst, 1500);
      switch (ret) {
      case 0:
        break;
      case pollux_err_stream_end:
        if (pollux_->seek_file(pollux_, 0, 0, 0))
          return -1;
        continue;
      case pollux_err_not_init:
        sirius_error("result_get: %d\n", ret);
        return -1;
      default:
        sirius_warnsp("result_get: %d\n", ret);
        continue;
      }

      std::string file_name = OUT_DIR + "/" + std::to_string(n) + "_" +
        std::to_string(rst->linesize[0]) + "-" + std::to_string(rst->height) +
        "_" + std::to_string(i) + fmt_to_file_suffix(fmt);

      if (!write_frame_to_file(fmt, rst, file_name)) {
        pollux_->result_free(pollux_, rst);
        return -1;
      }

      pollux_->result_free(pollux_, rst);
    }
    return 0;
  }

 public:
  int set_params_and_decode(pollux_img_t *img, const std::string &file) {
    pollux_decode_args_t args = {16, 0, img};

    int ret = pollux_->param_set(pollux_, file.c_str(), &args);
    if (ret) {
      sirius_error("param_set: %d\n", ret);
      return ret;
    }

    pollux_pix_fmt_t fmt = img ? img->fmt : pollux_->stream.video_img_fmt;
    return process_frames(fmt);
  }

 private:
  pollux_decode_t *pollux_ = nullptr;
};

int main() {
  testcxx::Init test_env;
  test_env.mkdir(OUT_DIR);

  try {
    PolluxDecoder decoder;
    pollux_img_t img;

    img = {0, 0, 32, pollux_pix_fmt_nv21};
    if (decoder.set_params_and_decode(&img, INPUT_FILE_1))
      return -1;

    img = {0, 0, 16, pollux_pix_fmt_yuv444p};
    if (decoder.set_params_and_decode(&img, INPUT_FILE_2))
      return -1;

  } catch (const std::exception &e) {
    sirius_error("Exception: %s\n", e.what());
    return -1;
  }

  return 0;
}

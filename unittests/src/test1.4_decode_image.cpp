#include "pollux/pollux_decode.h"
#include "pollux/pollux_erron.h"
#include "pollux/pollux_pixel_fmt.h"
#include "test.h"

constexpr std::string_view INPUT_FILE_ARR[] = {
    "./input100_@rurudo_5764-4000.jpg",
    "./input101_@fusuma_3838-2159.png",
};
const auto OUT_PREFIX =
    std::string(test_generated_pre) + "1.4_";

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

  void param_set(const std::string_view& url,
                 const pollux_decode_args_t* args) {
    auto ret =
        pollux_->param_set(pollux_, url.data(), args);
    if (ret) {
      throw std::runtime_error("param_set");
    }
  }

  void result_get(pollux_frame_t** f) {
    auto ret = pollux_->result_get(pollux_, f, 2000);
    if (ret || !*f) {
      throw std::runtime_error("result_get");
    }
  }

  void result_free(pollux_frame_t* f) {
    auto ret = pollux_->result_free(pollux_, f);
    if (ret) {
      throw std::runtime_error("result_free");
    }
  }

  void seek_url() {
    auto ret = pollux_->seek_file(pollux_, 0, 0, 0);
    if (ret) {
      throw std::runtime_error("seek_file");
    }
  }

 private:
  pollux_decode_t* pollux_ = nullptr;
};

int main() {
  testcxx::Init test_env;
  pollux_decode_args_t args{};
  pollux_img_t img{};
  img.fmt = pollux_pix_fmt_t::pollux_pix_fmt_rgb24;
  args.fmt_cvt_img = &img;

  try {
    PolluxDecoder decoder;
    pollux_frame_t* rst;

    for (size_t i = 0; const auto& file : INPUT_FILE_ARR) {
      decoder.param_set(file, &args);
      decoder.result_get(&rst);

      std::string file_name =
          OUT_PREFIX + std::to_string(rst->width) + "-" +
          std::to_string(rst->height) + "_" +
          std::to_string(i++) + ".rgb24";
      std::ofstream f(file_name, std::ios::binary);
      if (!f.is_open())
        throw std::runtime_error("Failed to open file: " +
                                 file_name);

      f.write(reinterpret_cast<char*>(rst->data[0]),
              rst->linesize[0] * rst->height);
      decoder.result_free(rst);
    }
  } catch (const std::exception& e) {
    sirius_error("Exception: %s\n", e.what());
    return -1;
  }
  return 0;
}

#include "pollux/pollux_decode.h"
#include "pollux/pollux_encode.h"
#include "pollux/pollux_erron.h"
#include "test.h"

/**
 * @note Considering that some image viewing software does not support
 * high-resolution GIFs well, do not set the length and width too large here.
 */
constexpr int WIDTH = 1763, HEIGHT = 1100;
constexpr auto INPUT_URL = "./input2_3506-2200_video.avi";
const auto OUTPUT_URL = std::string(test_generated_pre) + "2.4_" +
  std::to_string(WIDTH) + "-" + std::to_string(HEIGHT) + ".gif";

constexpr auto CODEC_ID = pollux_codec_id_t::pollux_codec_id_gif;
constexpr auto FMT = pollux_pix_fmt_t::pollux_pix_fmt_rgb8;
constexpr int FRAME_COUNT = 60;

static int ORD_FPS;

class PolluxDecoder {
 public:
  PolluxDecoder(const std::string &input_url) {
    if (pollux_decode_init(&decoder_)) {
      throw std::runtime_error("pollux_decode_init");
    }

    pollux_decode_args_t args {};
    pollux_img_t img {};
    args.cache_count = 4;
    img.width = WIDTH;
    img.height = HEIGHT;
    img.fmt = FMT;
    img.align = 1;
    args.fmt_cvt_img = &img;
    if (decoder_->param_set(decoder_, input_url.c_str(), &args)) {
      pollux_decode_deinit(decoder_);
      throw std::runtime_error("decoder_->param_set");
    }

    auto s = decoder_->stream;
    ORD_FPS = (int)(s.video_frame_rate.num / s.video_frame_rate.den);
    if (ORD_FPS <= 0) {
      ORD_FPS = 30;
      sirius_warn("Invalid argument: fps\n");
      sirius_warn("Use the default value: %d\n", ORD_FPS);
    }
  }

  ~PolluxDecoder() {
    if (decoder_) {
      decoder_->release(decoder_);
      pollux_decode_deinit(decoder_);
    }
  }

  inline pollux_frame_t *get_frame() {
    pollux_frame_t *f = nullptr;
    int ret = decoder_->result_get(decoder_, &f, 2000);
    if (ret == pollux_err_t::pollux_err_timeout) {
      return get_frame();
    } else if (ret == pollux_err_t::pollux_err_stream_end) {
      std::cout << "End of url" << std::endl;
      return nullptr;
    } else if (ret != pollux_err_t::pollux_err_ok) {
      throw std::runtime_error("decoder_->result_get");
    }
    return f;
  }

  inline void release_frame(pollux_frame_t *f) {
    decoder_->result_free(decoder_, f);
  }

 private:
  pollux_decode_t *decoder_ = nullptr;
};

class PolluxEncoder {
 public:
  PolluxEncoder(const std::string &output_file) {
    if (pollux_encode_init(&encoder_)) {
      throw std::runtime_error("pollux_encode_init");
    }

    pollux_encode_args_t args {};
    args.cont_fmt = pollux_cont_fmt_t::pollux_cont_fmt_none;
    args.bit_rate = 2 * 1024 * 1024;
    args.img.width = WIDTH;
    args.img.height = HEIGHT;
    args.img.fmt = FMT;
    args.frame_rate.num = ORD_FPS;
    args.frame_rate.den = 1;
    args.codec_id = CODEC_ID;
    args.thread_count = 4;

    if (encoder_->param_set(encoder_, output_file.c_str(), &args)) {
      pollux_encode_deinit(encoder_);
      throw std::runtime_error("encoder_->param_set");
    }

    if (encoder_->start(encoder_)) {
      pollux_encode_deinit(encoder_);
      throw std::runtime_error("encoder_->start");
    }
  }

  ~PolluxEncoder() {
    if (encoder_) {
      encoder_->stop(encoder_);
      encoder_->release(encoder_);
      pollux_encode_deinit(encoder_);
    }
  }

  inline void send_frame(pollux_frame_t *f) {
    encoder_->send_frame(encoder_, f);
  }

 private:
  pollux_encode_t *encoder_ = nullptr;
};

int main() {
  testcxx::Init();

  try {
    PolluxDecoder decoder(INPUT_URL);
    PolluxEncoder encoder(OUTPUT_URL);

    for (int i = 0; i < FRAME_COUNT; i++) {
      pollux_frame_t *f = decoder.get_frame();

      if (!f)
        break;

      encoder.send_frame(f);
      decoder.release_frame(f);
    }
  } catch (const std::exception &e) {
    sirius_error("Exception: %s\n", e.what());
    return -1;
  }

  return 0;
}

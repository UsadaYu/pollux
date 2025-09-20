#include "pollux/pollux_decode.h"
#include "pollux/pollux_encode.h"
#include "pollux/pollux_erron.h"
#include "test.h"

#define PNG 0

constexpr auto INPUT_FILE = "./input1_2560-1440_video.mp4";
constexpr char name_[] = "2.3_";
const auto OUT_DIR = testcxx::out_dir<name_>::make();

#if PNG
constexpr auto CONT_FMT = pollux_cont_fmt_t::pollux_cont_fmt_image2pipe;
constexpr auto CODEC_ID = pollux_codec_id_t::pollux_codec_id_png;
constexpr auto OUT_SUFFIX = ".png";
constexpr auto FMT = pollux_pix_fmt_t::pollux_pix_fmt_rgb24;
#else
constexpr auto CONT_FMT = pollux_cont_fmt_t::pollux_cont_fmt_image2pipe;
constexpr auto CODEC_ID = pollux_codec_id_t::pollux_codec_id_mjpeg;
constexpr auto OUT_SUFFIX = ".jpg";
constexpr auto FMT = pollux_pix_fmt_t::pollux_pix_fmt_yuvj420p;
#endif

static int ORD_WIDTH, ORD_HEIGHT;

class PolluxDecoder {
 public:
  PolluxDecoder(const std::string &input_file) {
    if (pollux_decode_init(&decoder_)) {
      throw std::runtime_error("pollux_decode_init");
    }

    pollux_decode_args_t args {};
    pollux_img_t img {};
    args.cache_count = 4;
    img.fmt = FMT;
    args.fmt_cvt_img = &img;
    if (decoder_->param_set(decoder_, input_file.c_str(), &args)) {
      pollux_decode_deinit(decoder_);
      throw std::runtime_error("decoder_->param_set");
    }

    ORD_WIDTH = decoder_->stream.video_width;
    ORD_HEIGHT = decoder_->stream.video_height;
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
    if (ret) {
      if (ret == pollux_err_stream_end) {
        std::cout << "End of url" << std::endl;
        return nullptr;
      } else if (ret == pollux_err_not_init) {
        throw std::runtime_error("decoder_->result_get");
      }
      return get_frame();
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
  PolluxEncoder(pollux_encode_t *encoder) : encoder_(encoder) {
    if (encoder_->start(encoder_)) {
      throw std::runtime_error("encoder_->start");
    }
  }

  ~PolluxEncoder() {
    encoder_->stop(encoder_);
  }

  pollux_err_t send_frame(pollux_frame_t *f) {
    return (pollux_err_t)(encoder_->send_frame(encoder_, f));
  }

 private:
  pollux_encode_t *encoder_;
};

class ImageEncoder {
 public:
  ImageEncoder(const std::string &out_dir) : out_dir(out_dir) {
    if (pollux_encode_init(&encoder_)) {
      throw std::runtime_error("pollux_encode_init");
    }

    args_.img.width = ORD_WIDTH;
    args_.img.height = ORD_HEIGHT;
    args_.img.fmt = FMT;
    args_.frame_rate.num = 1;
    args_.frame_rate.den = 1;
    args_.bit_rate = 0;
    args_.gop_size = 1;
    args_.max_b_frames = 0;
    args_.thread_count = 1;

    frame_idx_ = 0;
  }

  ~ImageEncoder() {
    encoder_->release(encoder_);
    pollux_encode_deinit(encoder_);
  }

  void img_encode(pollux_frame_t *f) {
    std::string file_name =
      out_dir + "/" + std::to_string(++frame_idx_) + OUT_SUFFIX;
    args_.cont_fmt = CONT_FMT;
    args_.codec_id = CODEC_ID;
    if (encoder_->param_set(encoder_, file_name.c_str(), &args_)) {
      throw std::runtime_error("Fail to set configuration");
    }
    PolluxEncoder ec(encoder_);

    if (ec.send_frame(f)) {
      throw std::runtime_error("Fail to send the frame");
    }
  }

 private:
  std::string out_dir;
  pollux_encode_t *encoder_ = nullptr;
  pollux_encode_args_t args_;
  unsigned int frame_idx_;
};

int main() {
  testcxx::Init test_env;
  test_env.mkdir(OUT_DIR);

  try {
    PolluxDecoder decoder(INPUT_FILE);
    ImageEncoder img_encoder(OUT_DIR);

    while (true) {
      pollux_frame_t *f = decoder.get_frame();
      if (!f)
        break;

      img_encoder.img_encode(f);
      decoder.release_frame(f);
    }
  } catch (const std::exception &e) {
    sirius_error("Exception: %s\n", e.what());
    return -1;
  }

  return 0;
}

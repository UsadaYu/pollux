#include "pollux/pollux_decode.h"
#include "pollux/pollux_encode.h"
#include "pollux/pollux_erron.h"
#include "test.h"

static constexpr auto INPUT_FILE =
    "./input1_2560-1440_video.mp4";

static constexpr auto ENCODE_ID =
    pollux_codec_id_t::pollux_codec_id_h264;

constexpr int PKG_AHEAD_NR_ARR[] = {
    12,
    256,
    0,
};

static std::string output_file;
static int ord_fps;
static int ord_width, ord_height;
static int ord_bit_rate;
static int ord_max_b_frames;
static int ord_gop_size;
static pollux_pix_fmt_t ord_fmt;
static double ord_duration;  // Unit: s

class PolluxDecoder {
 public:
  PolluxDecoder(const std::string &input_file) {
    if (pollux_decode_init(&decoder_)) {
      throw std::runtime_error("pollux_decode_init");
    }

    pollux_decode_args_t args{};
    args.result_cache_nr = 16;
    args.fmt_cvt_img = nullptr;
    if (decoder_->param_set(decoder_, input_file.c_str(),
                            &args)) {
      pollux_decode_deinit(decoder_);
      throw std::runtime_error("decoder_->param_set");
    }

    stream_info_print(decoder_->stream);
    stream_info_cpy(decoder_->stream);
  }

  ~PolluxDecoder() {
    if (decoder_) {
      decoder_->release(decoder_);
      pollux_decode_deinit(decoder_);
    }
  }

  pollux_frame_t *get_frame() {
    pollux_frame_t *f = nullptr;
    int ret = decoder_->result_get(decoder_, &f, 2000);
    if (ret) {
      if (ret == pollux_err_url_read_end) {
        decoder_->seek_file(decoder_, 0, 0, 0);
        return nullptr;
      } else if (ret == pollux_err_not_init) {
        return nullptr;
      }
      return get_frame();
    }
    return f;
  }

  void release_frame(pollux_frame_t *f) {
    decoder_->result_free(decoder_, f);
  }

  std::string get_out_name(size_t index) {
    auto prefix = std::string(test_generated_pre) +
                  "2.1_" + std::to_string(index) + "_" +
                  std::to_string(ord_width) + "-" +
                  std::to_string(ord_height) + "_" +
                  std::to_string(ord_fps) + "fps.mp4";
    return prefix;
  }

 private:
  void stream_info_print(
      const pollux_decode_stream_info_t &s) {
    sirius_infosp("Source video information:\n");
    sirius_infosp("\tWidth: %d; Height: %d\n",
                  s.video_width, s.video_height);
    sirius_infosp("\tImage format: %d\n", s.video_img_fmt);
    sirius_infosp("\tVideo codec_id: %d\n",
                  s.video_codec_id);
    sirius_infosp("\tVideo duration: %lfs\n",
                  (double)(s.duration) / 1000 / 1000);
    sirius_infosp("\tVideo frame per second: %d\n",
                  (int)(s.video_frame_rate.num /
                        s.video_frame_rate.den));
    sirius_infosp("\tVideo bitrate: %d\n", s.bit_rate);
    sirius_infosp("\tVideo max_b_frames: %d\n",
                  s.max_b_frames);
    sirius_infosp("\tVideo gop_size: %d\n", s.gop_size);
  }

  void stream_info_cpy(
      const pollux_decode_stream_info_t &s) {
    ord_width = s.video_width;
    ord_height = s.video_height;
    ord_fmt = s.video_img_fmt;
    ord_fps = (int)(s.video_frame_rate.num /
                    s.video_frame_rate.den);
    if (ord_fps <= 0) {
      ord_fps = 60;
      sirius_warn("Invalid argument: fps\n");
      sirius_warn("Use the default value: %d\n", ord_fps);
    }

    ord_duration = (double)(s.duration) / 1000 / 1000;
    ord_bit_rate = s.bit_rate;
    ord_max_b_frames = s.max_b_frames;
    ord_gop_size = s.gop_size;
  }

  pollux_decode_t *decoder_ = nullptr;
};

class PolluxEncoder {
 public:
  PolluxEncoder(const std::string &output_file,
                int pkg_ahead_nr) {
    if (pollux_encode_init(&encoder_)) {
      throw std::runtime_error("pollux_encode_init");
    }

    pollux_encode_args_t args = {};
    args.cont_fmt =
        pollux_cont_fmt_t::pollux_cont_fmt_none;
    args.bit_rate = ord_bit_rate;
    args.img.width = ord_width;
    args.img.height = ord_height;
    args.img.fmt = ord_fmt;
    args.frame_rate.num = ord_fps;
    args.frame_rate.den = 1;
    args.gop_size = ord_gop_size;
    args.max_b_frames = ord_max_b_frames;
    args.codec_id = ENCODE_ID;
    args.pkg_ahead_nr = pkg_ahead_nr;

    if (encoder_->param_set(encoder_, output_file.c_str(),
                            &args)) {
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
  double start_s, end_s;

  try {
    PolluxDecoder decoder(INPUT_FILE);

    for (size_t i = 0;
         const auto &ahead : PKG_AHEAD_NR_ARR) {
      PolluxEncoder encoder(decoder.get_out_name(++i),
                            ahead);

      start_s =
          (double)(sirius_get_time_us()) / 1000 / 1000;
      while (true) {
        pollux_frame_t *f = decoder.get_frame();
        if (!f) break;

        encoder.send_frame(f);
        decoder.release_frame(f);
      }
      end_s = (double)(sirius_get_time_us()) / 1000 / 1000;
      sirius_info(
          "\n---------------------------\n"
          "Index: %d\n"
          "Pkg ahead number: %d\n"
          "Encoding duration: %lf s\n"
          "Target video duration: %lf s\n"
          "---------------------------\n",
          i, ahead, end_s - start_s, ord_duration);
    }
  } catch (const std::exception &e) {
    sirius_error("exception: %s\n", e.what());
    return -1;
  }

  return 0;
}

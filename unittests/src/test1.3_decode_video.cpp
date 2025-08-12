#include "pollux/pollux_decode.h"
#include "pollux/pollux_erron.h"
#include "pollux/pollux_pixel_fmt.h"
#include "test.h"

constexpr int IMG_GRP = 404;
constexpr int SG = 5;
const std::string video_1 = "./input1_2560-1440_video.mp4";

constexpr char name_[] = "1.3_";
const auto OUT_DIR = testcxx::out_dir<name_>::make();

struct PolluxFrameDeleter {
  pollux_decode_t* decoder_handle = nullptr;
  void operator()(pollux_frame_t* frame) const {
    if (decoder_handle && frame) {
      decoder_handle->result_free(decoder_handle, frame);
    }
  }
};

using PolluxFramePtr =
    std::unique_ptr<pollux_frame_t, PolluxFrameDeleter>;

class PolluxDecoder {
 public:
  PolluxDecoder() {
    if (pollux_decode_init(&pollux_)) {
      throw std::runtime_error(
          "pollux_decode_init failed");
    }
  }

  ~PolluxDecoder() {
    if (pollux_) {
      pollux_->release(pollux_);
      pollux_decode_deinit(pollux_);
    }
  }

  PolluxDecoder(const PolluxDecoder&) = delete;
  PolluxDecoder& operator=(const PolluxDecoder&) = delete;

  PolluxDecoder(PolluxDecoder&& other) noexcept
      : pollux_(other.pollux_) {
    other.pollux_ = nullptr;
  }
  PolluxDecoder& operator=(
      PolluxDecoder&& other) noexcept {
    if (this != &other) {
      if (pollux_) {
        pollux_->release(pollux_);
        pollux_decode_deinit(pollux_);
      }
      pollux_ = other.pollux_;
      other.pollux_ = nullptr;
    }
    return *this;
  }

  void param_set(const std::string& url,
                 pollux_decode_args_t* args) {
    int ret =
        pollux_->param_set(pollux_, url.c_str(), args);
    if (ret) {
      throw std::runtime_error(std::format(
          "param_set failed with code: {}", ret));
    }
  }

  PolluxFramePtr result_get(int timeout_ms, int& ret) {
    pollux_frame_t* frame = nullptr;
    ret = pollux_->result_get(pollux_, &frame, timeout_ms);
    if (ret == 0) {
      return PolluxFramePtr(frame, {pollux_});
    }
    return nullptr;
  }

  void seek_file(auto timestamp, auto stream_index,
                 auto flags) {
    if (pollux_->seek_file(pollux_, timestamp,
                           stream_index, flags)) {
      throw std::runtime_error("seek_file failed");
    }
  }

 private:
  pollux_decode_t* pollux_ = nullptr;
};

void file_write(PolluxDecoder& decoder) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> rand_count_dist(1,
                                                  SG - 1);
  std::uniform_int_distribution<> action_dist(0, SG - 2);

  for (int i = 0; i < IMG_GRP; ++i) {
    std::vector<PolluxFramePtr> frames;
    int rands = rand_count_dist(gen);
    frames.reserve(rands);

    for (int j = 0; j < rands; ++j) {
      int ret = 0;
      PolluxFramePtr frame = decoder.result_get(2000, ret);

      switch (ret) {
        case 0:
          frames.push_back(std::move(frame));
          break;
        case pollux_err_url_read_end:
          frames.clear();
          decoder.seek_file(0, 0, 0);
          goto label_free;
        case pollux_err_not_init:
          throw std::runtime_error(
              "Decoder not initialized");
        default:
          sirius_error("result_get: %d\n", ret);
          continue;
      }
    }

    for (size_t j = 0; j < frames.size(); ++j) {
      if (!frames[j]) continue;

      switch (action_dist(gen)) {
        case 0:
          frames[j].reset();
          break;
        case 1:
          std::this_thread::sleep_for(
              std::chrono::milliseconds(1));
          break;
        default:
          auto& rst = frames[j];
          std::string file_name = std::format(
              "{0}/{1}-{2}_{3}_{4}.nv12", OUT_DIR,
              rst->linesize[0], rst->height, i, j);

          std::ofstream ofs(file_name, std::ios::binary);
          if (!ofs) {
            throw std::runtime_error(
                "Failed to open file `" + file_name + "`");
          }

          ofs.write(
              reinterpret_cast<const char*>(rst->data[0]),
              static_cast<std::streamsize>(
                  rst->linesize[0]) *
                  rst->height);
          ofs.write(
              reinterpret_cast<const char*>(rst->data[1]),
              (static_cast<std::streamsize>(
                   rst->linesize[1]) *
               rst->height) >>
                  1);
          break;
      }
    }

  label_free:;
  }
}

int main(int argc, char* argv[]) {
  testcxx::Init test_env;
  test_env.mkdir(OUT_DIR);

  try {
    PolluxDecoder decoder;

    pollux_img_t img = {1280, 720, 16,
                        pollux_pix_fmt_nv12};
    pollux_decode_args_t args{};
    args.result_cache_nr = 8;
    args.fmt_cvt_img = &img;

    decoder.param_set(video_1, &args);

    file_write(decoder);

  } catch (const std::exception& e) {
    sirius_error("Exception: %s\n", e.what());
    return -1;
  }

  return 0;
}

#ifndef POLLUX_TEST_H
#define POLLUX_TEST_H

#ifndef test_generated_pre
#  define test_generated_pre "test"
#endif

#ifdef __cplusplus
#  include <chrono>
#  include <cmath>
#  include <cstdlib>
#  include <filesystem>
#  include <format>
#  include <fstream>
#  include <iomanip>
#  include <iostream>
#  include <map>
#  include <memory>
#  include <random>
#  include <sstream>
#  include <stdexcept>
#  include <string>
#  include <thread>
#  include <vector>
#else
#  ifdef __STDC_VERSION__
#    if (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#      include <stdatomic.h>
#    endif

#    if (__STDC_VERSION__ >= 202311L)
#    else
#      include <stdbool.h>
#      define nullptr NULL
#    endif
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <fcntl.h>
#include <sirius/sirius_log.h>
#include <sirius/sirius_mutex.h>
#include <sirius/sirius_thread.h>
#include <sirius/sirius_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define t_assert(expr) \
  do { \
    if (unlikely(!(expr))) { \
      sirius_error("Assert: %s\n", #expr); \
      abort(); \
    } \
  } while (0)

#ifdef test_log_path_overload
static int gfd;
#endif

static inline void test_deinit() {
#ifdef test_log_path_overload
#  ifdef _WIN32
  _close(gfd);
#  else
  close(gfd);
#  endif

#endif
}

static inline void test_init() {
#ifdef test_log_path_overload
  sirius_log_config_t cfg = {0};

  gfd =
#  ifdef _WIN32
    _sopen(test_log_path, _O_RDWR | _O_CREAT | _O_APPEND, _SH_DENYNO,
           _S_IREAD | _S_IWRITE);
#  else
    open(test_log_path, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
#  endif
  if (-1 == gfd) {
    sirius_error("Failed to open the file: `%s`\n", test_log_path);
    t_assert(false);
  }

  cfg.fd_err = &gfd;
  cfg.fd_out = &gfd;
  sirius_log_config(cfg);

#endif
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

template<class T>
static inline int arr_length(T &arr) {
  return sizeof(arr) / sizeof(arr[0]);
}

namespace testcxx {

template<const char *test_module>
struct out_dir {
  static auto make() {
    return std::string(test_generated_pre) + test_module + []() {
      auto now = std::chrono::system_clock::now();
      auto zt = std::chrono::zoned_time(std::chrono::current_zone(), now);
      return std::format("{:%Y-%m-%d_%H-%M-%S}", zt);
    }();
  }
};

/**
 * @brief Recursively create a directory; if it already exists, clear its
 * content.
 *
 * @param[in] input_path The original path (which can be a relative or absolute
 * path).
 *
 * @return std::filesystem::path, the final directory path after creation or
 * clearing
 *
 * @throws std::runtime_error, thrown when the operation fails.
 */
std::filesystem::path recursive_mkdir(const std::string &dir) {
  try {
    if (std::filesystem::exists(dir)) {
      if (!std::filesystem::is_directory(dir)) {
        auto es =
          std::format("`{}` already exists but is not a directory", dir);
        throw std::runtime_error(es);
      }
      for (auto &entry : std::filesystem::directory_iterator(dir)) {
        std::filesystem::remove_all(entry.path());
      }
    } else {
      std::filesystem::create_directories(dir);
    }
    return dir;
  } catch (const std::filesystem::filesystem_error &e) {
    auto es = "File system error: " + std::string(e.what());
    throw std::runtime_error(es);
  }
}

class Init {
 public:
  Init() {
    test_init();
  }
  ~Init() {
    test_deinit();
  }

  /**
   * @note Recursively create a directory.
   */
  std::filesystem::path mkdir(const std::string &dir) {
    return recursive_mkdir(dir);
  }
};

} // namespace testcxx

#endif

#endif // POLLUX_TEST_H

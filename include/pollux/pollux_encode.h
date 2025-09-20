/**
 * @note Unless otherwise specified, encoding `API` are unsafe in
 * multi-threading.
 */

#ifndef POLLUX_ENCODE_H
#define POLLUX_ENCODE_H

#include <stdbool.h>

#include "pollux/codec/codec.h"
#include "pollux/pollux_codec_id.h"
#include "pollux/pollux_container_fmt.h"
#include "pollux/pollux_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Video encoding interface, which is based on ffmpeg.
 *
 * @details
 * flow:
 * (1) Call the `pollux_encode_init` function to get the encoder handle.
 * (2) Call the `param_set` function to set parameters.
 * (3) Call the `pollux_encode_priv_set` function to set private options of the
 * encoder. (Optional)
 * (4) Call the `start` function to start encoding.
 * (5) Call the `send_frame` function to send the frame into the encoder.
 * (6) Call the `stop` function to stop encoding.
 * (7) Call the `release` function to release the encoding resource.
 * (8) Call the `pollux_encode_deinit` function to release the encoder handle.
 */

typedef struct {
  /**
   * @brief Container format. When set to `pollux_cont_fmt_none` or the
   * configuration is invalid, use the default value.
   */
  pollux_cont_fmt_t cont_fmt;

  /**
   * @brief Bit rate.
   */
  int64_t bit_rate;

  /**
   * @brief Information of the encoded image settings. Among them, the `align`
   * value does not take effect here.
   */
  pollux_img_t img;

  /**
   * @brief Frame rate. In general, frame rate = num / den.
   */
  pollux_rational frame_rate;

  /**
   * @brief Gop size, keyframe interval.
   */
  int gop_size;

  /**
   * @brief Maximum number of B-frames between non-B-frames.
   */
  int max_b_frames;

  /**
   * @brief Thread count.
   */
  int thread_count;

  /**
   * @brief Encoder id.
   */
  pollux_codec_id_t codec_id;
} pollux_encode_args_t;

typedef struct pollux_encode_t {
  /**
   * @brief Private data.
   */
  void *priv_data;

  /**
   * @brief Release the resource of the encoder, this function must be used
   * before `pollux_encode_deinit`. It is repeatable and thread-safe; if not
   * called explicitly, it will also be called in the `pollux_encode_deinit`.
   *
   * @param[in] h Encoder handle.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*release)(struct pollux_encode_t *h);

  /**
   * @brief Set parameters to the encoder; this function will request some
   * resources, which must be released through the `release` function.
   *
   * @param[in] h Encoder handle.
   * @param[in] url The output path or network url (e.g., "./output.mp4" or
   * "tcp://192.168.1.100:1234").
   * @param[in] args Configuration.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*param_set)(struct pollux_encode_t *h, const char *url,
                   const pollux_encode_args_t *args);

  /**
   * @brief Start encoding, write the header of the stream.
   *
   * @param[in] h Encoder handle.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*start)(struct pollux_encode_t *h);

  /**
   * @brief Stop encoding, write the tail of the stream.
   *
   * @param[in] h Encoder handle.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*stop)(struct pollux_encode_t *h);

  /**
   * @brief Send the frame to the encoder.
   *
   * @param[in] h Encoder handle.
   * @param[in] frame Frame information.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*send_frame)(struct pollux_encode_t *h, const pollux_frame_t *frame);
} pollux_encode_t;

/**
 * @see pollux_encode_priv_set
 */
pollux_api int _pollux_encode_priv_set(pollux_encode_t *handle,
                                       pollux_codec_id_t codec_id,
                                       const void *args);

/**
 * @brief Deinit the encode module, this function will release all encoding
 * resources under the current handle.
 *
 * @param[in] handle Encoder handle.
 */
pollux_api void pollux_encode_deinit(pollux_encode_t *handle);

/**
 * @brief Init the encode module.
 *
 * @param[out] handle Encoder handle.
 *
 * @return 0 on success, error code otherwise.
 */
pollux_api int pollux_encode_init(pollux_encode_t **handle);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
static inline int _pollux_encode_priv_set_impl(pollux_encode_t *handle,
                                               hevc_encode_args_t *args) {
  return _pollux_encode_priv_set(handle, pollux_codec_id_hevc, args);
}
static inline int _pollux_encode_priv_set_impl(pollux_encode_t *handle,
                                               const hevc_encode_args_t *args) {
  return _pollux_encode_priv_set(handle, pollux_codec_id_hevc, args);
}
static inline int _pollux_encode_priv_set_impl(pollux_encode_t *handle,
                                               av1_encode_args_t *args) {
  return _pollux_encode_priv_set(handle, pollux_codec_id_av1, args);
}
static inline int _pollux_encode_priv_set_impl(pollux_encode_t *handle,
                                               const av1_encode_args_t *args) {
  return _pollux_encode_priv_set(handle, pollux_codec_id_av1, args);
}
#else
#  if defined(__GNUC__) || defined(__clang__)
#    define ENCODE_EXPECT_ID_FOR_ARGS(args) \
      _Generic((args), \
        hevc_encode_args_t *: pollux_codec_id_hevc, \
        const hevc_encode_args_t *: pollux_codec_id_hevc, \
        av1_encode_args_t *: pollux_codec_id_av1, \
        const av1_encode_args_t *: pollux_codec_id_av1, \
        default: pollux_codec_id_none)

#    define _pollux_encode_priv_set_impl(handle, args) \
      _pollux_encode_priv_set(handle, ENCODE_EXPECT_ID_FOR_ARGS(args), args)
#  elif defined(_MSC_VER)
#    error "For c files, please use gcc/clang, MSVC only supports cpp"
#  else
#    error "Unsupported compilation environment"
#  endif
#endif // __cplusplus

/**
 * @brief Encode private option settings.
 *
 * @param[in] h Encoder handle.
 * @param[in] args The encoder parameter options.
 *
 * @example
 * ```c++
 * #include <pollux/codec/hevc.h>
 *
 * hevc_encode_args_t args{};
 * // args = ...
 *
 * pollux_encode_priv_set(encoder, &args);
 * ```
 *
 * @return 0 on success, error code otherwise.
 */
#define pollux_encode_priv_set(handle, args) \
  (_pollux_encode_priv_set_impl(handle, args))

#endif // POLLUX_ENCODE_H

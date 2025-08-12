/**
 * @note Although some mutex lock processing has been done
 *  inside the function, it is not recommended to use
 *  decoding `API` simultaneously in multiple threads.
 */

#ifndef POLLUX_DECODE_H
#define POLLUX_DECODE_H

#include "pollux_codec_id.h"
#include "pollux_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Video decoding interface, which is based on
 *  ffmpeg.
 *
 * @details
 *  flow:
 *  (1) Call the `pollux_decode_init` function to get the
 *      decoder handle.
 *  (2) Call the `param_set` function to set parameters.
 *  (3) Call the `result_get` function to get the decoding
 *      result.
 *  (4) Call the `result_free` function to release the
 *      decoding result.
 *  (5) Call the `release` function to release the decoding
 *      resource.
 *  (6) Call the `pollux_decode_deinit` function to release
 *      the decoder handle.
 */

/**
 * @brief Decoding parameter.
 */
typedef struct {
  /**
   * @brief The maximum number of result caches, that is,
   *  the number of caches of the `pollux_frame_t` struct.
   *
   * @note When configuring this parameter, consider the
   *  actual hardware memory. The default value is 1.
   */
  unsigned short result_cache_nr;

  /**
   * @brief
   *  - (1) When this parameter is configured to `nullptr`,
   *  the decoded image will be output in the original
   *  format.
   *
   *  - (2) When this parameter is not configured to
   *  `nullptr`, the decoded image will be converted into
   *  the image format configured with this parameter and
   *  then output.
   *
   *  - (3) When the configuration of this parameter is
   *  invalid or 0, use the format of the source stream
   *  data instead of the invalid value.
   *
   * @note
   *  - (1) If this parameter is not configured to
   *  `nullptr`, the process of format conversion will
   *  consume some time.
   *
   *  - (2) If the image of the source video already
   *  conforms to the configured image parameters, the
   *  image will not be converted multiple times.
   */
  pollux_img_t *fmt_cvt_img;
} pollux_decode_args_t;

typedef struct {
  /* Video width and height. */
  int video_width, video_height;

  /* The average bitrate. */
  long long int bit_rate;

  /**
   * @brief Video frame rate. In general,
   *  frame rate = num / den
   */
  pollux_rational video_frame_rate;

  /**
   * @brief Maximum number of B-frames between
   *  non-B-frames.
   */
  int max_b_frames;

  /**
   * @brief The number of pictures in a group of pictures,
   *  or 0 for intra_only.
   */
  int gop_size;

  /* Video frame format. */
  pollux_pix_fmt_t video_img_fmt;

  /* Video encoding format. */
  pollux_codec_id_t video_codec_id;

  /**
   * @brief Stream duration, unit: us, 0 if unrecognized.
   */
  signed long long int duration;
} pollux_decode_stream_info_t;

/**
 * @brief Decode handle.
 */
typedef struct pollux_decode_t {
  /* Private data. */
  void *priv_data;

  /**
   * @brief Stream information, this member will be
   *  populated after the `param_set` function is called.
   */
  pollux_decode_stream_info_t stream;

  /**
   * @brief Release the resource of the decoder, this
   *  function must be used before `pollux_decode_deinit`.
   *  It is repeatable and thread-safe; if not called
   *  explicitly, it will also be called in the
   *  `pollux_decode_deinit`.
   *
   * @param[in] h: Decoder handle.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*release)(struct pollux_decode_t *h);

  /**
   * @brief Set parameters to the decoder; this function
   *  will request some resources, which must be released
   *  through the `release` function.
   *
   * @param[in] h: Decoder handle.
   * @param[in] url: Source stream url.
   * @param[in] args: Configuration. When this parameter is
   *  configured to `nullptr`, the default value is used
   *  for decoding.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*param_set)(struct pollux_decode_t *h,
                   const char *url,
                   const pollux_decode_args_t *args);

  /**
   * @brief Release the result cache, this function must
   *  be called to release the result cache after calling
   *  the `result_get` function.
   *
   * @param[in] h: Decoder handle.
   * @param[in] result: Decoding result.
   */
  int (*result_free)(struct pollux_decode_t *h,
                     pollux_frame_t *result);

  /**
   * @brief Get decodeing result, which needs to be
   *  released by calling the `result_free` function.
   *  However, when the function returns a failure value,
   *  the `result_free` function does not need to be
   *  called.
   *
   * @param[in] h: Decoder handle.
   * @param[out] result: Decoding result.
   * @param[in] milliseconds: Timeout duration, unit: ms.
   *  Setting the value to `0` means no wait, and setting
   *  it to `(~0U)` means infinite wait.
   *
   * @return
   *  - (1) 0 on success;
   *
   *  - (2) `pollux_err_url_read_end` indicates the file
   *  has been decoded to the end. At this point, the
   *  `seek_file` function can be called to reset the
   *  decoding timestamp or the `release` function can be
   *  called to release the related resource;
   *
   *  - (3) `pollux_err_not_init` indicates that the decode
   *  thread has not been started or has exited; calling
   *  this function again at this point makes no sense.
   *  To decode, the `param_set` function should be called
   *  first;
   *
   *  - (4) Error code otherwise, you may be able to
   *  continue calling this function to get results.
   */
  int (*result_get)(struct pollux_decode_t *h,
                    pollux_frame_t **result,
                    unsigned long milliseconds);

  /**
   * @brief Seek to timestamp ts.
   *
   * @param[in] h: Decoder handle.
   * @param[in] min_ts: Smallest acceptable timestamp.
   * @param[in] ts: Target timestamp.
   * @param[in] max_ts: Largest acceptable timestamp.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*seek_file)(struct pollux_decode_t *h,
                   long long int min_ts, long long int ts,
                   long long int max_ts);
} pollux_decode_t;

/**
 * @brief Deinit the decode module, this function will
 *  release all decoding resources under the current
 *  handle. It is repeatable but may be thread-unsafe, when
 *  calling this function, make sure that no `release`
 *  function or the function itself being called in another
 *  thread.
 *
 * @param[in] handle: Decoder handle.
 */
void pollux_decode_deinit(pollux_decode_t *handle);

/**
 * @brief Init the decode module.
 *
 * @param[out] handle: Decoder handle.
 *
 * @return 0 on success, error code otherwise.
 */
int pollux_decode_init(pollux_decode_t **handle);

#ifdef __cplusplus
}
#endif

#endif  // POLLUX_DECODE_H

/**
 * @note Unless otherwise specified, encoding `API` are
 *  unsafe in multi-threading.
 */

#ifndef POLLUX_ENCODE_H
#define POLLUX_ENCODE_H

#include "pollux_codec_id.h"
#include "pollux_container_fmt.h"
#include "pollux_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Video encoding interface, which is based on
 *  ffmpeg.
 *
 * @details
 *  flow:
 *  (1) Call the `pollux_encode_init` function to get the
 *      encoder handle.
 *  (2) Call the `param_set` function to set parameters.
 *  (3) Call the `start` function to start encoding.
 *  (4) Call the `send_frame` function to send the frame
 *      into the encoder.
 *  (5) Call the `stop` function to stop encoding.
 *  (6) Call the `release` function to release the encoding
 *      resource.
 *  (7) Call the `pollux_encode_deinit` function to release
 *      the encoder handle.
 */

typedef struct {
  /**
   * @brief Container format. When set to
   *  `pollux_cont_fmt_none` or the configuration is
   *  invalid, use the default value.
   */
  pollux_cont_fmt_t cont_fmt;

  /* Bit rate. */
  long int bit_rate;

  /**
   * Information of the encoded image settings. Among them,
   * the `align` value does not take effect here.
   */
  pollux_img_t img;

  /**
   * @brief Frame rate. In general,
   *  frame rate = num / den
   */
  pollux_rational frame_rate;

  /* Gop size, keyframe interval. */
  int gop_size;

  /* Maximum number of B-frames between non-B-frames. */
  int max_b_frames;

  /* Encoder id. */
  pollux_codec_id_t codec_id;

  /**
   * @brief The maximum allowable encoding frame cache when
   *  encoding at the rate of the target stream.
   *
   *  - (1) When this parameter is configured to `0`, it
   *  will encode as quickly as possible.
   *
   *  - (2) When this parameter is not configured to `0`,
   *  the encoder will try its best to encode at the
   *  configured rate of the target stream. Reference the
   *  parameter `frame_rate`.
   *
   * @note
   *  - (1) When this parameter is not configured to `0`,
   *  it can prevent the encoding speed from being too
   *  fast. Similar to the `-re` parameter in the ffmpeg
   *  command.
   *
   *  - (2) When this value configuration is greater than
   *  4096, take 4096.
   */
  int pkg_ahead_nr;
} pollux_encode_args_t;

typedef struct pollux_encode_t {
  /* Private data. */
  void *priv_data;

  /**
   * @brief Release the resource of the encoder, this
   *  function must be used before `pollux_encode_deinit`.
   *  It is repeatable and thread-safe; if not called
   *  explicitly, it will also be called in the
   *  `pollux_encode_deinit`.
   *
   * @param[in] h: Encoder handle.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*release)(struct pollux_encode_t *h);

  /**
   * @brief Set parameters to the encoder; this function
   *  will request some resources, which must be released
   *  through the `release` function.
   *
   * @param[in] h: Encoder handle.
   * @param[in] url: The output path or network url (e.g.,
   *  "./output.mp4" or "tcp://192.168.1.100:1234").
   * @param[in] args: Configuration.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*param_set)(struct pollux_encode_t *h,
                   const char *url,
                   const pollux_encode_args_t *args);

  /**
   * @brief Start encoding, write the header of the stream.
   *
   * @param[in] h: Encoder handle.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*start)(struct pollux_encode_t *h);

  /**
   * @brief Stop encoding, write the tail of the stream.
   *
   * @param[in] h: Encoder handle.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*stop)(struct pollux_encode_t *h);

  /**
   * @brief Send the frame to the encoder.
   *
   * @param[in] h: Encoder handle.
   * @param[in] frame: Frame information.
   *
   * @return 0 on success, error code otherwise.
   */
  int (*send_frame)(struct pollux_encode_t *h,
                    const pollux_frame_t *frame);
} pollux_encode_t;

/**
 * @brief Deinit the encode module, this function will
 *  release all encoding resources under the current
 *  handle.
 *
 * @param[in] handle: Encoder handle.
 */
void pollux_encode_deinit(pollux_encode_t *handle);

/**
 * @brief Init the encode module.
 *
 * @param[out] handle: Encoder handle.
 *
 * @return 0 on success, error code otherwise.
 */
int pollux_encode_init(pollux_encode_t **handle);

#ifdef __cplusplus
}
#endif

#endif  // POLLUX_ENCODE_H

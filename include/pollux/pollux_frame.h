#ifndef POLLUX_FRAME_H
#define POLLUX_FRAME_H

#include <stdint.h>

#include "pollux/pollux_attributes.h"
#include "pollux/pollux_pixel_fmt.h"
#include "pollux/pollux_rational.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  /**
   * @brief Width and height.
   */
  int width, height;

  /**
   * @brief The buffer size alignment for the image.
   */
  int align;

  /**
   * @brief Format.
   */
  pollux_pix_fmt_t fmt;
} pollux_img_t;

/**
 * @brief Frame information.
 */
typedef struct {
  /**
   * @brief Private data.
   */
  void *priv_data;

  /**
   * @brief Width and height.
   */
  int width, height;

  /**
   * @brief Format.
   *
   * @see pollux_pix_fmt_t
   */
  pollux_pix_fmt_t fmt;

  /**
   * @brief Presentation timestamp in time_base units (time when frame should be
   * shown to user).
   */
  int64_t pts;

  /**
   * @brief DTS copied from the AVPacket that triggered returning this frame.
   */
  int64_t pkt_dts;

  /**
   * @brief Time base for the timestamps in this frame.
   */
  pollux_rational time_base;

/**
 * @brief The maximum value of the planar component inherited from ffmpeg.
 *
 * @see AV_NUM_DATA_POINTERS
 */
#define POLLUX_DECODE_DATA_NR (8)

  /**
   * @brief Linesize.
   */
  int linesize[POLLUX_DECODE_DATA_NR];

  /**
   * @brief The data of the image.
   */
  unsigned char *data[POLLUX_DECODE_DATA_NR];
} pollux_frame_t;

/**
 * @brief Free the `pollux_frame_t` cache.
 */
pollux_api void pollux_frame_free(pollux_frame_t **res);

/**
 * @brief Allocate for `pollux_frame_t` cache, which needs to be released by
 * the `pollux_frame_free` function.
 *
 * @param[in] img Image information.
 *
 * - (1) When this parameter is set to `nullptr`, only memory is allocated for
 * the frame, not for the image data.
 *
 * - (2) When this parameter is not set to `nullptr`, memory will be allocated
 * for the frame and for the image data.
 *
 * @param[out] res: Pointer to `pollux_frame_t`.
 *
 * @return 0 on success, error code otherwise.
 */
pollux_api int pollux_frame_alloc(const pollux_img_t *img,
                                  pollux_frame_t **res);

#ifdef __cplusplus
}
#endif

#endif // POLLUX_FRAME_H

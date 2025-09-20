#ifndef POLLUX_INTERNAL_FRAME_H
#define POLLUX_INTERNAL_FRAME_H

#include <libavutil/frame.h>

/**
 * @brief When the `pollux_frame_alloc` function is called, memory of type
 * `frame_t` is allocated, which is stored in the `priv_data` pointer of the
 * `pollux_frame_t` struct.
 */
typedef struct {
  /**
   * @brief User's private data, the `pollux_frame_alloc` function will not
   * allocate memory for this pointer.
   */
  void *user_priv_data;

  /**
   * @brief Has memory been allocated for the image data.
   *
   * @note This parameter cannot be omitted. When the image memory is allocated
   * by other modules, the `pollux_frame_free` function should not release this
   * piece of image memory.
   */
  bool has_img_mem;

  /**
   * @brief Frame data of ffmpeg.
   */
  AVFrame *av_frame;
} frame_t;

#endif // POLLUX_INTERNAL_FRAME_H

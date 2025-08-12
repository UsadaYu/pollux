#ifndef POLLUX_SWSCALE_H
#define POLLUX_SWSCALE_H

#include "pollux_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *pollux_sws_handle;

/**
 * @brief Scale the image.
 *
 * @param[in] handle: The image scaling handle.
 * @param[in] src: The source image frame.
 * @param[out] src: The destination image frame.
 *
 * @note Before calling this function, memory needs to be
 *  allocated for the pointer `dst`.
 *
 * @return 0 on success, error code otherwise.
 */
int pollux_sws_scale(pollux_sws_handle handle,
                     pollux_frame_t *src,
                     pollux_frame_t *dst);

/**
 * @brief Allocate an image scaling handle.
 *
 * @param[in] src_width: The width of the source image.
 * @param[in] src_height: The height of the source image.
 * @param[in] src_fmt: The format of the source image.
 * @param[in] dst_width: The width of the destination
 *  image.
 * @param[in] dst_height: The height of the destination
 *  image.
 * @param[in] dst_fmt: The format of the destination image.
 *
 * @return A pointer to an allocated handle, or `nullptr`
 *  in case of error.
 */
pollux_sws_handle pollux_sws_context_alloc(
    int src_width, int src_height,
    pollux_pix_fmt_t src_fmt, int dst_width,
    int dst_height, pollux_pix_fmt_t dst_fmt);

/**
 * @brief Free the image scaling handle.
 */
void pollux_sws_context_free(pollux_sws_handle *handle);

#ifdef __cplusplus
}
#endif

#endif  // POLLUX_SWSCALE_H

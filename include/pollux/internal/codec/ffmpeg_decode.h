#ifndef POLLUX_INTERNAL_CODEC_FFMPEG_DECODE_H
#define POLLUX_INTERNAL_CODEC_FFMPEG_DECODE_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

/**
 * @brief Decoder configuration parameters.
 */
typedef struct {
  /**
   * @brief Number of threads for decoding. 0 for auto.
   */
  int thread_count;
} ffmpeg_decode_args_t;

typedef struct {
  AVFormatContext *fmt_ctx;
  AVCodecContext *codec_ctx;

  int stream_index;

  AVFrame *frame;
  AVPacket *pkt;
} ffmpeg_decode_t;

/**
 * @brief Creates and initializes a decoder context by opening an input URL.
 * This function handles `avformat_open_input` and `avformat_find_stream_info`.
 *
 * @param[in] url The input media URL (file path, rtsp, etc.).
 *
 * @return A pointer to the `ffmpeg_decode_t` context on success, nullptr on
 * failure.
 */
ffmpeg_decode_t *ffmpeg_decoder_create(const char *url);

/**
 * @brief Destroys the decoder context and frees all associated resources.
 *
 * @param[in] d_ptr A pointer to the decoder context pointer. It will be set to
 * nullptr after destruction.
 */
void ffmpeg_decoder_destroy(ffmpeg_decode_t **d_ptr);

/**
 * @brief Finds a specific media stream, configures and opens the corresponding
 * decoder.
 *
 * @param[in] d The decoder context created by ffmpeg_decoder_create.
 * @param[in] media_type The type of media to find (e.g., AVMEDIA_TYPE_VIDEO).
 * @param[in] args Decoder configuration parameters. Can be nullptr for default
 * settings.
 *
 * @return 0 on success, a negative error code on failure.
 */
int ffmpeg_decoder_open_stream(ffmpeg_decode_t *d, enum AVMediaType media_type,
                               const ffmpeg_decode_args_t *args);

/**
 * @brief Allocates reusable resources (AVPacket and AVFrame) for the decoding
 * loop.
 *
 * @param[in] d The decoder context.
 *
 * @return 0 on success, a negative error code on failure.
 */
int ffmpeg_decoder_alloc_buffers(ffmpeg_decode_t *d);

/**
 * @brief Frees the resources allocated by `ffmpeg_decoder_alloc_buffers`.
 *
 * @param[in] d The decoder context.
 */
void ffmpeg_decoder_free_buffers(ffmpeg_decode_t *d);

#endif // POLLUX_INTERNAL_CODEC_FFMPEG_DECODE_H

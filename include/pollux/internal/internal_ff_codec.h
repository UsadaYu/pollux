#ifndef INTERNAL_FF_CODEC_H
#define INTERNAL_FF_CODEC_H

#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <sirius/sirius_attributes.h>
#include <sirius/sirius_log.h>

#include "internal_sys.h"
#include "pollux_erron.h"

/**
 * @brief Find stream index based on `media_type`.
 *
 * @param[in] fmt_ctx: Format context.
 * @param[in] media_type: Media type, refer `AVMediaType`.
 * @param[out] stream: Stream index.
 *
 * @return `true` on success, `false` otherwise.
 */
static force_inline bool internal_ff_codec_stream_get(
    const AVFormatContext *fmt_ctx,
    enum AVMediaType media_type, unsigned int *stream) {
  *stream = -1u;
  if (unlikely(!fmt_ctx)) return false;

  for (int i = 0; i < fmt_ctx->nb_streams; i++) {
    if (media_type ==
        fmt_ctx->streams[i]->codecpar->codec_type) {
      *stream = i;
      break;
    }
  }
  if (*stream == -1u) {
    sirius_warn("media type [%d] not found\n", media_type);
    return false;
  }

  return true;
}

#endif  // INTERNAL_FF_CODEC_H
